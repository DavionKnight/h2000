/*
 * Copyright (C) 2007-2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Author: Roy Pledge, Roy.Pledge@freescale.com
 *
 * Description:
 * This file provides the pme_database device interface to the pattern-matcher
 * driver. It is used for creating special I/O contexts to allow administration
 * of the pattern matcher database.
 */

#include "user_private.h"

struct database_op_result {
	struct list_head list;
	struct pme_fbchain *chain;
	u32 result;
};

/* Private management structure for pme_database devices */
struct database_data {
	/* The context used for this database channel */
	struct pme_context *context;
	/* The data waiting to be read */
	struct list_head pending_data;
	/* The offset into the current chain entry */
	size_t offset;
	/* pending data mutex, used to protect the pending_data list*/
	spinlock_t spinlock;
	/* Wait queue */
	wait_queue_head_t queue;
	/* Flags */
	u32 flags;
	/* Freelist that will be used for output */
	struct pme_data fb_mem;
	/* Number of commands in flight */
	atomic_t cmds_inflight;
	/* List of pre allocated op_result structs */
	struct list_head result_cache;
	/* Locks the result_cache */
	spinlock_t result_cache_lock;
	u32 freelists;
};

/* Flag set if the channel is no longer usable.
 * Once an instance of pme_database is dead, the fd must be closed
 * This flag is set so any callers to read won't block waiting for data that
 * can never come */
#define PME_DATABASE_FLAG_DEAD 	0x1

/* Callback Pend Structure :
 * These are allocated to keep track of the data needed
 * when the AIO write methods are invoked. */
struct database_cb_context {
	wait_queue_head_t wait_queue;
	int done;
	int result;
	struct kiocb *iocb;
	struct pme_mem_mapping input_data;
	struct database_data *database_data;
};

static inline int database_data_empty(struct database_data *database_data)
{
	int result = 0;
	spin_lock_bh(&database_data->spinlock);
	if (!list_empty(&database_data->pending_data))
		result = 1;
	spin_unlock_bh(&database_data->spinlock);
	return result;
}

static inline struct database_op_result *database_data_head(struct database_data
							   *database_data)
{
	return list_entry(database_data->pending_data.next,
			  struct database_op_result, list);
}

static int database_data_add(struct database_data *database_data)
{
	struct database_op_result *res = kmalloc(sizeof(*res), GFP_KERNEL);
	if (!res)
		return -ENOMEM;
	spin_lock_bh(&database_data->result_cache_lock);
	list_add_tail(&res->list, &database_data->result_cache);
	spin_unlock_bh(&database_data->result_cache_lock);
	return 0;
}

static inline struct database_op_result *database_data_pop(
		struct database_data *database_data)
{
	struct database_op_result *res;
	spin_lock_bh(&database_data->result_cache_lock);
	res = list_entry(database_data->result_cache.next,
			struct database_op_result, list);
	list_del(database_data->result_cache.next);
	spin_unlock_bh(&database_data->result_cache_lock);
	return res;
}

static inline void database_data_reclaim(struct database_op_result *val)
{
	kfree(val);
}

static int database_fops_open(struct inode *node, struct file *fp)
{
	/* private_data, as of v2.6.35 is set with the miscdevice ptr, with the
	 * assumption that it would be useful.  We don't want it, so set it to
	 * NULL */
	fp->private_data = NULL;
	/* Nothing is allocated until channel is set */
	return 0;
}

static void database_dtor(void *ctx, enum pme_dtor reason)
{
	struct database_data *database_data  = ctx;
	struct database_op_result *curr, *next;

	/* Channel error occurred, this replaces the previous usage of direct
	 * registration with the channel. */
	if (unlikely(reason == pme_dtor_channel)) {
		database_data->flags |= PME_DATABASE_FLAG_DEAD;
		wake_up(&database_data->queue);
		return;
	}
	/* Cleanup any data that was never read */
	list_for_each_entry_safe(curr, next, &database_data->pending_data,
					list) {
		if (curr->chain)
			pme_fbchain_recycle(curr->chain);
		database_data_reclaim(curr);
	}

	kfree(database_data);
}

static int database_fops_release(struct inode *node, struct file *fp)
{
	struct database_data *database_data;
	if (fp->private_data) {
		database_data = fp->private_data;
		pme_context_delete(database_data->context);
	}
	return 0;
}

static int database_set_channel(struct file *fp, int channel_fd)
{
	u32 flags;
	int ret;
	struct file *chan_file;
	struct database_data *database_data;
	struct pme_channel *channel;
	struct pme_parameters params = {
		.mode = PME_MODE_CONTROL
	};
	u64 stream_id = 0;
	/* If a context has been assigned, changing the
	 * channel is not allowed */
	if (fp->private_data)
		return -EINVAL;

	/* Get the channel from the file descriptor */
	chan_file = fget(channel_fd);
	if (!chan_file)
		return -EBADFD;
	channel = pme_device_get_channel(chan_file);
	fput(chan_file);
	if (!channel)
		return -EBADFD;

	/* Make sure a freebuffer list is assigned to this channel
	 * We need a freebuffer list to operate */
	flags = pme_channel_freelists(channel);
	if (!flags) {
		printk(KERN_INFO PMMOD "Freelist not enabled for channel, "
				"flags %x\n", flags);
		pme_channel_put(channel);
		return -EINVAL;
	}
	/* Allocate the structure */
	database_data =	kmalloc(sizeof(*database_data), GFP_KERNEL);
	if (!database_data)
		return -ENOMEM;
	fp->private_data = database_data;
	INIT_LIST_HEAD(&database_data->pending_data);
	database_data->offset = 0;
	spin_lock_init(&database_data->spinlock);
	init_waitqueue_head(&database_data->queue);
	database_data->flags = 0;
	database_data->context = NULL;
	atomic_set(&database_data->cmds_inflight, 0);
	spin_lock_init(&database_data->result_cache_lock);
	INIT_LIST_HEAD(&database_data->result_cache);
	database_data->freelists = flags;

	database_data->fb_mem.type = data_out_fl;
	/* Assign the freelist value.  Using A over B for no real reason */
	if (flags & PME_DMA_CHANNEL_FL_A_ENABLED)
		database_data->fb_mem.freelist_id = 0;
	else
		database_data->fb_mem.freelist_id = 1;

	/* Allocate a context for database and store the address of
	 * database_data as the report stream ID */
	stream_id = ptr_to_u64(database_data);
	ret = pme_context_create(&database_data->context, channel,
				  &params, 0, stream_id, GFP_KERNEL,
				  database_data, database_dtor);
	pme_channel_put(channel);
	if (ret) {
		kfree(database_data);
		fp->private_data = NULL;
	}
	return ret;
}

enum database_common_read_e {
	contig,
	iovec
};

static ssize_t database_common_read(struct file *fp, void *user_data,
				 size_t len, loff_t *offset,
				 enum database_common_read_e type)
{
	struct database_op_result *op_result;
	size_t result = 0;
	struct database_data *database_data;

	if (!fp->private_data)
		return -EINVAL;

	database_data = fp->private_data;

	if (database_data->flags & PME_DATABASE_FLAG_DEAD)
		return -ENODEV;
	if (offset && *offset)
		return -EINVAL;

	/* Check and see if there is any pending data */
	if (!database_data_empty(database_data)) {
		/* No data */
		if (fp->f_flags & O_NONBLOCK) {
			/* If we are non blocking and no commands are being
			 * processed return 0.  This allows callers to
			 * determine when all data has been consumed */
			if (atomic_read(&database_data->cmds_inflight))
				return -EAGAIN;
			return 0;
		}
		/* We can block, so we pend on the queue */
		if (wait_event_interruptible(database_data->queue,
			     database_data_empty(database_data) ||
			     (database_data->flags & PME_DATABASE_FLAG_DEAD)))
			return -ERESTARTSYS;
	}
	/* Check if the channel died while we were waiting */
	if (database_data->flags & PME_DATABASE_FLAG_DEAD)
		return -ENODEV;
	/* Data is present on the queue */
	spin_lock_bh(&database_data->spinlock);

	op_result = database_data_head(database_data);
	if (op_result->chain) {
		if (type == contig)
			result = pme_mem_fb_copy_to_user(user_data, len,
							op_result->chain,
							&database_data->offset);
		else
			result = pme_mem_fb_copy_to_user_iovec(user_data, len,
							op_result->chain,
							&database_data->offset);

		/* If the fb has been totally consumed, remove it
		 * from the list and recycle it */
		if (pme_fbchain_current(op_result->chain)) {
			/* Still have data remaining */
			pme_fbchain_crop_recycle(op_result->chain);
		} else {
			/* recycle the whole thing */
			pme_fbchain_recycle(op_result->chain);
			if (!op_result->result) {
				/* If the result code is set, keep the buffer
				 * so it gets returned on the next read (this
				 * is for the truncation case) */
				list_del(database_data->pending_data.next);
				database_data_reclaim(op_result);
			} else
				op_result->chain = NULL;
			database_data->offset = 0;
		}
	} else {
		/* There is no chain attached to this result.  This
		 * indicated an error occured.  Returen the result
		 * code to the caller and cleanup */
		result = op_result->result;
		list_del(database_data->pending_data.next);
		database_data_reclaim(op_result);
	}
	spin_unlock_bh(&database_data->spinlock);
	return result;
}

static ssize_t database_fops_read(struct file *fp, char __user *data,
				 size_t len, loff_t *offset)
{
	return database_common_read(fp, data, len, offset, contig);
}

static ssize_t database_fops_aio_read(struct kiocb *iocb,
				  const struct iovec __user *user_vec,
				  unsigned long len, loff_t offset)
{
	struct file *fp = iocb->ki_filp;
	return database_common_read(fp, (void *)user_vec, len, &offset, iovec);
}

/* Callback that is executed when a database command is completed */
static int database_io_cb(struct pme_context *context,
				u32 pme_completion_flags,
				u8 exception_code,
				u64 stream_id,
				struct pme_context_callback *cb,
				size_t output_used,
				struct pme_fbchain *fb_output)
{
	struct database_cb_context *cb_ctx = NULL;
	struct database_data *database_data = NULL;
	struct database_op_result *op_res;

	if (!(pme_completion_flags & (PME_COMPLETION_COMMAND |
		 PME_COMPLETION_ABORTED)))
		goto got_output;
	/* This callback indicates that a command has either
	 * been consumed or aborted */
	cb_ctx = (struct database_cb_context *) cb->ctx.words[0];

	if (pme_completion_flags & PME_COMPLETION_ABORTED) {
		cb_ctx->result = -EIO;
		database_data_reclaim(database_data_pop(cb_ctx->database_data));
	} else
		cb_ctx->result = 0;
	cb_ctx->done = 1;
	if (cb_ctx->iocb) {
		pme_mem_unmap(&cb_ctx->input_data);
		aio_complete(cb_ctx->iocb, 0, 0);
		kfree(cb_ctx);
	} else
		wake_up(&cb_ctx->wait_queue);
	return 0;

got_output:
	/* This is the pattern matcher stage callback.  Any data that
	 * is present is the result of PMI read commands and will be
	 * queued in the output data stream */
	database_data = u64_to_ptr(stream_id);

	op_res = database_data_pop(database_data);
	if (exception_code)
		printk(KERN_INFO PMMOD "pme_database exception detected, "
			"code 0x%x\n", exception_code);

	if (output_used) {
		/* In the case of a data report, the cb->ctx pointer may not
		 * be valid, and therefore, shouldn't be used.  The database
		 * data is all we need, and it is the same as the report
		 * stream ID.  Store the output on the pending list */
		op_res->chain = fb_output;
		fb_output = NULL;
		if (pme_completion_flags & PME_COMPLETION_TRUNC)
			op_res->result = -ENOMEM;
		else if (exception_code)
			op_res->result = -EIO;
		else
			op_res->result = 0;
		spin_lock_bh(&database_data->spinlock);
		list_add_tail(&op_res->list, &database_data->pending_data);
		spin_unlock_bh(&database_data->spinlock);
	} else if (exception_code) {
		/* An error occured and no data was recovered
		 * Add the op result to the list so the error
		 * will be indicated in the read sys call */
		op_res->chain = NULL;
		op_res->result = -EIO;
		spin_lock_bh(&database_data->spinlock);
		list_add_tail(&op_res->list, &database_data->pending_data);
		spin_unlock_bh(&database_data->spinlock);
	} else
		/* No output needs to be saved.  Free the result
		 * structure we has pre allocated */
		database_data_reclaim(op_res);

	atomic_dec(&database_data->cmds_inflight);
	/* Wake up anyway */
	wake_up(&database_data->queue);

	if (fb_output)
		pme_fbchain_recycle(fb_output);
	return 0;
}

/* Write only needs to wait for the data to be consumed */
static ssize_t database_dma_data_write(struct file *fp,
				      struct pme_mem_mapping *dma_data,
				      struct kiocb *io_cb)
{
	struct database_data *database_data;
	int ret;
	u32 flags = PME_FLAG_CB_COMMAND;
	struct pme_context_callback callback;
	struct database_cb_context local_cb_ctx;
	struct database_cb_context *cb_ctx_p = NULL;

	if (!fp->private_data)
		return -EINVAL;
	database_data = fp->private_data;

	if (database_data->flags & PME_DATABASE_FLAG_DEAD)
		return -ENODEV;

	/* Setup the callback structure */
	if (io_cb) {
		cb_ctx_p = kmalloc(sizeof(*cb_ctx_p), GFP_KERNEL);
		if (!cb_ctx_p)
			return -ENOMEM;
		memcpy(&cb_ctx_p->input_data, dma_data, sizeof(*dma_data));
	} else {
		cb_ctx_p = &local_cb_ctx;
		init_waitqueue_head(&cb_ctx_p->wait_queue);
	}
	cb_ctx_p->result = 0;
	cb_ctx_p->done = 0;
	cb_ctx_p->iocb = io_cb;
	cb_ctx_p->database_data = database_data;

	callback.completion = database_io_cb;
	callback.ctx.words[0] = (u32) cb_ctx_p;

	/* Allocate a result */
	ret = database_data_add(database_data);
	if (ret) {
		if (io_cb)
			kfree(cb_ctx_p);
		return ret;
	}
	atomic_inc(&database_data->cmds_inflight);
	/* Submit to the context */
	ret = pme_context_io_cmd(database_data->context, flags,
				     &callback, &dma_data->data, NULL,
				     &database_data->fb_mem);
	if (ret) {
		atomic_dec(&database_data->cmds_inflight);
		database_data_reclaim(database_data_pop(database_data));
		if (io_cb)
			kfree(cb_ctx_p);
		return ret;
	}

	if (!io_cb) {
		/* Wait for the consumption of the data */
		wait_event(cb_ctx_p->wait_queue, cb_ctx_p->done != 0);
		return cb_ctx_p->result;
	}
	return 0;
}

static ssize_t database_fops_write(struct file *fp,
				  const char __user *user_data,
				  size_t user_data_len, loff_t *offset)
{
	struct pme_mem_mapping write_data;
	int ret;

	if (offset && *offset)
		return -EINVAL;

	/* Build an appropriate structure for passing the input
	 * to the device */
	ret = pme_mem_map((unsigned long)user_data, user_data_len, &write_data,
				DMA_TO_DEVICE);
	if (ret)
		return ret;

	ret = database_dma_data_write(fp, &write_data, NULL);

	pme_mem_unmap(&write_data);

	if (ret)
		return ret;
	return user_data_len;
}

static unsigned int database_fops_poll(struct file *fp,
				      struct poll_table_struct *wait)
{
	struct database_data *database_data;
	unsigned int mask = POLLOUT | POLLWRNORM;

	if (!fp->private_data)
		return -EINVAL;

	database_data = fp->private_data;

	poll_wait(fp, &database_data->queue, wait);
	/* Wake up if there is data present OR there are no more commands
	 * in flight and this is a non blocking interface */
	if (database_data->flags & PME_DATABASE_FLAG_DEAD)
		return POLLERR;

	if (database_data_empty(database_data) || ((fp->f_flags & O_NONBLOCK) &&
		!atomic_read(&database_data->cmds_inflight)))
		mask |= (POLLIN | POLLRDNORM);
	return mask;
}

static int database_begin_exclusive(struct file *fp)
{
	struct database_data *database_data;
	if (!fp->private_data)
		return -EINVAL;
	database_data = fp->private_data;
	return pme_context_set_exclusive_mode(database_data->context, 1);
}

static int database_end_exclusive(struct file *fp)
{
	struct database_data *database_data;
	if (!fp->private_data)
		return -EINVAL;
	database_data = fp->private_data;
	return pme_context_set_exclusive_mode(database_data->context, 0);
}

static int database_flush(struct file *fp)
{
	struct database_data *database_data;
	if (!fp->private_data)
		return -EINVAL;
	database_data = fp->private_data;
	return pme_context_flush(database_data->context);
}

static ssize_t database_fops_aio_write(struct kiocb *iocb,
					const struct iovec __user *user_vec,
					unsigned long len,
					loff_t offset)
{
	struct pme_mem_mapping write_data;
	int ret;
	ssize_t write_len;

	if (offset)
		return -EINVAL;
	/* Build an appropriate structure for passing the input
	 * to the device */
	ret = pme_mem_map_vector(user_vec, len, &write_data,
					  DMA_TO_DEVICE);
	if (ret)
		return ret;
	if (is_sync_kiocb(iocb)) {
		ret = database_dma_data_write(iocb->ki_filp, &write_data, NULL);
		write_len = write_data.data.length;
		pme_mem_unmap(&write_data);
		if (ret)
			return ret;
		return write_len;
	}
	ret = database_dma_data_write(iocb->ki_filp, &write_data, iocb);
	if (ret) {
		pme_mem_unmap(&write_data);
		return ret;
	}
	return -EIOCBQUEUED;
}

/* No seek support on database device */
static loff_t database_fops_llseek(struct file *file , loff_t loff, int whence)
{
	return -ESPIPE;
}

static int database_set_freelist(struct file *file,
			enum pme_database_freelist val)
{
	struct database_data *database_data = file->private_data;

	if (!database_data)
		return -EINVAL;
	switch (val) {
	case freelist_A:
		if (!(database_data->freelists & PME_DMA_CHANNEL_FL_A_ENABLED))
			return -EINVAL;
		break;
	case freelist_B:
		if (!(database_data->freelists & PME_DMA_CHANNEL_FL_B_ENABLED))
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}
	database_data->fb_mem.freelist_id = val;
	return 0;
}

static int database_fops_ioctl(struct inode *node, struct file *fp,
			      unsigned int opt, unsigned long val)
{
	int fd;
	enum pme_database_freelist fl_id;

	switch (opt) {
	case PME_IOCTL_BEGIN_EXCLUSIVE:
		return database_begin_exclusive(fp);
		break;
	case PME_IOCTL_END_EXCLUSIVE:
		return database_end_exclusive(fp);
		break;
	case PME_IOCTL_SET_CHANNEL:
		if (copy_from_user(&fd, (void *)val, sizeof(fd)))
			return -EFAULT;
		return database_set_channel(fp, fd);
		break;
	case PME_IOCTL_FLUSH:
		return database_flush(fp);
		break;
	case PME_DATABASE_IOCTL_SET_FREELIST:
		if (copy_from_user(&fl_id, (void *)val, sizeof(fl_id)))
			return -EFAULT;
		return database_set_freelist(fp, fl_id);
		break;
	default:
		return -EINVAL;
	}
}

/* Character Device management structures */
static struct file_operations database_fops = {
	.owner = THIS_MODULE,
	.ioctl = database_fops_ioctl,
	.open = database_fops_open,
	.release = database_fops_release,
	.read = database_fops_read,
	.aio_read = database_fops_aio_read,
	.write = database_fops_write,
	.poll = database_fops_poll,
	.aio_write = database_fops_aio_write,
	.llseek = database_fops_llseek
};

static struct miscdevice database_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = PME_DEVICE_DATABASE_NODE,
	.fops = &database_fops
};

int database_init(void)
{
	int err = misc_register(&database_dev);
	if (err) {
		printk(KERN_ERR PMMOD "registration of device %s failed\n",
					database_dev.name);
		return err;
	}
	printk(KERN_INFO PMMOD "device %s registered\n", database_dev.name);
	return 0;
}

void database_finish(void)
{
	int err = misc_deregister(&database_dev);
	if (err)
		printk(KERN_ERR PMMOD "Failed to deregister device %s, "
				"code %d\n", database_dev.name, err);
	else
		printk(KERN_INFO PMMOD "device %s deregistered\n",
				database_dev.name);
}

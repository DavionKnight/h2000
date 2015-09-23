/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
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
 * This file implements the pme_scanner user-interface to the pattern-matcher
 * driver. It is the mechanism used from user-space to perform decompression
 * and pattern-matching I/O in blocking and/or non-blocking modes using
 * zero-copy with user-provided descriptors and/or freelist output.
 */

#include "user_private.h"

/* The pme_scanner module implements a ioctl() based, zero copy
 * mechanism for sending scan and deflate operations to the
 * device.
 *
 * There are 2 methods for sending to the device, synchronous and
 * asynchronous.
 *
 * In the sychronous method, the ioctl() blocks until the entire
 * operation has completed and the output buffer(s) are avaliable
 * to be read by the calling application.
 *
 * In the asynchronous method, the user begins a command with one
 * ioctl() and finds out about its completion be using another ioctl().
 * The application is responsible for ensuring that any buffers remain
 * valid for the duration of the operation.
 *
 * Input buffers are always supplied by the user and are never copied.
 *
 * Ouput buffers can be specified by the user or they can be obtained
 * from the freebuffer management pool.
 *
 * If freebuffers are used, they are automatically mapped into the calling
 * application's virtual memory space when the command is completed.  The
 * buffers can then be accessed as any other buffer until the user either
 * releases the buffer with the free memory ioctl() or the scanner file
 * handle is closed (possibily due to the owning process exiting) */

/* Private structure that is allocated for each open that is done
 * on the pme_scanner device.  This is used to maintain the state
 * of a scanner instance */
struct scanner_object {
	/* The context that is needed to
	 * communicate with the pattern matching hardware */
	struct pme_context *context;

	/* The channel id this scanner is using */
	struct pme_channel *channel_id;

	/* For asynchronous processing */
	wait_queue_head_t waiting_for_completion;
	struct list_head completed_commands;
	/* Locks completed_commands */
	spinlock_t completed_commands_lock;
	atomic_t completed_count;
	int all_done;
	wait_queue_head_t done_queue;
	struct semaphore sem;
};

#define is_report_stream(stream_id) (stream_id == PME_SCANNER_REPORT_IDX)
#define is_deflate_stream(stream_id) (stream_id == PME_SCANNER_DEFLATE_IDX)

/* Flags to keep track of which callbacks have been
 * invoked */
#define __DEFLATE_COMPLETE	0x1
#define __REPORT_COMPLETE	0x2

/* When all these flags are done, the command is done */
#define __OPERATION_DONE (__DEFLATE_COMPLETE | __REPORT_COMPLETE)

/* Local helper to validate that the structure is good,
 * isn't a freelist before releasing it */
static inline void check_and_unmap_mem(struct pme_mem_mapping *mem)
{
	if (mem && mem->data.addr && (mem->data.type != data_out_sg_fl))
		pme_mem_unmap(mem);
}

/* Management Token for PM Scanner operations
 * One of these is created for every operation on
 * a context.  When the context operation is complete
 * cleanup is done */
struct scanner_token {
	/* Scanner object that owns this request */
	struct scanner_object *owner;

	/* Set to non zero if this is a synchronous request */
	u8 synchronous;

	/* Set to non zero when the context is desroyed */
	u8 done;

	/* The kernels copy of the user op structure */
	struct pme_scanner_operation kernel_op;

	/* Data management */
	struct pme_mem_mapping input;

	struct token_output {
		struct pme_mem_mapping dma_data;
		u8 truncated;
		u8 exception_code;
	} results[PME_SCANNER_NUM_RESULTS];

	/* Flags collected to describe the result */
	u32 result_flags;

	/* For blocking requests, we need a wait
	 * point and condition */
	wait_queue_head_t *queue;

	/* List management for completed async requests */
	struct list_head completed_list;
};

/* open hook: Allocates memory to manage the stream */
static int scanner_fops_open(struct inode *node, struct file *fp)
{
	struct scanner_object *scanner;

	fp->private_data = kmalloc(sizeof(*scanner), GFP_KERNEL);
	if (!fp->private_data)
		return -ENOMEM;
	scanner = fp->private_data;
	scanner->context = NULL;
	scanner->channel_id = NULL;
	scanner->all_done = 0;
	/* Set up the structures used for asynchronous requests */
	init_waitqueue_head(&scanner->waiting_for_completion);
	init_waitqueue_head(&scanner->done_queue);
	spin_lock_init(&scanner->completed_commands_lock);
	INIT_LIST_HEAD(&scanner->completed_commands);
	atomic_set(&scanner->completed_count, 0);
	sema_init(&scanner->sem, 1);
	return 0;
}

/* Seek not supported on the scanner device */
static loff_t scanner_fops_llseek(struct file *file , loff_t loff, int whence)
{
	return -ESPIPE;
}

/* Callback invoked when the pme_context is destroyed */
static void scanner_dtor_callback(void *ctx, enum pme_dtor reason)
{
	struct scanner_object *scanner = ctx;
	/* This used to occur via direct registration with the channel, but
	 * that comes through this callback. Curiously, channel-errors and
	 * object-destruction do exactly the same thing, so we don't even
	 * bother checking 'reason'. */
	scanner->all_done = 1;
	wake_up(&scanner->done_queue);
}

/* Release the token only if it is async (and therefore allocated
 * from a kmem_cache */
static inline void free_async_token(struct scanner_token *token_p)
{
	if (!token_p->synchronous)
		kfree(token_p);
}

/* Cleanup for the execute_cmd method */
static inline void cleanup_token(struct scanner_token *token_p)
{
	int i;
	check_and_unmap_mem(&token_p->input);
	for (i = 0; i < PME_SCANNER_NUM_RESULTS; i++)
		check_and_unmap_mem(&token_p->results[i].dma_data);
	free_async_token(token_p);
}

/* Shutdown mechanism.  Cleanup any outstanding requests */
static int scanner_fops_release(struct inode *node, struct file *fp)
{
	struct scanner_object *scanner = fp->private_data;
	struct scanner_token *token, *next_token;
	int i;
	if (scanner->context) {
		pme_context_delete(scanner->context);
		wait_event(scanner->done_queue, scanner->all_done);
	}
	/* Free all the completed commands that were not consumed */
	list_for_each_entry_safe(token, next_token,
				 &scanner->completed_commands, completed_list) {
	for (i = 0; i < PME_SCANNER_NUM_RESULTS; i++)
		if (token->results[i].dma_data.data.chain)
			pme_fbchain_recycle(
				token->results[i].dma_data.data.chain);
		cleanup_token(token);
	}

	if (scanner->channel_id)
		pme_channel_put(scanner->channel_id);

	kfree(scanner);
	return 0;
}

/* ioctl() implementations */
static int ioctl_get(struct scanner_object *scanner,
		struct pme_parameters *params)
{
	struct pme_parameters parameters;
	int ret = 0;

	/* Copy in the user structure so we respect the specified flags */
	if (copy_from_user(&parameters, params, sizeof(parameters)))
		return -EFAULT;

	down(&scanner->sem);
	if (!scanner->context) {
		/* The context isn't allocated until the channel is set */
		ret = -ENODEV;
		goto done;
	}
	ret = pme_context_get(scanner->context, &parameters);
	if (ret)
		goto done;
	if (copy_to_user(params, &parameters, sizeof(parameters)))
		ret = -EFAULT;
done:
	up(&scanner->sem);
	return ret;
}

/* Modify the parameters of the scanner */
static int ioctl_set(struct scanner_object *scanner,
		struct pme_parameters *params)
{
	struct pme_parameters parameters;
	int ret = 0;

	if (copy_from_user(&parameters, params, sizeof(parameters)))
		return -EFAULT;
	/* Error out if the flags we don't allow used from user space are set*/
	if (parameters.flags & PME_PARAM_RESERVED)
		return -EINVAL;
	/* Don't allow this device to be a conduit for control operations */
	if (parameters.mode == PME_MODE_CONTROL)
		return -EINVAL;

	down(&scanner->sem);
	if (scanner->channel_id == NULL) {
		/* Need to set channel first */
		ret = -EINVAL;
		goto done;
	}
	if (!scanner->context) {
		/* Returning here as all the work will either be
		 * complete or failed */
		ret = pme_context_create(&scanner->context,
				scanner->channel_id,
				&parameters,
				PME_SCANNER_DEFLATE_IDX,
				PME_SCANNER_REPORT_IDX,
				GFP_KERNEL, scanner,
				scanner_dtor_callback);
		goto done;
	}
	ret = pme_context_update(scanner->context, &parameters);
done:
	up(&scanner->sem);
	return ret;
}

static int reset_seqnum(struct scanner_object *scanner)
{
	struct pme_parameters params = {
		.flags = PME_PARAM_RESET_SEQ_NUMBER
	};
	return pme_context_update(scanner->context, &params);
}

static int reset_residue(struct scanner_object *scanner)
{
	struct pme_parameters params = {
		.flags = PME_PARAM_RESET_RESIDUE
	};
	return pme_context_update(scanner->context, &params);
}

static int begin_exclusive(struct scanner_object *scanner)
{
	int val = (pme_context_set_exclusive_mode(scanner->context, 1));
	return val;
}

static int end_exclusive(struct scanner_object *scanner)
{
	int val = (pme_context_set_exclusive_mode(scanner->context, 0));
	return val;
}

/* Callback for scanner operations */
static int scanner_op_cb(struct pme_context *context, u32 flags,
			    u8 exception_code, u64 stream_id,
			    struct pme_context_callback *cb,
			    size_t output_used,
			    struct pme_fbchain *fb_output)
{
	struct scanner_token *token;
	struct token_output *result = NULL;
	token = (struct scanner_token *) cb->ctx.words[0];
	if (unlikely(flags & PME_COMPLETION_ABORTED)) {
		/* Only one callback on an abort */
		token->result_flags |= PME_SCANNER_RESULT_ABORTED;
		token->done = __OPERATION_DONE;
	} else {
		result = &token->results[stream_id];
		if (is_report_stream(stream_id))
			token->done |= __REPORT_COMPLETE;
		else if (is_deflate_stream(stream_id))
			token->done |= __DEFLATE_COMPLETE;
	}
	if (flags & PME_COMPLETION_TRUNC)
		result->truncated = 1;
	/* Update the size */
	if (result) {
		result->dma_data.data.size = output_used;
		result->exception_code = exception_code;
	}
	/* Detach the freebuffs from the dma_layer and
	 * keep them for ourselves */
	if (flags & PME_COMPLETION_FB) {
		result->dma_data.data.type = data_out_sg_fl;
		result->dma_data.data.chain = fb_output;
	}
	if (token->done == __OPERATION_DONE) {
		/* If this is a asynchronous command, queue the token */
		if (!token->synchronous) {
			spin_lock_bh(&token->owner->completed_commands_lock);
			list_add_tail(&token->completed_list,
				      &token->owner->completed_commands);
			atomic_inc(&token->owner->completed_count);
			spin_unlock_bh(&token->owner->completed_commands_lock);
		}
		/* Wake up the thread that's waiting for us */
		wake_up(token->queue);
	}
	return 0;
}

/* Process a completed token.
 * This consists of doing any pme_mem maps that were needed
 * as well as mapping any freebuffers into user space */
static int process_completed_token(struct file *fp,
				struct scanner_token *token_p,
				struct pme_scanner_operation *user_op)
{
	int ret = 0, i;
	struct token_output *result;
	struct pme_scanner_result_data *output;
	/* The processing is complete */
	/* Don't need DMA Mapping's anymore */
	pme_mem_unmap(&token_p->input);
	for (i = 0; i < PME_SCANNER_NUM_RESULTS; i++) {
		result = &token_p->results[i];
		output = &token_p->kernel_op.results[i];
		check_and_unmap_mem(&result->dma_data);
		output->exception_code = result->exception_code;
		output->used = result->dma_data.data.size;
		if (result->truncated)
			output->flags |= PME_SCANNER_RESULT_TRUNCATED;

		if (token_p->result_flags & PME_SCANNER_RESULT_ABORTED)
			output->flags |= PME_SCANNER_RESULT_ABORTED;
		/* Handle freebuffer output */
		if (result->dma_data.data.type != data_out_sg_fl ||
			!result->dma_data.data.chain)
			continue;
		if (pme_fbchain_num(result->dma_data.data.chain) == 0) {
			pme_fbchain_recycle(result->dma_data.data.chain);
			result->dma_data.data.chain = NULL;
			continue;
		}
		ret = pme_mem_fb_map(fp, result->dma_data.data.chain,
			    (unsigned long *) &output->data, &output->size);
		output->flags |= PME_SCANNER_RESULT_FB;
		if (ret == PME_MEM_SG)
			output->flags |= PME_SCANNER_RESULT_SG;

		if (unlikely(ret < 0)) {
			for (i = 0; i < PME_SCANNER_NUM_RESULTS; i++)
				if (token_p->results[i].dma_data.data.type ==
							data_out_sg_fl)
					pme_fbchain_recycle(
						token_p->results[i].dma_data.
								data.chain);
			free_async_token(token_p);
			return ret;
		}
	}

	/* Update the used values */
	if (unlikely(copy_to_user(user_op, &token_p->kernel_op,
			 sizeof(token_p->kernel_op)))) {
		for (i = 0; i < PME_SCANNER_NUM_RESULTS; i++) {
			output = &token_p->kernel_op.results[i];
			if (output->flags & PME_SCANNER_RESULT_FB)
				pme_mem_fb_unmap(fp,
					(unsigned long)output->data);
		}
		free_async_token(token_p);
		return -EFAULT;
	}
	free_async_token(token_p);
	return 0;
}

/* Execute a command */
static int execute_cmd(struct file *fp, struct scanner_object *scanner,
	struct pme_scanner_operation *user_op, u8 synchronous)
{
	int ret, i = 0;
	struct pme_context_callback cb;
	struct scanner_token local_token;
	struct scanner_token *token_p = NULL;
	u32 cmd_flags = 0;
	struct pme_scanner_result_data *output;
	struct pme_parameters params = {
		.flags = PME_PARAM_MODE
	};
	size_t fl_size;
	struct pme_data *mem[PME_SCANNER_NUM_RESULTS];
	DECLARE_WAIT_QUEUE_HEAD(local_waitqueue);
	/* If synchronous, use a local token (from the stack)
	 * If asynchronous, allocate a token to use */
	if (synchronous)
		token_p = &local_token;
	else {
		token_p = kmalloc(sizeof(*token_p), GFP_KERNEL);
		if (!token_p)
			return -ENOMEM;
	}
	memset(mem, 0, sizeof(mem));
	/* It's vey important that this structure be zeroed out
	 * We check the deflate and report pme_mem structures for
	 * a non zero address to see if they are in use */
	memset(token_p, 0, sizeof(struct scanner_token));
	token_p->owner = scanner;
	token_p->synchronous = synchronous;
	/* Copy the op to kernel space */
	if (copy_from_user(&token_p->kernel_op, user_op, sizeof(*user_op))) {
		cleanup_token(token_p);
		return -EFAULT;
	}

	/* Map the input */
	if (unlikely(token_p->kernel_op.input_size <= 0)) {
		/* No point in allowing scanner operations that are
		 * of length zero or less */
		cleanup_token(token_p);
		return -EINVAL;
	}
	/* Get the activity code of the underlying context */
	ret = pme_context_get(scanner->context, &params);
	if (unlikely(ret)) {
		cleanup_token(token_p);
		return ret;
	}
	if (token_p->kernel_op.cmd_flags & PME_SCANNER_CMD_SG_INPUT)
		/* The specified input is SG */
		ret = pme_mem_map_vector(token_p->kernel_op.input_vector,
						token_p->kernel_op.input_size,
						&token_p->input,
						DMA_TO_DEVICE);
	else
		/* Regular buffer for input */
		ret = pme_mem_map((unsigned long)token_p->
				kernel_op.input_data,
				token_p->kernel_op.input_size,
				&token_p->input, DMA_TO_DEVICE);
	if (unlikely(ret < 0)) {
		cleanup_token(token_p);
		return ret;
	}
	/* If the user specified the EOS flag, set the command as EOS */
	if (token_p->kernel_op.cmd_flags & PME_SCANNER_CMD_EOS)
		cmd_flags |= PME_FLAG_EOS;

	/* Setup the output structures */
	for (i = 0; i < PME_SCANNER_NUM_RESULTS; i++) {
		if (is_deflate_stream(i) &&
				!pme_mode_is_recovery_done(params.mode))
			continue;
		if (is_report_stream(i) &&
				!pme_mode_is_scan_done(params.mode))
			continue;
		output = &token_p->kernel_op.results[i];
		mem[i] = &token_p->results[i].dma_data.data;
		/* Size of 0 means use freelist */
		if (!output->size)
			goto freelist_setup;

		if (output->flags & PME_SCANNER_RESULT_SG)
			/* The specified deflate output is SG */
			ret = pme_mem_map_vector(output->vector,
					     output->size,
					     &token_p->results[i].dma_data,
					     DMA_FROM_DEVICE);
		else
			/* Regular buffer for output */
			ret = pme_mem_map((unsigned long)
				output->data,
				output->size,
				&token_p->results[i].dma_data,
				DMA_FROM_DEVICE);
		if (unlikely(ret < 0)) {
			cleanup_token(token_p);
			return ret;
		}
		/* All done setup */
		continue;

freelist_setup:
		/* Caller has specified that freebuffers be used for ouput.
		 * Verify the mapping can be done safely */
		if (output->freelist_id > 1) {
			cleanup_token(token_p);
			return -EINVAL;
		}
		fl_size = pme_channel_freelist_blksize
				(scanner->channel_id, output->freelist_id);
		if (!fl_size) {
			cleanup_token(token_p);
			return -EINVAL;
		}
		if (fl_size % PAGE_SIZE) {
			/* Freebuffer output is not page sized, and therefore
			 * can only be returned to a priveleged user if they
			 * asked for it. Only allow if root and the ALLOW flag
			 * is set */
			if (!(token_p->kernel_op.cmd_flags &
					PME_SCANNER_CMD_ALLOW_ZC_IOVEC) ||
					!capable(CAP_SYS_ADMIN)) {
				cleanup_token(token_p);
				return -EPERM;
			}
		}
		/* Set up the output to use the S/G Freelist */
		mem[i]->freelist_id = output->freelist_id;
		mem[i]->type = data_out_sg_fl;
	}
	/* use the local wait queue if synchronous, the shared
	 * queue if asynchronous */
	if (synchronous)
		token_p->queue = &local_waitqueue;
	else
		token_p->queue = &scanner->waiting_for_completion;

	token_p->done = 0;
	/* Get the parameters of the context
	 * so we can determine what callbacks to
	 * expect */
	if (!(pme_mode_is_scan_done(params.mode)))
		/* No Pattern Matching report will
		 * be generated */
		token_p->done |= __REPORT_COMPLETE;

	if ((pme_mode_is_passthru(params.mode)
	     && !pme_mode_is_recovery_done(params.mode)))
		/* No Deflate output will
		 * be generated */
		token_p->done |= __DEFLATE_COMPLETE;

	/* Build the callback structure
	 * This will have a pointer to the
	 * token and we'll cleanup on the callback */
	cb.completion = scanner_op_cb;
	cb.ctx.words[0] = (u32) token_p;
	ret = pme_context_io_cmd(scanner->context, cmd_flags, &cb,
				&token_p->input.data,
				mem[PME_SCANNER_DEFLATE_IDX],
				mem[PME_SCANNER_REPORT_IDX]);
	if (unlikely(ret)) {
		/* Don't need DMA Mapping's anymore */
		cleanup_token(token_p);
		return ret;
	}
	if (!synchronous)
		/* Don't wait.  The command is away */
		return 0;

	/* Wait for the command to complete */
	wait_event(*token_p->queue, token_p->done == __OPERATION_DONE);
	return process_completed_token(fp, token_p, user_op);
}

/* Used to retreive a completed asynchronous command
 * from the pending queue */
static int complete_cmd(struct file *fp, struct scanner_object *scanner,
			struct pme_scanner_completion *user_comp_ptr)
{
	struct scanner_token *token_p = NULL, *next_token = NULL;
	LIST_HEAD(done_tokens);
	int ret = 0;
	unsigned int done_tokens_count = 0;
	struct pme_scanner_completion completions;

	if (copy_from_user(&completions, user_comp_ptr, sizeof(completions)))
		return -EFAULT;
	/* At least one result must be supplied */
	if (!completions.max_results)
		return -EINVAL;
	completions.num_results = 0;
	/* Retreive a completed command */
retry:
	spin_lock_bh(&scanner->completed_commands_lock);
	/* Move up to max_results into a temporary list
	 * for later processing.  This is to minimize
	 * the amount of time we are holding the spinlock */
	while (!list_empty(&scanner->completed_commands) &&
			(done_tokens_count < completions.max_results)) {
		list_move_tail(scanner->completed_commands.next, &done_tokens);
		++done_tokens_count;
		atomic_dec(&scanner->completed_count);
	}
	spin_unlock_bh(&scanner->completed_commands_lock);

	if (!done_tokens_count) {
		/* There were no command completions ready.
		 * Either wait for some or return, depending on
		 * the options the user specified */
		if (completions.timeout < 0)
			/* Useful for a polling interface */
			return -EWOULDBLOCK;

		if (completions.timeout == 0) {
			/* Wait forever */
			if (wait_event_interruptible(
				scanner->waiting_for_completion,
				atomic_read(&scanner->completed_count)))
				return -ERESTARTSYS;
		} else {
			/* Wait with timeout */
			ret = wait_event_interruptible_timeout
				(scanner->waiting_for_completion,
				atomic_read(&scanner->completed_count),
				msecs_to_jiffies(completions.timeout));
			if (ret < 0)
				return -ERESTARTSYS;
			if (ret == 0)
				return -ETIME;
		}
		goto retry;
	}

	/* We've extracted some commands off the queue.  Process each one
	 * and return them to the user */
	ret = 0;
	list_for_each_entry_safe(token_p, next_token, &done_tokens,
			completed_list) {
		/* if an error has occurred, push the results back
		 * onto the list of tokens to be consumed */
		if (ret) {
			spin_lock_bh(&scanner->completed_commands_lock);
			list_move(&token_p->completed_list,
				&scanner->completed_commands);
			spin_unlock_bh(&scanner->completed_commands_lock);
			continue;
		}
		ret = process_completed_token(fp, token_p,
				&completions.results[completions.num_results]);
		if (!ret)
			++completions.num_results;
	}
	/* We've popped a completed command off the queue. Copy back to the
	 * user */
	if (copy_to_user(user_comp_ptr, &completions, sizeof(completions)))
		return -EFAULT;
	return ret;
}

/* Main switch loop for ioctl operations */
static int scanner_fops_ioctl(struct inode *node, struct file *fp,
			      unsigned int opt, unsigned long val)
{
	struct file *chan_fd;
	struct scanner_object *scanner = fp->private_data;
	int ret = 0, fd;
	unsigned long user_mem;

	if (opt == PME_IOCTL_SET_CHANNEL) {
		if (copy_from_user(&fd, (void *)val, sizeof(fd)))
			return -EFAULT;
		down(&scanner->sem);
		/* Deal with SET channel first */
		if (scanner->channel_id) {
			ret = -EINVAL;
			goto done;
		}
		/* Get the channel from the file descriptor */
		chan_fd = fget(fd);
		if (!chan_fd) {
			ret = -EBADFD;
			goto done;
		}
		scanner->channel_id = pme_device_get_channel(chan_fd);
		fput(chan_fd);
		if (!scanner->channel_id)
			ret = -EBADFD;
done:
		up(&scanner->sem);
		return ret;
	}
	if (!scanner->context && opt != PME_IOCTL_SET_PARAMETERS)
		/* The context will be NULL until a channel is assigned
		 * and the inital set is done */
		return -EINVAL;

	switch (opt) {
	case PME_IOCTL_GET_PARAMETERS:
		return ioctl_get(scanner, (struct pme_parameters *)val);
		break;
	case PME_IOCTL_SET_PARAMETERS:
		return ioctl_set(scanner, (struct pme_parameters *)val);
		break;
	case PME_IOCTL_RESET_SEQUENCE_NUMBER:
		return reset_seqnum(scanner);
		break;
	case PME_IOCTL_RESET_RESIDUE:
		return reset_residue(scanner);
		break;
	case PME_SCANNER_IOCTL_EXECUTE_CMD:
		/* Synchronous Execution */
		return execute_cmd(fp, scanner,
				(struct pme_scanner_operation *)val, 1);
		break;
	case PME_IOCTL_BEGIN_EXCLUSIVE:
		return begin_exclusive(scanner);
		break;
	case PME_IOCTL_END_EXCLUSIVE:
		return end_exclusive(scanner);
		break;
	case PME_SCANNER_IOCTL_FREE_MEMORY:
		if (copy_from_user(&user_mem, (void *)val, sizeof(user_mem)))
			return -EFAULT;
		return pme_mem_fb_unmap(fp, user_mem);
		break;
	case PME_IOCTL_FLUSH:
		return pme_context_flush(scanner->context);
		break;
	case PME_SCANNER_IOCTL_EXECUTE_ASYNC_CMD:
		/* Asynchronous Execution */
		return execute_cmd(fp, scanner,
				(struct pme_scanner_operation *)val, 0);
		break;
	case PME_SCANNER_IOCTL_COMPLETE_ASYNC_CMD:
		return complete_cmd(fp, scanner,
			(struct pme_scanner_completion *)val);
		break;
	default:
		return -EINVAL;
		break;
	}
}

/* File operations for the scanner */
static struct file_operations scanner_fops = {
	.owner = THIS_MODULE,
	.ioctl = scanner_fops_ioctl,
	.open = scanner_fops_open,
	.release = scanner_fops_release,
	.llseek = scanner_fops_llseek,
	/* Use mmap routines from pme_mem */
	.mmap = pme_mem_fops_mmap,
};

static struct miscdevice scanner_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = PME_SCANNER_NODE,
	.fops = &scanner_fops
};

/* Module Init */
int pmscanner_init(void)
{
	int err = misc_register(&scanner_dev);
	if (err) {
		printk(KERN_ERR PMMOD "registration of device %s failed\n",
		       scanner_dev.name);
		return err;
	}
	printk(KERN_INFO PMMOD "device %s registered\n", scanner_dev.name);
	return 0;
}

/* Module Teardown */
void pmscanner_finish(void)
{
	int err = misc_deregister(&scanner_dev);
	if (err)
		printk(KERN_ERR PMMOD "Failed to deregister device %s, "
				"code %d\n", scanner_dev.name, err);
	printk(KERN_ERR PMMOD "device %s deregistered\n", scanner_dev.name);
}

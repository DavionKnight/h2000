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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE US:E OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Author: Geoff Thorpe, Geoff.Thorpe@freescale.com
 *
 * Description:
 * This file implements the low-level context resource as understood by the
 * pattern-matcher's DMA engine. It is an internal API. The context abstraction
 * is built on top of this to bind it to data-processing parameters and
 * functions, which forms part of the exported interface.
 *
 */

/*****************/
/* pme_object    */
/*****************/

/*
 * This is an "abstraction" that gets inlined into channel.c, thus
 * encapsulation isn't enforced by the compiler. This data-path code ought to
 * be a single compiler object so we can't break it into C files, yet it's
 * preferable to keep some separation.
 */

/* FIFO formatting definitions */
enum fifocmd_code {
	NOP = 0x0,
	STREAM_CONTEXT_DRIVEN = 0x1,
	STREAM_CONTEXT_UPDATE = 0x2
};
enum fifocmd_assignment {
	ASSIGNMENT_DEFAULT = 0x0,
	DEFLATE_IS_X = 0x1
};
enum fifocmd_treatment {
	DO_NOTHING = 0x0,
	DEALLOCATE_A = 0x1,
	DEALLOCATE_B = 0x2
};
enum fifo_src {
	CMD_ENTRY = 0x0,
	FREELIST_A = 0x1,
	FREELIST_B = 0x2,
	BUF_INVALID = 0x3
};

/* Format a command FIFO entry. This will generate codec functions, eg;
 *   static inline u64  c_get_stream_id(struct fifo_fbm *s);
 *   static inline void c_set_stream_id(struct fifo_fbm *s, u64 val);
 */
CODEC_BASIC(fifo_cmd, c, stream_id, 0)
CODEC_BASIC(fifo_cmd, c, input_phys_pointer, 1)
CODEC_BASIC(fifo_cmd, c, input_virt_pointer, 2)
CODEC_BASIC(fifo_cmd, c, output_x_phys_pointer, 3)
CODEC_FN(fifo_cmd, c, input_length, 4, 32, 32, u32)
CODEC_FN(fifo_cmd, c, input_offset, 4, 16, 16, u16)
CODEC_FN(fifo_cmd, c, output_x_offset, 4, 0, 16, u16)
CODEC_FN(fifo_cmd, c, input_treatment, 5, 62, 2, enum fifocmd_treatment)
CODEC_FN(fifo_cmd, c, input_ext, 5, 61, 1, enum fifo_ext)
CODEC_FN(fifo_cmd, c, input_fmt, 5, 60, 1, enum fifo_fmt)
CODEC_FN(fifo_cmd, c, input_ieee, 5, 59, 1, u8)
CODEC_FN(fifo_cmd, c, output_x_ieee, 5, 58, 1, u8)
CODEC_FN(fifo_cmd, c, input_eos, 5, 57, 1, u8)
CODEC_FN(fifo_cmd, c, exclusive, 5, 56, 1, u8)
CODEC_FN(fifo_cmd, c, suppress_report_notif, 5, 55, 1, u8)
CODEC_FN(fifo_cmd, c, suppress_deflate_notif, 5, 54, 1, u8)
CODEC_FN(fifo_cmd, c, int_on_completion, 5, 53, 1, u8)
CODEC_FN(fifo_cmd, c, int_on_notification, 5, 52, 1, u8)
CODEC_FN(fifo_cmd, c, output_assignment, 5, 51, 1, enum fifocmd_assignment)
CODEC_FN(fifo_cmd, c, input_bufflen, 5, 32, 17, u32)
CODEC_FN(fifo_cmd, c, cmd_code, 5, 24, 8, enum fifocmd_code)
CODEC_FN(fifo_cmd, c, output_y_src, 5, 22, 2, enum fifo_src)
CODEC_FN(fifo_cmd, c, output_y_fmt, 5, 21, 1, enum fifo_fmt)
CODEC_FN(fifo_cmd, c, output_x_ext, 5, 20, 1, enum fifo_ext)
CODEC_FN(fifo_cmd, c, output_x_src, 5, 18, 2, enum fifo_src)
CODEC_FN(fifo_cmd, c, output_x_fmt, 5, 17, 1, enum fifo_fmt)
CODEC_FN(fifo_cmd, c, output_x_bufflen, 5, 0, 17, u32)
CODEC_PTR(fifo_cmd, c, app_spec_pass_thru, 6, void)
CODEC_BASIC(fifo_cmd, c, output_y_phys_pointer, 8)
CODEC_FN(fifo_cmd, c, output_y_offset, 9, 32, 16, u16)
CODEC_FN(fifo_cmd, c, output_y_ext, 9, 31, 1, enum fifo_ext)
CODEC_FN(fifo_cmd, c, output_y_ieee, 9, 30, 1, u8)
CODEC_FN(fifo_cmd, c, output_y_bufflen, 9, 0, 17, u32)

/* Format a notification FIFO entry. This will generate codec functions, eg;
 *   static inline u64  n_get_stream_id(struct fifo_fbm *s);
 *   static inline void n_set_stream_id(struct fifo_fbm *s, u64 val);
 */
CODEC_BASIC(fifo_notify, n, stream_id, 0)
CODEC_BASIC(fifo_notify, n, output_phys_pointer, 1)		/* BV */
CODEC_BASIC(fifo_notify, n, output_virt_pointer, 2)		/* BV */
CODEC_BASIC(fifo_notify, n, output_phys_hptr, 1)		/* LL */
CODEC_BASIC(fifo_notify, n, output_virt_hptr, 2)		/* LL */
CODEC_BASIC(fifo_notify, n, output_virt_tptr, 3)		/* LL */
CODEC_FN(fifo_notify, n, output_buff_cnt, 4, 32, 16, u16)	/* LL */
CODEC_FN(fifo_notify, n, output_length, 4, 0, 32, u32)
CODEC_FN(fifo_notify, n, output_block_size, 4, 48, 16, u16)
CODEC_FN(fifo_notify, n, output_tbl_count, 5, 48, 16, u16) 	/* BV */
CODEC_FN(fifo_notify, n, output_offset, 5, 32, 16, u16)
CODEC_FN(fifo_notify, n, exception_code, 5, 24, 8, u8)
CODEC_FN(fifo_notify, n, output_truncated, 5, 21, 1, u8)
CODEC_FN(fifo_notify, n, output_ext, 5, 20, 1, enum fifo_ext)
CODEC_FN(fifo_notify, n, output_buffer_src, 5, 18, 2, enum fifo_src)
CODEC_FN(fifo_notify, n, output_buffer_fmt, 5, 17, 1, enum fifo_fmt)
CODEC_FN(fifo_notify, n, output_bufflen, 5, 0, 17, u32)
CODEC_PTR(fifo_notify, n, app_spec_pass_thru, 6, void)

/* Per-object state. We don't store per-command state here, it is printed onto
 * command FIFO entries and present in any notification FIFO entries also. We
 * do however store a reference count for completion-handling and wake-ups. */
struct pme_object {
	struct pme_channel *channel;
	struct pme_channel_err_ctx notifier;
	int stopped;
	int topdog;
	struct __pme_dma_resource ctx;
	struct __pme_dma_resource residue;
	atomic_t refs;
	atomic_t err_refs;
	/* Locks access to stopped/topdog/etc */
	spinlock_t lock;
	/* Used to link sleeper contexts for threads */
	struct list_head sleepers;
	/* This item is used by channel logic for linking this object to the
	 * 'access_list'. */
	struct list_head access;
	/* Whenever a command is started that will use freelist output(s), the
	 * destination fbchain objects are linked as chains_output. The
	 * completions will pop them off - care must be taken when processing
	 * 'abortion' logic to empty any chains that are in this list. We also
	 * store input fbchains when they are going to be deallocated on
	 * consumption, in chains_input. Both lists use the lock. */
	struct list_head chains_output;
	struct list_head chains_input;
	/* Used when stopping/destroying an object */
	wait_queue_head_t queue;
	/* User context */
	void *opaque;
	void (*dtor)(void *, enum pme_dtor reason);
	/* Flags passed to kernel memory allocation functions */
	unsigned int gfp_flags;
	/* These flags are PME_DMA flags that are sticky
	 * (like exclusive and dont_sleep) */
	u32 flags;
	/* The scanning paramters */
	struct pme_parameters params;
	/* Stream IDs */
	u64 report_sid;
	u64 deflate_sid;

	/* 0 for idle, -1 for updating, postive when
	 * scanning */
	atomic_t state;
};

/* Key Values for STREAM_CONTEXT_UPDATE Commands */
#define PME_UPDATE_KEY_ACTIVITY_CODE		0x01040000
#define PME_UPDATE_KEY_SESSION_ID		0x01040001
#define PME_UPDATE_KEY_RESIDUE_LEN		0x01000002
#define PME_UPDATE_KEY_PATTERN_SET		0x01010003
#define PME_UDPATE_KEY_PATTERN_SUBSET		0x01020004
#define PME_UPDATE_KEY_SEQUENCE_NUMBER		0x01000005
#define PME_UPDATE_KEY_REPORT_SID_HIGH		0x01040006
#define PME_UPDATE_KEY_REPORT_SID_LOW		0x01040007
#define PME_UPDATE_KEY_RESIDUE_PTR_HIGH		0x01040008
#define PME_UPDATE_KEY_RESIDUE_PTR_LOW		0x01040009
#define PME_UPDATE_KEY_DEFLATE_SID_HIGH		0x0104000A
#define PME_UPDATE_KEY_DEFLATE_SID_LOW		0x0104000B
#define PME_UPDATE_KEY_END_OF_SUI_EVENT		0x0101000C
#define PME_UPDATE_KEY_DEBUG_CTRL_CODE		0x01010100

/* Size of the data is stored in the key in the
 * second byte */
static inline size_t get_size_from_key(u32 key)
{
	size_t val = ((key & 0x00FF0000) >> 16);
	return val;
}

#define MAX_PME_CONTEXT_UPDATE_OPTIONS	14
#define MAX_UPDATE_BUFFER_SIZE \
	(MAX_PME_CONTEXT_UPDATE_OPTIONS * sizeof(u32) * 2)

/* The 'err_refs' count is to prevent racing between things like topdog(),
 * residue(), and the error notification. Typically, the error notification
 * will fire, take the err_ref count to zero, and thus release any topdog
 * status or residue. Using inc/dec can delay that cleanup when the error
 * notification occurs to avoid races without locking (because those functions
 * need to be able to sleep). */
static inline int err_inc(struct pme_object *obj)
{
	if (unlikely(atomic_inc_return(&obj->err_refs) == 1)) {
		/* So someone else had already taken it to zero and so has
		 * cleaned up (or is doing so right now). Don't call err_dec()
		 * here. */
		atomic_dec(&obj->err_refs);
		return -ENODEV;
	}
	return 0;
}

static inline void err_dec(struct pme_object *obj)
{
	int stopped;
	/* The trick here is that __object_free() will call this as will the
	 * error notification, if it occurs. If both occur, the
	 * __object_free() call happens second, races against nothing, and
	 * takes a zero-count down to -1, which is harmless. It allows code
	 * like topdog() to call inc/dec as a protection against an error
	 * notification coming in. */
	if (!atomic_dec_and_test(&obj->err_refs))
		return;
	/* If racers go into err_inc at the same time after we already hit
	 * zero, it's possible one of them will think everything's fine, go on
	 * to fail his operation (ie. topdog() or residue(), because the
	 * channel is actually broken), then call err_dec() and trigger the
	 * cleanup to run twice! We use 'stopped' to prevent this. */
	spin_lock_bh(&obj->lock);
	stopped = obj->stopped;
	obj->stopped = 1;
	spin_unlock_bh(&obj->lock);
	if (stopped)
		return;
	if (obj->topdog) {
		channel_topdog(obj->channel, obj, 0);
		obj->topdog = 0;
	}
	if (!pme_dma_resource_empty(&obj->residue))
		channel_residue_free(obj->channel, &obj->residue);
	channel_context_free(obj->channel, &obj->ctx);
}

static inline void __object_free(struct pme_object *obj)
{
	/* Wakeup only if the ref count reaches 1 */
	int val = atomic_dec_return(&obj->refs);
	if (val == 1)
		wake_up(&obj->queue);
	if (likely(val != 0))
		return;
	/* There are no references left */
	if (obj->dtor)
		obj->dtor(obj->opaque, pme_dtor_object);
	err_dec(obj);
	kfree(obj);
}
static void object_notifier_cb(struct pme_channel_err_ctx *ctx, int error)
{
	struct pme_object *obj = container_of(ctx, struct pme_object,
						notifier);
	if (obj->dtor && error)
		obj->dtor(obj->opaque, pme_dtor_channel);
	if (error)
		err_dec(obj);
	__object_free(obj);
}

static int object_complete_cmd(struct pme_channel *channel,
					struct fifo_cmd *ptr, u8 aborting,
					u8 reduced)
{
	enum fifocmd_treatment treatment;
	struct pme_fbchain *releaser = NULL;
	int ret = 0, inc = 1;
	u32 *pthru = c_get_app_spec_pass_thru(ptr);
	/* The remaining passthru is the callback structure. */
	struct pme_callback *cb = (struct pme_callback *)(pthru + 1);
	/* The first 32-bits of passthru is the obj (lowest bit is
	 * command-expiry). */
	struct pme_object *obj = u64_to_ptr(pthru[0] & ~1);
	/* Whether this is aborting or not, if the input data was an fbchain to
	 * be released, we'll need to post-process */
	treatment = c_get_input_treatment(ptr);
	if (unlikely(treatment != DO_NOTHING)) {
		spin_lock_bh(&obj->lock);
		releaser = list_entry(obj->chains_input.next,
				struct pme_fbchain, list);
		list_del(&releaser->list);
		spin_unlock_bh(&obj->lock);
	}
	/* Perform an abort callback if appropriate */
	if (unlikely(aborting)) {
		int numchains = 0;
		enum fifo_src src;
		/* If the input was to be deallocated to a freelist, we need to
		 * correct channel book-keeping - the owner will resume his/her
		 * responsibilities. */
		if (releaser)
			channel_inputrelease_abort(channel, releaser);
		/* Return value is meaningless */
		cb->completion(obj, PME_COMPLETION_ABORTED, 0,
				c_get_stream_id(ptr), cb, 0, NULL);
		/* Pop off any chains that were going to be used for
		 * encapsulating freelist output. Freelists only apply to
		 * DRIVEN commands. */
		if (c_get_cmd_code(ptr) == STREAM_CONTEXT_DRIVEN) {
			src = c_get_output_x_src(ptr);
			if ((src == FREELIST_A) || (src == FREELIST_B))
				numchains++;
			src = c_get_output_y_src(ptr);
			if ((src == FREELIST_A) || (src == FREELIST_B))
				numchains++;
			spin_lock_bh(&obj->lock);
			while (numchains--) {
				struct pme_fbchain *chain;
				chain = list_entry(obj->chains_output.next,
						struct pme_fbchain, list);
				list_del(&chain->list);
				fbchain_free(chain);
			}
			spin_unlock_bh(&obj->lock);
		}
		/* We need to calculate how many references we added, and drop
		 * them. */
		switch (c_get_cmd_code(ptr)) {
		case NOP:
			inc++;
			break;
		case STREAM_CONTEXT_UPDATE:
			break;
		case STREAM_CONTEXT_DRIVEN:
			if (!c_get_suppress_deflate_notif(ptr))
				inc++;
			if (!c_get_suppress_report_notif(ptr))
				inc++;
			break;
		default:
			BUG();
		}
	/* Perform a command-completion if asked or if it's an UPDATE */
	} else {
		/* If the input was to be deallocated to a freelist, it has
		 * been consumed. */
		if (unlikely(releaser))
			channel_inputrelease_consume(channel, releaser);
		if ((pthru[0] & 1) || (c_get_cmd_code(ptr) ==
					STREAM_CONTEXT_UPDATE))
			ret = cb->completion(obj, PME_COMPLETION_COMMAND, 0,
					c_get_stream_id(ptr), cb, 0, NULL);
	}
	while (inc--)
		__object_free(obj);
	return ret;
}

static int object_complete_notification(struct pme_channel *channel,
						struct fifo_notify *ptr)
{
	int ret;
	u32 flags = 0;
	u32 *pthru = n_get_app_spec_pass_thru(ptr);
	/* The first 32-bits is obj (lowest bit is command-expiry). The
	 * remainder is the callback structure, read it in-place */
	struct pme_object *obj = u64_to_ptr(pthru[0] & ~1);
	struct pme_callback *cb = (struct pme_callback *)(pthru + 1);
	struct pme_fbchain *chain = NULL;
	enum fifo_src src = n_get_output_buffer_src(ptr);
	enum fifo_fmt fmt = n_get_output_buffer_fmt(ptr);
	size_t output_used = n_get_output_length(ptr);

	if (n_get_output_truncated(ptr))
		flags |= PME_COMPLETION_TRUNC;
	if (unlikely(src == BUF_INVALID)) {
		flags |= PME_COMPLETION_NULL;
		output_used = 0;
		goto do_callback;
	}
	/* Generic stuff for CMD_ENTRY or FREELIST_[A|B] */
	if (fmt == BV)
		flags |= PME_COMPLETION_SG;
	if (src == CMD_ENTRY)
		goto do_callback;
	/* Handle freelist output */
	flags |= PME_COMPLETION_FB;
	spin_lock_bh(&obj->lock);
	chain = list_entry(obj->chains_output.next, struct pme_fbchain, list);
	list_del(&chain->list);
	spin_unlock_bh(&obj->lock);
	chain->freelist_id = ((src == FREELIST_A) ? 0 : 1);
	chain->num_blocks = n_get_output_buff_cnt(ptr);
	chain->blocksize = n_get_output_block_size(ptr);
	if (!chain->blocksize)
		/* for the 16-bit field, 0 represents 64Kb */
		chain->blocksize = 0x00010000;
	chain->data_len = output_used;
	if (fmt == LL)
		fbchain_setup_ll(chain, n_get_output_virt_hptr(ptr),
				n_get_output_phys_hptr(ptr),
				n_get_output_virt_tptr(ptr),
				n_get_output_offset(ptr),
				n_get_output_bufflen(ptr));
	else
		fbchain_setup_sg(chain, n_get_output_virt_pointer(ptr),
				n_get_output_phys_pointer(ptr),
				n_get_output_ext(ptr),
				n_get_output_tbl_count(ptr),
				n_get_output_offset(ptr),
				n_get_output_bufflen(ptr));
	/* Freelist accounting */
	channel_fb_owned(channel, chain);
do_callback:
	ret = cb->completion(obj, flags, n_get_exception_code(ptr),
			n_get_stream_id(ptr), cb, output_used, chain);
	/* Release a reference to the object */
	__object_free(obj);
	return ret;
}

int pme_object_alloc(struct pme_object **obj, struct pme_channel *channel,
			unsigned int gfp_flags, void *userctx,
			void (*dtor)(void *userctx, enum pme_dtor reason))
{
	int err;
	struct pme_object *ret = kmalloc(sizeof(*ret), gfp_flags);
	if (!ret)
		return -ENOMEM;
	ret->channel = channel;
	ret->stopped = 0;
	ret->topdog = 0;
	ret->opaque = userctx;
	ret->dtor = dtor;
	ret->gfp_flags = gfp_flags;
	pme_dma_resource_init(&ret->ctx);
	pme_dma_resource_init(&ret->residue);
	/* NB, we start with a ref-count of 2. The reason is simple, there is
	 * one initial ref-count from our user's perspective and an internal
	 * reference due to the channel object-registration. The deregistration
	 * (or channel error, we get one or the other but never both) removes
	 * one reference. The user also removes one of these references when
	 * calling pme_object_free(). */
	atomic_set(&ret->refs, 2);
	/* This count is used to protect against races between code that can't
	 * lock (might sleep) yet would not handle an error notification
	 * occuring during the sleep. */
	atomic_set(&ret->err_refs, 1);
	spin_lock_init(&ret->lock);
	INIT_LIST_HEAD(&ret->sleepers);
	INIT_LIST_HEAD(&ret->chains_output);
	INIT_LIST_HEAD(&ret->chains_input);
	init_waitqueue_head(&ret->queue);
	err = channel_context_alloc(channel, &ret->ctx, gfp_flags);
	if (err)
		goto error_alloc;
	ret->notifier.cb_on_deregister = 1;
	ret->notifier.cb = object_notifier_cb;
	err = pme_channel_err_register(channel, &ret->notifier);
	if (err)
		goto error_register;
	*obj = ret;
	return 0;
error_register:
	channel_context_free(channel, &ret->ctx);
error_alloc:
	kfree(ret);
	return err;
}
EXPORT_SYMBOL(pme_object_alloc);

void pme_object_free(struct pme_object *obj)
{
	/* We attempt to explicitly deregister here because the user will not
	 * expect callbacks after this point, even if the object remains
	 * allocated due to outstanding completions. */
	pme_channel_err_deregister(obj->channel, &obj->notifier, 0);
	__object_free(obj);
}
EXPORT_SYMBOL(pme_object_free);

int pme_object_wait(struct pme_object *obj)
{
	int err;
	/* This is difficult to do without deregistration because otherwise we
	 * need to allow a ref-count of 2 if the object isn't stopped or 1 if
	 * it is - yet being "stopped" is an unlocked event from tasklet
	 * context that can happen at any time.  What we do is deregister,
	 * guaranteeing that the unlocked event can't occur thereafter. Then we
	 * wait until the ref-count is 1, at that point we can reregister
	 * before returning. Reregistration fails if the channel stopped, so
	 * we're covered at any point in time the channel error occurs. */
	pme_channel_err_deregister(obj->channel, &obj->notifier, 1);
	if (obj->stopped)
		return -ENODEV;
	wait_event(obj->queue, atomic_read(&obj->refs) == 1);
	/* Inc prior to the registration because notifications could occur
	 * before registration returns. */
	atomic_inc(&obj->refs);
	/* NB: no need to repopulate obj->notifier.cb[_on_deregister] */
	err = pme_channel_err_register(obj->channel, &obj->notifier);
	if (err) {
		atomic_dec(&obj->refs);
		return -ENODEV;
	}
	return 0;
}
EXPORT_SYMBOL(pme_object_wait);

/* Extra 'enum pme_data_type' helpers for pme_object_cmd() */
static inline int pme_data_in_normal(struct pme_data *p)
{ return (p->type == data_in_normal); }
static inline int pme_data_in_sg(struct pme_data *p)
{ return (p->type == data_in_sg); }
static inline int pme_data_out_freelist(struct pme_data *p)
{ return (p->type >= data_out_fl); }
static inline int pme_data_out_sg(struct pme_data *p)
{ return ((p->type == data_out_sg) || (p->type == data_out_sg_fl)); }

int pme_object_cmd(struct pme_object *obj, u32 flags,
			struct pme_callback *cb,
			struct pme_data *input,
			struct pme_data *deflate,
			struct pme_data *report)
{
	int tmp;
	void *ptr;
	struct pme_data *x, *y;
	struct pme_fbchain *chain1 = NULL, *chain2 = NULL;

#ifdef IF_YOU_DONT_TRUST_THE_CALLER
	/* NOP requires NULL input, deflate, report, no completion flags */
	if ((flags & PME_FLAG_NOP) && (input || deflate || report ||
			(flags & (PME_FLAG_CB_ALL | PME_FLAG_UPDATE |
					PME_FLAG_EOS))))
		panic("non-NULL parameters for NOP call\n");
	/* Input required non-NULL for everything else */
	if (!(flags & PME_FLAG_NOP) && !input)
		panic("Invalid input\n");
	/* Input-release requires a callback flag, otherwise the caller has no
	 * way to know when it's safe to forget about the input fbchain (if the
	 * command is aborted, the caller must recycle his chain himself). */
	if ((flags & PME_FLAG_RECYCLE) && !(flags & PME_FLAG_CB_ALL))
		panic("input recycling requires command callback\n");
	/* Update requires NULL deflate, report, no completion flags, and no
	 * notify */
	if ((flags & PME_FLAG_UPDATE) && (deflate || report ||
			(flags & (PME_FLAG_CB_ALL |
				PME_FLAG_INT_ON_NOTIFICATION))))
		panic("non-NULL output parameters for UPDATE call\n");
	/* Update requires non-NULL input */
	if ((flags & PME_FLAG_UPDATE) && (!input ||
				!pme_data_in_normal(input) ||
				(flags & PME_FLAG_EOS)))
		panic("Unusual input for UPDATE call\n");
	/* Check input type */
	if (input && !pme_data_input(input))
		panic("Invalid input type %d\n", input->type);
	/* Report should be non-NULL iff CB_REPORT set
	 * It's OK for deflate to be NULL because we need
	 * to collect the result flags of the deflate stage
	 * even if the data isn't going to be recovered */
	if (deflate && !(flags & PME_FLAG_CB_DEFLATE))
		panic("Invalid deflate configuration\n");
	if ((report && !(flags & PME_FLAG_CB_REPORT)) ||
			(!report && (flags & PME_FLAG_CB_REPORT)))
		panic("Invalid report configuration\n");
	/* Check output types */
	if ((deflate && !pme_data_output(deflate)) ||
		(report && !pme_data_output(report)))
		panic("Invalid output types\n");
	/* The caller should not pass us misaligned updates, but make this a
	 * debug check rather than returning -EFAULT or anything. */
	if ((flags & PME_FLAG_UPDATE) && (!pme_data_in_normal(input) ||
						(input->addr & 0x7)))
		panic("Mis-aligned stream-context update\n");
#endif

	if (obj->stopped)
		return -EBUSY;
	/* If our output will need chains, pre-allocate */
	if (deflate && pme_data_out_freelist(deflate)) {
		chain1 = fbchain_alloc(obj->channel, obj->gfp_flags);
		if (!chain1)
			return -ENOMEM;
	}
	if (report && pme_data_out_freelist(report)) {
		chain2 = fbchain_alloc(obj->channel, obj->gfp_flags);
		if (!chain2) {
			if (chain1)
				fbchain_free(chain1);
			return -ENOMEM;
		}
	}
	/* Add the chains to the list that gets drained by notification
	 * handling. In this way we always have (no more than) enough available
	 * for the outstanding completions, but don't need to allocate in the
	 * completion-handler. */
	if (chain1 || chain2) {
		spin_lock_bh(&obj->lock);
		if (chain1)
			list_add_tail(&chain1->list, &obj->chains_output);
		if (chain2)
			list_add_tail(&chain2->list, &obj->chains_output);
		spin_unlock_bh(&obj->lock);
	}
	/* The call to _start() will block if necessary, but returns with a
	 * writable FIFO pointer and the FIFO spinlock held. We then have to
	 * call _finish() or _revert() to release the FIFO lock. */
	ptr = channel_cmd_start(obj->channel, obj, &tmp,
			!(flags & PME_FLAG_DONT_SLEEP));
	if (!ptr)
		goto err_nocmd;

	/* Two non-freelist outputs can only be specified if we're not in
	 * reduced mode. 'tmp' is non-zero if the FIFO is in reduced mode. */
	if (tmp && deflate && report && !chain1 && !chain2) {
		/* This is a run-time configuration problem, and doesn't
		 * necessarily represent a coding bug. */
		printk(KERN_ERR PMMOD "Reduced command FIFO mode\n");
		tmp = -EINVAL;
		goto err;
	}

	/**********************************/
	/* Encode the command FIFO entry. */
	/* Set EXCLUSIVITY, CB_***, INT_ON_***, APP_SPEC_PASS_THRU no matter
	 * what. */
	c_set_exclusive(ptr, (flags & PME_FLAG_EXCLUSIVITY) ? 1 : 0);
	c_set_suppress_deflate_notif(ptr,
		(flags & (PME_FLAG_CB_DEFLATE | PME_FLAG_NOP |
			PME_FLAG_UPDATE)) ?  0 : 1);
	c_set_suppress_report_notif(ptr,
		(flags & (PME_FLAG_CB_REPORT | PME_FLAG_NOP |
			PME_FLAG_UPDATE)) ?  0 : 1);
	c_set_int_on_completion(ptr, (flags & PME_FLAG_INT_ON_COMPLETION) ?
			1 : 0);
	c_set_int_on_notification(ptr, (flags & PME_FLAG_INT_ON_NOTIFICATION) ?
			1 : 0);
	{
	/* The first u64 of passthrough is the pme_object and the first
	 * 32-bits of callback, the second u64 is the remaining 64-bits of
	 * callback. Don't use a 64-bit pointer type otherwise gcc might assume
	 * 64-bit alignment for the memcpy. */
	u32 *pdata = c_get_app_spec_pass_thru(ptr);
	/* The CB_COMMAND flag has no representation in the command FIFO and we
	 * have to be able to parse it from the tasklet without using object
	 * state. So mask it as a lowest-bit in the object's address, which has
	 * at least 2 low-order bits fixed as zero. */
	pdata[0] = ptr_to_u32(obj) | ((flags & PME_FLAG_CB_COMMAND) ? 1 : 0);
	memcpy(pdata + 1, cb, sizeof(*cb));
	}
	/* If the operation is a NOP, we're finished encoding. */
	if (flags & PME_FLAG_NOP) {
		c_set_cmd_code(ptr, NOP);
		c_set_input_treatment(ptr, DO_NOTHING);
		goto ok;
	}
	/* Encode input. */
	if (input->type == data_in_fbchain) {
		if (input->chain->type == fbchain_ll) {
			c_set_input_phys_pointer(ptr,
					input->chain->ll.head.phys);
			c_set_input_virt_pointer(ptr,
					input->chain->ll.head.virt);
			c_set_input_fmt(ptr, LL);
			c_set_input_bufflen(ptr,
					input->chain->ll.head.bufflen);
		} else {
			c_set_input_phys_pointer(ptr,
					input->chain->sg.phys);
			c_set_input_virt_pointer(ptr,
					input->chain->sg.head.addr);
			c_set_input_ext(ptr,
					(input->chain->sg.head.__extoffset &
						0x80000000) ? 1 : 0);
			c_set_input_fmt(ptr, BV);
			c_set_input_bufflen(ptr,
					input->chain->sg.head.bufflen);
			c_set_input_ieee(ptr,
				(input->flags & PME_DATA_IEEE1212) ?  1 : 0);
		}
		c_set_input_length(ptr, input->chain->data_len);
		c_set_input_eos(ptr, (flags & PME_FLAG_EOS) ? 1 : 0);
		if (flags & PME_FLAG_RECYCLE) {
			channel_inputrelease_add(obj->channel, input->chain);
			spin_lock_bh(&obj->lock);
			list_add_tail(&input->chain->list, &obj->chains_input);
			spin_unlock_bh(&obj->lock);
			c_set_input_treatment(ptr, input->chain->freelist_id ?
						DEALLOCATE_B : DEALLOCATE_A);
		} else
			c_set_input_treatment(ptr, DO_NOTHING);
	} else {
		c_set_input_phys_pointer(ptr, (u64)input->addr);
		c_set_input_length(ptr, input->length);
		c_set_input_ext(ptr, pme_data_in_sg(input) ?
					EXT_SCATTERGATHER : EXT_CONTIGUOUS);
		c_set_input_treatment(ptr, DO_NOTHING);
		c_set_input_fmt(ptr, BV);
		c_set_input_bufflen(ptr, input->size);
		c_set_input_ieee(ptr,
			(input->flags & PME_DATA_IEEE1212) ?  1 : 0);
	}
	/* XXX: input fbchains must be unmodified - currently this is ok
	 * because there are no APIs to modify them. The hardware handles
	 * INPUT_RELEASE logic using the data length, bufflens, etc, so those
	 * values mustn't have been diddled with by user-modification since the
	 * chain was first produced otherwise we might leak blocks or get
	 * mangled in weird and wonderful ways. */
	c_set_input_offset(ptr, 0);
	c_set_input_eos(ptr, (flags & PME_FLAG_EOS) ? 1 : 0);
	/* Encode stream context. */
	c_set_stream_id(ptr, (u64)obj->ctx.addr);
	/* If the operation is UPDATE, we're finished encoding. */
	if (flags & PME_FLAG_UPDATE) {
		c_set_cmd_code(ptr, STREAM_CONTEXT_UPDATE);
		goto ok;
	}
	c_set_cmd_code(ptr, STREAM_CONTEXT_DRIVEN);
	if (!deflate || !report) {
		/* Choosing source X should be easy */
		if (deflate) {
			x = deflate;
			y = report;
		} else {
			x = report;
			y = deflate;
		}
	} else {
		/* We have dual output, give preference to non-freelist */
		if (pme_data_out_freelist(deflate)) {
			x = report;
			y = deflate;
		} else {
			x = deflate;
			y = report;
		}
	}
	/* Proceed on the assumptions;
	 *   (a) y can only be non-NULL if x is,
	 *   (b) if either x or y uses freelist output, y does. */
#ifdef IF_YOU_DONT_TRUST_THE_CALLER
	if (y && tmp && !pme_data_out_freelist(y))
		panic("dual non-freelist output in reduced mode\n");
#endif
	if (deflate && (deflate == x))
		c_set_output_assignment(ptr, DEFLATE_IS_X);
	else
		c_set_output_assignment(ptr, ASSIGNMENT_DEFAULT);
	if (!x) {
		c_set_output_x_src(ptr, BUF_INVALID);
		c_set_output_y_src(ptr, BUF_INVALID);
		goto skip_output;
	}
	if (pme_data_out_freelist(x)) {
		c_set_output_x_src(ptr,
				x->freelist_id ? FREELIST_B : FREELIST_A);
		c_set_output_x_fmt(ptr, pme_data_out_sg(x) ? BV : LL);
	} else {
		c_set_output_x_phys_pointer(ptr, (u64)x->addr);
		c_set_output_x_offset(ptr, 0);
		c_set_output_x_ext(ptr, pme_data_out_sg(x) ?
				EXT_SCATTERGATHER : EXT_CONTIGUOUS);
		c_set_output_x_src(ptr, CMD_ENTRY);
		c_set_output_x_fmt(ptr, BV);
		c_set_output_x_bufflen(ptr, x->size);
		c_set_output_x_ieee(ptr,
				(x->flags & PME_DATA_IEEE1212) ? 1 : 0);
		/* If a user supplies a 0 length output buffer, then the
		 * hardware expects that the command fifo entry will specify
		 * that no output buffer was supplied. The driver software
		 * implementes this rule and upon detection of a 0 length user
		 * specified buffer sets the appropriate buffer source
		 * information in the command fifo entry. */
		if (unlikely(!x->size))
			c_set_output_x_src(ptr, BUF_INVALID);
	}
	if (!y) {
		c_set_output_y_src(ptr, BUF_INVALID);
		goto skip_output;
	}
	if (pme_data_out_freelist(y)) {
		c_set_output_y_src(ptr,
				y->freelist_id ? FREELIST_B : FREELIST_A);
		c_set_output_y_fmt(ptr, pme_data_out_sg(y) ? BV : LL);
	} else {
		c_set_output_y_src(ptr, CMD_ENTRY);
		c_set_output_y_fmt(ptr, BV);
		c_set_output_y_phys_pointer(ptr, (u64)y->addr);
		c_set_output_y_offset(ptr, 0);
		c_set_output_y_ext(ptr, pme_data_out_sg(y) ?
				EXT_SCATTERGATHER : EXT_CONTIGUOUS);
		c_set_output_y_bufflen(ptr, y->size);
		c_set_output_x_ieee(ptr,
				(x->flags & PME_DATA_IEEE1212) ? 1 : 0);
		/* As per block guide, zero-length output descriptors need
		 * to be typed as BUF_INVALID instead of CMD_ENTRY. */
		if (unlikely(!y->size))
			c_set_output_y_src(ptr, BUF_INVALID);
	}
skip_output:
	/* Set suppress bits */
	c_set_suppress_deflate_notif(ptr,
			(flags & PME_FLAG_CB_DEFLATE) ? 0 : 1);
	c_set_suppress_report_notif(ptr,
			(flags & PME_FLAG_CB_REPORT) ? 0 : 1);
ok:
	/* Success */
	atomic_add(1 + ((flags & PME_FLAG_CB_DEFLATE) ? 1 : 0) +
			((flags & PME_FLAG_CB_REPORT) ? 1 : 0) +
			((flags & PME_FLAG_NOP) ? 1 : 0),
			&obj->refs);
	channel_cmd_finish(obj->channel, obj);
	return 0;
err:
	/* Failure */
	channel_cmd_revert(obj->channel, obj);
err_nocmd:
	/* If chains were allocated, release the same  number of chains.
	 * Because another command completion may have consumed the exact
	 * chains that were allocated at the start of this API, pull chains
	 * off the chains_output list and free them */
	if (chain1 || chain2) {
		int todelete = (chain1 ? 1 : 0) + (chain2 ? 1 : 0);
		spin_lock_bh(&obj->lock);
		while (todelete--) {
			chain1 = list_entry(obj->chains_output.next,
					struct pme_fbchain, list);
			list_del(&chain1->list);
			fbchain_free(chain1);
		}
		spin_unlock_bh(&obj->lock);
	}
	return tmp;
}
EXPORT_SYMBOL(pme_object_cmd);

int pme_object_topdog(struct pme_object *obj, int enable)
{
	int res;
	/* NB, the call to channel_topdog() might block so we can't hold the
	 * object lock. OTOH: the notification callback might fire
	 * asynchronously and auto-cleanup the topdog status. We inc the
	 * 'err_refs' count for the duration of this function to protect
	 * against this issue. */
	res = err_inc(obj);
	if (res)
		return res;
	if (enable)
		res = channel_topdog(obj->channel, obj, enable);
	else {
		if (obj->stopped)
			res = 0;
		else
			res = channel_topdog(obj->channel, obj, enable);
	}
	err_dec(obj);
	return res;
}
EXPORT_SYMBOL(pme_object_topdog);

int pme_object_residue(struct pme_object *obj, int enable, u64 *addr)
{
	int res = err_inc(obj);
	if (res)
		return res;
	might_sleep_if(enable && !(obj->gfp_flags & GFP_ATOMIC));
	if (enable) {
		res = channel_residue_alloc(obj->channel, &obj->residue,
						obj->gfp_flags);
		*addr = (u64)obj->residue.addr;
	} else {
		if (!obj->stopped)
			channel_residue_free(obj->channel, &obj->residue);
		res = 0;
	}
	err_dec(obj);
	return res;
}
EXPORT_SYMBOL(pme_object_residue);

void pme_object_write(struct pme_object *obj, void *val)
{
	obj->opaque = val;
}
EXPORT_SYMBOL(pme_object_write);

void *pme_object_read(struct pme_object *obj)
{
	return obj->opaque;
}
EXPORT_SYMBOL(pme_object_read);

void __pme_object_suspend_tasklet(struct pme_object *obj, int suspended)
{
	channel_suspend(obj->channel, suspended);
}
EXPORT_SYMBOL(__pme_object_suspend_tasklet);

void __pme_object_invoke_tasklet(struct pme_object *obj)
{
	channel_sub_tasklet(obj->channel);
}
EXPORT_SYMBOL(__pme_object_invoke_tasklet);

/*********************************************************
 *
 * This file contains the all the logic used to
 * create and update Stream Contexts
 * The functions defined here are intended only to be
 * called from within the pme_base module
 *
 *********************************************************/
struct update_cmd_ctx {
	/* The space for the update command */
	u64 buffer[MAX_UPDATE_BUFFER_SIZE / 8];

	/* The command stored in physical address */
	struct pme_data cmd_buf;

	/* The next free space for a key/value pair */
	void *position;
};

static struct kmem_cache *update_cmd_cache;

#define KEY_VALUE_BYTE_SIZE	8

/* Initializes the local command structure */
int update_command_init(struct pme_object *ctx, struct update_cmd_ctx **cmd)
{
	*cmd = kmem_cache_alloc(update_cmd_cache, ctx->gfp_flags);
	if (!(*cmd))
		return -ENOMEM;
	(*cmd)->cmd_buf.length = 0;
	(*cmd)->cmd_buf.size = 0;
	(*cmd)->cmd_buf.type = data_in_normal;
	(*cmd)->cmd_buf.addr = DMA_MAP_SINGLE((*cmd)->buffer,
					      sizeof((*cmd)->buffer),
					      DMA_TO_DEVICE);
	/* Set position to the start of the buffer */
	(*cmd)->position = (*cmd)->buffer;
	return 0;
}

/***********************************************
 * Releases resources held by a local command
 **********************************************/
void update_command_free(struct update_cmd_ctx *cmd)
{
	DMA_UNMAP_SINGLE(cmd->cmd_buf.addr, sizeof(cmd->buffer), DMA_TO_DEVICE);
	kmem_cache_free(update_cmd_cache, cmd);
}

/****************************************
 * Add a control command to the buffer
 * This function takes care of left
 * alignment and padding the values
 ***************************************/
void update_command_add(struct update_cmd_ctx *cmd, u32 key, const void *value)
{
	size_t pad_size;
	size_t size = get_size_from_key(key);

	BUG_ON((cmd->position + size + sizeof(key)) >
			(void *)(cmd->buffer + sizeof(cmd->buffer)));

	/* Insert the key */
	memcpy(cmd->position, &key, sizeof(key));
	cmd->position += sizeof(key);

	/* Pad the structure with zeros at the left side, if needed */
	pad_size = 4 - size;
	if (pad_size) {
		memset(cmd->position, 0, pad_size);
		cmd->position += pad_size;
	}

	/* Add the proper number of bytes from the caller */
	if (size) {
		memcpy(cmd->position, value, size);
		cmd->position += size;
	}

	cmd->cmd_buf.length += KEY_VALUE_BYTE_SIZE;
	cmd->cmd_buf.size += KEY_VALUE_BYTE_SIZE;
}

/*************************************
 * Callback for command completion
 * Wakes the caller when the command
 * is done
 *************************************/
int update_command_cb(struct pme_object *obj,
		       u32 result_flags,
		       u8 exception_code,
		       u64 stream_id, struct pme_callback *cb,
		       size_t output_used, struct pme_fbchain *fb_output)
{
	/* We stored a pointer to our local structure
	 * for later use */
	update_command_free((void *)cb->ctx.words[0]);
	return 0;
}

/**************************************
 * Executes the command specified by
 * local_ctx and then waits for the
 * command to complete
 **************************************/
int update_command_execute(struct pme_object *ctx, struct update_cmd_ctx *cmd)
{
	struct pme_callback cb_ctx;
	int flags = ctx->flags | PME_FLAG_UPDATE;

	if (!cmd->cmd_buf.size)
		return -EINVAL;

	/* Build a callback context
	 * We only need one word of context here the local_ctx will
	 * survive while we execute the command */
	cb_ctx.completion = update_command_cb;
	cb_ctx.ctx.words[0] = (u32)cmd;

	return pme_object_cmd(ctx, flags,
				 &cb_ctx, &cmd->cmd_buf, NULL, NULL);
}

int update_command_setup(void)
{
	update_cmd_cache = kmem_cache_create("pme_update_cmd_cache",
				sizeof(struct update_cmd_ctx), 0,
				SLAB_HWCACHE_ALIGN, NULL);
	if (!update_cmd_cache)
		return -ENOMEM;
	return 0;
}

void update_command_teardown(void)
{
	kmem_cache_destroy(update_cmd_cache);
}

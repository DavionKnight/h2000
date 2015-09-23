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
 * Author: Geoff Thorpe, Geoff.Thorpe@freescale.com
 *
 * Description:
 * This file implements channels for the pattern-matcher driver. It inlines
 * code from other files for the purposes of encapsulation but without losing
 * the optimisation of inlines and macros. Eg. FIFOs, buffer management,
 * virtual freelists, contexts, etc.
 */


#include "base_private.h"

/****************************/
/* Interrupt registers bits */
/****************************/

/* Error bits */
/* Freelist low-water thresholds */
#define CHANNEL_IR_FREELIST_A	(u32)(1 << 9)
#define CHANNEL_IR_FREELIST_B	(u32)(1 << 8)
/* FIFO work bits */
#define CHANNEL_IR_FIFO_FB	(u32)(1 << 4)
#define CHANNEL_IR_FIFO_NOT	(u32)(1 << 1)
#define CHANNEL_IR_FIFO_CMD	(u32)(1 << 0)

/* Combined masks */
#define CHANNEL_IR_FREELIST	(CHANNEL_IR_FREELIST_A | \
				CHANNEL_IR_FREELIST_B | \
				CHANNEL_IR_FIFO_FB)
#define CHANNEL_IR_FIFO		(CHANNEL_IR_FIFO_FB | \
				CHANNEL_IR_FIFO_NOT | \
				CHANNEL_IR_FIFO_CMD)
#define CHANNEL_IR_ERR		(~(CHANNEL_IR_FREELIST | \
				CHANNEL_IR_FIFO))

/* The interrupt-inhibit register (CH_II) is boolean, choose on/off values */
#define CH_II_INHIBIT		0xFFFFFFFF
#define CH_II_UNINHIBIT		0

/* Manipulations of FBL_BS_[A|B] */
static inline int FBL_BS_ENABLED(u32 v)
{ return ((v >> 26) & 1); }
static inline int FBL_BS_MASTER(u32 v)
{ return ((v >> 27) & 1); }
static inline int FBL_BS_SIZE(u32 v)
{ return (v & 0x0001fffe); }
static inline int FBL_BS_ONDECK(u32 v)
{ return (v & 0x1); }

/********************************/
/* Per-channel register offsets */
/********************************/

/* They're documented as byte offsets but we use u32 pointer additions */
#define CH_IS			(0x000 >> 2) /* <-- start of DMA registers */
#define CH_IE			(0x004 >> 2)
#define CH_ISD			(0x008 >> 2)
#define CH_II			(0x00c >> 2)
#define CH_NPI			(0x060 >> 2)
#define CH_CCI			(0x064 >> 2)
#define CH_FBFCI		(0x070 >> 2)
#define CH_FBERR		(0x074 >> 2)
#define CH_NCI			(0x080 >> 2)
#define CH_CPI			(0x084 >> 2)
#define CH_CEI			(0x088 >> 2)
#define CH_FBFPI		(0x090 >> 2)
#define CH_TRUNCIC		(0x0a0 >> 2)
#define CH_RBC			(0x0a4 >> 2)
#define CH_SCOS			(0x0a8 >> 2)
#define CH_CRC			(0x100 >> 2)
#define CH_CMD_FIFO_BASE_H	(0x120 >> 2)
#define CH_CMD_FIFO_BASE_L	(0x124 >> 2)
#define CH_CMD_FIFO_DEPTH	(0x128 >> 2)
#define CH_CMD_FIFO_THRESH	(0x12c >> 2)
#define CH_NOT_FIFO_BASE_H	(0x140 >> 2)
#define CH_NOT_FIFO_BASE_L	(0x144 >> 2)
#define CH_NOT_FIFO_DEPTH	(0x148 >> 2)
#define CH_NOT_FIFO_THRESH	(0x14c >> 2)
#define CH_NDLL			(0x150 >> 2)
#define CH_NRLL			(0x154 >> 2)
#define CH_FB_FIFO_BASE_H	(0x160 >> 2)
#define CH_FB_FIFO_BASE_L	(0x164 >> 2)
#define CH_FB_FIFO_DEPTH	(0x168 >> 2)
#define CH_FB_FIFO_THRESH	(0x16c >> 2)
#define CH_SC_CONFIG		(0x180 >> 2)
#define CH_SC_BASE_H		(0x184 >> 2)
#define CH_SC_BASE_L		(0x188 >> 2)
#define CH_RES_CONFIG		(0x1a0 >> 2)
#define CH_RES_BASE_H		(0x1a4 >> 2)
#define CH_RES_BASE_L		(0x1a8 >> 2)
#define CH_CAC			(0x1c0 >> 2)
#define CH_MTPC			(0x1c4 >> 2)
#define CH_FBL_THRESH_A		(0x400 >> 2) /* <-- start of FBM registers */
#define CH_FBL_HLWM_A		(0x404 >> 2)
#define CH_FBL_AD_A		(0x408 >> 2)
#define CH_FBL_ODD_A		(0x40c >> 2)
#define CH_FBL_PHH_A		(0x410 >> 2)
#define CH_FBL_PHL_A		(0x414 >> 2)
#define CH_FBL_VHH_A		(0x418 >> 2)
#define CH_FBL_VHL_A		(0x41c >> 2)
#define CH_FBL_PTH_A		(0x420 >> 2)
#define CH_FBL_PTL_A		(0x424 >> 2)
#define CH_FBL_NB_A		(0x430 >> 2)
#define CH_FBL_BS_A		(0x434 >> 2)
#define CH_FBL_VDH_A		(0x438 >> 2) /* blockguide uses "..._A[HL]" */
#define CH_FBL_VDL_A		(0x43c >> 2) /* we alter it for macro ease */
/* Channel FBL registers are mirrored A/B, via this offset
 * The offset needs to be shifted the same as the other offsets! */
#define CH_FBL_OFFSET_B		(0x40 >> 2)
#define __CH_FBL_FN(name) \
static inline u32 CH_FBL_##name(u8 freelist_id) { \
	return CH_FBL_##name##_A + (freelist_id ? CH_FBL_OFFSET_B : 0); \
}
__CH_FBL_FN(THRESH)
__CH_FBL_FN(AD)
__CH_FBL_FN(ODD)
__CH_FBL_FN(PHH)
__CH_FBL_FN(PHL)
__CH_FBL_FN(VHH)
__CH_FBL_FN(VHL)
__CH_FBL_FN(PTH)
__CH_FBL_FN(PTL)
__CH_FBL_FN(NB)
__CH_FBL_FN(BS)
__CH_FBL_FN(VDH)
__CH_FBL_FN(VDL)

/* The API declared PME_DMA_CHANNEL_FL_[A|B]_ENABLED, internally we also need
 * these two. */
#define PME_DMA_CHANNEL_FL_A_MASTER	(u32)0x00000002
#define PME_DMA_CHANNEL_FL_B_MASTER	(u32)0x00000020

/* Wrapper structures for FIFO entries */
struct fifo_cmd		{ u64 blk[128 >> 3]; };
struct fifo_notify	{ u64 blk[64 >> 3]; };
struct fifo_fbm		{ u64 blk[32 >> 3]; };

/*
 * These abstractions are included inline because this data-path logic ought to
 * be a single compiler object (with corresponding macroes/inlining where
 * appropriate). Also, this allows us to avoid over-dereferencing by declaring
 * structures statically - particularly when inlining is enabled, the compiler
 * should be able to do much better. Please preserve the abstractions and
 * respect the use of ordering to designate which interfaces are available to
 * which others.
 *
 * NB: the base_object.h code is included lower, after some predeclarations.
 */

/* Channel FIFO implementation */
struct channel_fifo;
#include "base_fifo.h"

/* Freebuffer manipulations */
struct fbuffer_slab;
#include "base_buffers.h"

/* Channel virtual freelist handling */
struct channel_fbmfifo;
struct channel_vfreelist;
struct channel_fbthread;
#include "base_vfreelist.h"

/* Tables for context/residue */
struct channel_table;
#include "base_table.h"

/* Predeclarations, required by pme_object */
static inline int channel_residue_alloc(struct pme_channel *,
					struct __pme_dma_resource *res,
					unsigned int gfp_flags);
static inline void channel_residue_free(struct pme_channel *,
					struct __pme_dma_resource *res);
static inline int channel_context_alloc(struct pme_channel *,
					struct __pme_dma_resource *res,
					unsigned int gfp_flags);
static inline void channel_context_free(struct pme_channel *,
					struct __pme_dma_resource *res);
static inline void *channel_cmd_start(struct pme_channel *,
				struct pme_object *, int *reduced,
				int blocking);
static inline void channel_cmd_finish(struct pme_channel *,
				struct pme_object *);
static inline void channel_cmd_revert(struct pme_channel *,
				struct pme_object *);
static inline int channel_topdog(struct pme_channel *,
				struct pme_object *, int);
static inline void channel_fb_owned(struct pme_channel *,
				struct pme_fbchain *chain);
static inline void channel_inputrelease_add(struct pme_channel *channel,
					struct pme_fbchain *chain);
static inline void channel_inputrelease_consume(struct pme_channel *,
				struct pme_fbchain *chain);
static inline void channel_inputrelease_abort(struct pme_channel *,
				struct pme_fbchain *chain);
static inline void channel_suspend(struct pme_channel *, int suspended);
static inline void channel_sub_tasklet(struct pme_channel *);

/* Objects and their commands/notifications */
struct pme_object;
#include "base_object.h"

/* The channel object */
struct pme_channel {
	/* 'alive' is a synchroniser to ensure we only *start* the kill once if
	 * an error and kill-command race. 'killed' is set once the killing is
	 * safely underway, ie. it's not safe to wait for reset (or indeed do a
	 * reset) until 'killed' is set, which occurs some time after 'alive'
	 * indicates that someone has won the race. */
	atomic_t alive;
	int killed;
	/* This lock is (*only*) used for 'table', 'num_objs', 'obj_list',
	 * 'access_list', 'topdog', and writes to the command FIFO. Ie.
	 * anything that assumes the channel is alive/not-killed but needs
	 * synchronisation anyway. */
	spinlock_t lock;
	/* This waitqueue is used for syncing, monitoring, and FIFO access */
	wait_queue_head_t queue;
	/* This is used to suspend the tasklet by the blocking interface */
	atomic_t bhsuspend;
	struct tasklet_struct tasklet;
	struct pme_regmap *reg;
	struct channel_fifo fifo[3];
	struct channel_fbthread fbthread;
	struct channel_fbmfifo fbmfifo;
	struct channel_vfreelist vfl[2];
	struct channel_table table[2];
	/* Total objects allocated from 'table's */
	unsigned int num_objs;
	/* We want to properly synchronise callers to kill/reset/monitor, yet
	 * the implementations might sleep so we can't hold spinlocks and we
	 * don't want to invent new state to work around the problem. */
	struct semaphore sem;
	/* State for the monitoring interface */
	u32 monitor_IS;
	u32 monitor_uid;
	/* Linked-list of channel-objects */
	struct list_head obj_list;
	/* A linked-list of objects waiting for command FIFO access - eg. the
	 * FIFO is full and/or a 'topdog' object has exclusivity. */
	struct list_head access_list;
	int access_list_waking;
	/* If a 'topdog' object has exclusivity, this is non-NULL. */
	void *topdog;
	/* Stats sleepers -> sleeper_list, when woken -> yawner_list */
	struct list_head sleeper_list, yawner_list;
	/* Triggers and counters for stats sleepers */
	u16 stat_cmd_trigger, stat_not_trigger, stat_cmd, stat_not;
	/* 0..3 */
	u8 idx;
};

/* A sleeper context for threads linked to pme_object::access. Objects
 * themselves are linked to pme_channel::access_list */
struct channel_sleeper {
	struct pme_object *obj;
	int woken;
	struct list_head list;
};

static irqreturn_t channel_isr(int irq, void *dev_id);
static void channel_tasklet(unsigned long __channel);

/**********************/
/* Internal functions */
/**********************/

/* Perform a reset of the channel register-space */
static int __channel_reset(struct pme_channel *c)
{
	might_sleep();
	/* Put the channel into reset */
	reg_set(c->reg, CH_CRC, 1);
	/* If the write had no effect, a CC error is probably asserted */
	if (!reg_get(c->reg, CH_CRC)) {
		printk(KERN_ERR PMMOD "channel %d reset blocked, CC error?\n",
				c->idx);
		return -EBUSY;
	}
	msleep(PME_DMA_CH_RESET_MS);
	/* FBL channel registers aren't zapped by reset, so we do so here so
	 * that subsequent logic can continue to assume they were ... */
	reg_set(c->reg, CH_FBL_ODD(0), 0);
	reg_set(c->reg, CH_FBL_ODD(1), 0);
	reg_set(c->reg, CH_FBL_THRESH(0), 0);
	reg_set(c->reg, CH_FBL_THRESH(1), 0);
	/* ... and read the read-reset regs too ... */
	reg_get(c->reg, CH_FBL_AD(0));
	reg_get(c->reg, CH_FBL_AD(1));
	/* ... and write-to-clear the corresponding overflow bits */
	reg_set(c->reg, CH_SCOS, PME_CHANNEL_ST_CA | PME_CHANNEL_ST_CB);
	/* Take the channel out of reset (but leave it disabled) */
	reg_set(c->reg, CH_CRC, 0);
	return 0;
}

/* Set the CH_CRC register - used by __channel_set() and __channel_kill() */
static inline void __channel_enable(struct pme_channel *c, int en)
{
	reg_set(c->reg, CH_CRC, (1 << 9) | (1 << 8) |	/* freelist A and B */
				(1 << 4) |		/* FB FIFO */
				((en ? 1 : 0) << 1) |	/* cmd FIFO */
				(0 << 0));		/* reset */
}

/* Set the channel registers - not including encapsulated structures, their
 * "set" functions are called separately. */
static void __channel_set(struct pme_channel *c)
{
	/**************************/
	/* Set up cache-awareness */
	reg_set(c->reg, CH_CAC,
		(PME_DMA_CH_SNOOP_CMD << 14) |
		(PME_DMA_CH_SNOOP_FB << 12) |
		(PME_DMA_CH_SNOOP_REPORT << 10) |
		(PME_DMA_CH_SNOOP_INPUT << 8) |
		(PME_DMA_CH_SNOOP_DEFLATE << 6) |
		(PME_DMA_CH_SNOOP_RESIDUE << 4) |
		(PME_DMA_CH_SNOOP_CONTEXT << 2) |
		(PME_DMA_CH_SNOOP_NOTIFY << 0));
	/****************************************/
	/* Set up memory transaction priorities */
	reg_set(c->reg, CH_MTPC,
		(PME_DMA_CH_MTP_DEFLATE << 28) |
		(PME_DMA_CH_MTP_REPORT << 24) |
		(PME_DMA_CH_MTP_INPUT << 20) |
		(PME_DMA_CH_MTP_RESIDUE << 16) |
		(PME_DMA_CH_MTP_CONTEXT << 12) |
		(PME_DMA_CH_MTP_CMD << 8) |
		(PME_DMA_CH_MTP_NOTIFY << 4) |
		(PME_DMA_CH_MTP_FB << 0));
	/* We shouldn't be disabling any interrupt bits */
	reg_set(c->reg, CH_ISD, 0);
	/* All IS bits can trigger interrupts */
	reg_set(c->reg, CH_IE, ~(u32)0);
	/******************/
	/* Enable channel */
	__channel_enable(c, 1);
}

/* This allows front ends (pme_channel_kill) or back ends (tasklet
 * error-handling) to put the channel in a "killed" state. Unlike some other
 * __channel_*** helpers, this one *does* deal with any channel encapsulations,
 * eg. killing vfreelists. 'in_error_interrupt' should be true iff we're in the
 * error path which is true iff we're in tasklet(/interrupt) context. */
static void __channel_kill(struct pme_channel *c, int in_error_interrupt)
{
	void *ptr;
	might_sleep_if(!in_error_interrupt);
	if (!atomic_dec_and_test(&c->alive)) {
		/* The channel was already killed, leave 'alive' as it was */
		atomic_inc(&c->alive);
		return;
	}
	/* We need to have and release the spinlock after setting 'killed'.
	 * Routines like cmd_start(), which check 'killed' while holding the
	 * spinlock, will therefore not race against any of the cleanups we do
	 * after releasing the lock. */
	spin_lock_bh(&c->lock);
	/* We're the first "killer"; */
	c->killed = 1;
	spin_unlock_bh(&c->lock);
	/* Disable cmd FIFO */
	__channel_enable(c, 0);
	if (!in_error_interrupt)
		/* We're not in the tasklet so we can sleep, and unlike the
		 * tasklet (error interrupt), we may *need* to sleep to be sure
		 * the DE is idle. */
		msleep(PME_DMA_CH_DISABLE_MS);
	channel_fifo_abandon(&c->fifo[FIFO_CMD], ptr,
		if (object_complete_cmd(c, ptr, 1, (c->fifo[FIFO_CMD].type ==
							fifo_cmd_reduced)))
			printk(KERN_ERR PMMOD "command abortion failed\n"););
	channel_vfreelist_kill(&c->vfl[0], &c->fbmfifo, &c->fbthread);
	channel_vfreelist_kill(&c->vfl[1], &c->fbmfifo, &c->fbthread);
	/* To avoid nasty side-effects of a race between setting killed=1, the
	 * deallocation of the final object, and someone starting a
	 * wait_event() for num_objs to hit zero, we issue a write-barrier
	 * prior to the explicit wake_up() further down. */
	wmb();
	/* Finally, dequeue and invoke any registered channel notifiers that
	 * the channel is "down". NB, this means any command abortions have
	 * already fired but it does not necessarily mean that all outstanding
	 * notifications (and thus completions) have been flushed out. */
	spin_lock_bh(&c->lock);
	while (!list_empty(&c->obj_list)) {
		struct pme_channel_err_ctx *item =
				list_entry(c->obj_list.next,
					struct pme_channel_err_ctx, list);
		item->notified = 1;
		list_del(&item->list);
		spin_unlock_bh(&c->lock);
		item->cb(item, 1);
		item->complete = 1;
		spin_lock_bh(&c->lock);
	}
	spin_unlock_bh(&c->lock);
	/* This addresses the aforementioned race possibility as well blocking
	 * deregistrations waiting for item->complete==1. */
	wake_up(&c->queue);
}

static inline int __channel_synced(struct pme_channel *c)
{
	if (!c->killed || c->num_objs || !list_empty(&c->obj_list) ||
			!channel_vfreelist_synced(&c->vfl[0]) ||
			!channel_vfreelist_synced(&c->vfl[1]))
		return 0;
	return 1;
}

/* Note: this is not quite right. The timeout is being applied multiple times
 * serially, so the effective timeout is a multiple of the specified value.
 * Syncing should be possible on the same waitqueue, we should look at putting
 * object wake-ups on the fbthread queue rather than having the channel with a
 * separate queue. However, the worst-case scenario (timeout is up to 3 times
 * longer than requested) is only possible if each wait succeeds before timing
 * out, but takes close enough to the timeout to create the additive effect.
 * It's unlikely an (comparitively) harmless, fixing this is a low-runner. */
static inline int __channel_wait(struct pme_channel *c, int user, u32 ms)
{
	int res;
	if (!c->killed)
		return -EBUSY;
	if (ms) {
		res = wait_event_interruptible_timeout(c->queue, !c->num_objs,
					msecs_to_jiffies(ms));
		if (res < 0)
			res = -EINTR;
		else if (!res)
			res = -ETIMEDOUT;
		else
			res = 0;
	} else {
		res = wait_event_interruptible(c->queue, !c->num_objs);
		if (res < 0)
			res = -EINTR;
	}
	if (!res)
		res = channel_vfreelist_wait(&c->vfl[0], &c->fbthread, 1, ms);
	if (!res)
		res = channel_vfreelist_wait(&c->vfl[1], &c->fbthread, 1, ms);
	return res;
}

/**********************************************************
 * Code used to initalize pattern matcher internal memories
 **********************************************************/
struct pmi_message {
	u8 version;
	u8 type;
	u16 reserved;
	u32 length;
	u64 message_id;
	u32 table_id;
} __attribute((packed))__;

#define one_byte_trigger_table		0x0
#define two_byte_trigger_table		0x1
#define vlt_trigger_table		0x2
#define confidence_table		0x3
#define pattern_desc_state_rule_table 	0x4
#define user_defined_group_map_table	0x5
#define equivalance_byte_map_table	0x6
#define sre_context_table		0x7
#define special_trigger_table		0x8

#define pmi_version			0x1
#define write_table 			0x1

struct pmi_message pmi_commands[] = {
	{
		.version = pmi_version,
		.type = write_table,
		.table_id = one_byte_trigger_table,
		.reserved = 1,
		.length = 32
	},
	{
		.version = pmi_version,
		.type = write_table,
		.table_id = two_byte_trigger_table,
		.reserved = 512,
		.length = 8
	},
	{
		.version = pmi_version,
		.type = write_table,
		.table_id = vlt_trigger_table,
		.reserved = 4096,
		.length = 8
	},
	{
		.version = pmi_version,
		.type = write_table,
		.table_id = special_trigger_table,
		.reserved = 1,
		.length = 32
	},
	{
		.version = pmi_version,
		.type = write_table,
		.table_id = equivalance_byte_map_table,
		.reserved = 1,
		.length = 256
	},
	{
		.version = pmi_version,
		.type = write_table,
		.table_id = user_defined_group_map_table,
		.reserved = 1,
		.length = 256
	}
};

#define pmi_msg_num	(sizeof(pmi_commands) / sizeof(struct pmi_message))

static const u32 control_context[] = {
	0x01040000, 0x01000000, 0x01010003, 0x00000000,
	0x01020004, 0x00000000, 0x01040001, 0x00000000,
	0x01000002, 0x00000000, 0x01000005, 0x00000000,
	0x01040006, 0x00000000, 0x01040007, 0x00000000,
	0x0104000a, 0x00000000, 0x0104000b, 0x00000000,
	0x01040008, 0x00000000, 0x01040009, 0x00000000,
	0x0101000c, 0x00000000, 0x01010100, 0x00000000
};

static int free_cb(struct pme_object *obj, u32 pme_completion_flags,
		u8 exception_code, u64 stream_id, struct pme_callback *cb,
		size_t output_used, struct pme_fbchain *fb_output)
{
	if (pme_completion_flags & PME_COMPLETION_COMMAND)
		kfree((void *)cb->ctx.words[0]);
	return 0;
}

static int init_internal_memories(struct pme_channel *c)
{
	int ret, i, j;
	void *input, *paligned, *curr;
	struct pme_object *obj;
	struct pme_data input_data;
	struct pme_callback free_callback = {
		.completion = free_cb
	};
	size_t size;

	ret = pme_object_alloc(&obj, c, GFP_KERNEL, NULL, NULL);
	if (ret)
		return ret;
	/* Send the stream context update to tag this as a control object */
	input = kmalloc(sizeof(control_context) + 8, GFP_KERNEL);
	if (!input)
		return -ENOMEM;
	free_callback.ctx.words[0] = (u32)input;
	paligned = (void *)(((unsigned long)input + 7) & ~(unsigned long)7);
	memcpy(paligned, control_context, sizeof(control_context));
	input_data.addr = DMA_MAP_SINGLE(paligned, sizeof(control_context),
					DMA_TO_DEVICE);
	input_data.size = input_data.length = sizeof(control_context);
	input_data.type = data_in_normal;
	input_data.flags = 0;
	ret = pme_object_cmd(obj, PME_FLAG_UPDATE, &free_callback, &input_data,
			NULL, NULL);
	if (ret)
		goto end;

	for (i = 0; i < pmi_msg_num; i++) {
		struct pmi_message *msg;
		size = (pmi_commands[i].length * pmi_commands[i].reserved) +
				(pmi_commands[i].reserved * sizeof(u32)) +
				sizeof(struct pmi_message);
		input = kmalloc(size + 8, GFP_KERNEL);
		if (!input)
			return -ENOMEM;
		free_callback.ctx.words[0] = (u32)input;
		/* Align the pointer */
		paligned = (void *)(((unsigned long)input + 7) &
				~(unsigned long)7);
		memcpy(paligned, &pmi_commands[i], sizeof(pmi_commands[i]));
		msg = paligned;
		msg->length *= pmi_commands[i].reserved;
		msg->reserved = 0;
		msg->length += sizeof(struct pmi_message) +
			(pmi_commands[i].reserved * sizeof(u32));
		curr = paligned + sizeof(pmi_commands[i]);
		for (j = 0; j < pmi_commands[i].reserved; j++) {
			memset(curr, 0, pmi_commands[i].length + sizeof(u32));
			*(u32 *)curr = j;
			curr += pmi_commands[i].length + sizeof(u32);
		}
		input_data.addr = DMA_MAP_SINGLE(paligned, size, DMA_TO_DEVICE);
		input_data.size = input_data.length = size;
		input_data.type = data_in_normal;
		input_data.flags = 0;
		ret = pme_object_cmd(obj, PME_FLAG_CB_COMMAND, &free_callback,
					&input_data, NULL, NULL);
		if (ret)
			goto end;
	}
	/* And finally, reset any stats that have been affected */
	reg_get(c->reg, CH_TRUNCIC);
	reg_get(c->reg, CH_RBC);
	reg_set(c->reg, CH_SCOS, PME_CHANNEL_ST_TRUNCIC | PME_CHANNEL_ST_RBC);
end:
	pme_object_free(obj);
	return ret;
}

/* Extra initialization step required for the deflate engine. It is an
 * initial deflate operation that primes the deflate internal memory
 * appropriately. It is required to be done prior to normal deflate
 * operation. Software does this as part of deflate engine initialization. */
struct __init_deflate {
	wait_queue_head_t queue;
	int ret, done;
};
static int __cb_init_deflate(struct pme_object *obj, u32 flags,
		u8 exception_code, u64 stream_id, struct pme_callback *cb,
		size_t output_used, struct pme_fbchain *fb_output)
{
	/* Completion callback - flag an IO error if anything looked wrong */
	struct __init_deflate *init_deflate_ptr = (void *)cb->ctx.words[0];
	if (exception_code || (flags & PME_COMPLETION_ABORTED))
		init_deflate_ptr->ret = -EIO;
	init_deflate_ptr->done = 1;
	wake_up(&init_deflate_ptr->queue);
	return 0;
}
static void __dtor_init_deflate(void *userctx, enum pme_dtor reason)
{
	/* Destructor - only free during object destruction, in case a channel
	 * error had also arrived. */
	if (reason == pme_dtor_object)
		kfree(userctx);
}
/* This is the data needed to perform a stream context update to set up the
 * DEFLATE_RECOVER_SCAN activity code */
static const u32 __hex_init_deflate[] = {
	0x01040000, 0x0100000b, 0x01010003, 0x00000000,
	0x01020004, 0x00000000, 0x01040001, 0x00000000,
	0x01000002, 0x00000000, 0x01000005, 0x00000000,
	0x01040006, 0x00000000, 0x01040007, 0x00000001,
	0x0104000a, 0x00000000, 0x0104000b, 0x00000000,
	0x01040008, 0x00000000, 0x01040009, 0x00000000,
	0x0101000c, 0x00000000, 0x01010100, 0x00000000
};
static int __init_deflate(struct pme_channel *c)
{
	int ret;
	unsigned char *input, *paligned;
	struct pme_object *obj;
	struct pme_data input_data;
	struct pme_data output_data = {
		.type = data_out_normal,
		.size = 0,
		.addr = 0
	};
	size_t size = 32768 + 5; /* Deflate header is 5 bytes*/
	struct __init_deflate init_deflate = {
		.queue = __WAIT_QUEUE_HEAD_INITIALIZER(init_deflate.queue)
	};
	struct pme_callback deflate_cb = {
		.completion = __cb_init_deflate,
		.ctx = {
			.words = { (u32)&init_deflate }
		}
	};
	input = kmalloc(size, GFP_KERNEL);
	if (!input)
		return -ENOMEM;
	ret = pme_object_alloc(&obj, c, GFP_KERNEL, input, __dtor_init_deflate);
	if (ret) {
		kfree(input);
		return ret;
	}
	/* "UPDATE" the context for deflate-recover */
	paligned = (void *)(((unsigned long)input + 7) & ~(unsigned long)7);
	memcpy(paligned, __hex_init_deflate, sizeof(__hex_init_deflate));
	input_data.addr = DMA_MAP_SINGLE(paligned, sizeof(__hex_init_deflate),
					DMA_TO_DEVICE);
	input_data.size = input_data.length = sizeof(__hex_init_deflate);
	input_data.type = data_in_normal;
	input_data.flags = 0;
	init_deflate.ret = init_deflate.done = 0;
	ret = pme_object_cmd(obj, PME_FLAG_UPDATE, &deflate_cb,
					&input_data, NULL, NULL);
	if (ret)
		goto end;
	wait_event(init_deflate.queue, init_deflate.done);
	ret = init_deflate.ret;
	if (ret)
		goto end;
	/* Run a special-sauce deflate work unit through */
	input[0] = 0x01;
	input[1] = 0x00;
	input[2] = 0x80;
	input[3] = 0xff;
	input[4] = 0x7f;
	input_data.addr = DMA_MAP_SINGLE(input, size, DMA_TO_DEVICE);
	input_data.size = input_data.length = size;
	input_data.type = data_in_normal;
	input_data.flags = 0;
	init_deflate.ret = init_deflate.done = 0;
	ret = pme_object_cmd(obj, PME_FLAG_CB_DEFLATE, &deflate_cb,
					&input_data, &output_data, NULL);
	if (ret)
		goto end;
	wait_event(init_deflate.queue, init_deflate.done);
	ret = init_deflate.ret;
	/* And finally, reset any stats that are affected by this activity */
	reg_get(c->reg, CH_TRUNCIC);
	reg_get(c->reg, CH_RBC);
	reg_set(c->reg, CH_SCOS, PME_CHANNEL_ST_TRUNCIC |
				PME_CHANNEL_ST_RBC);
end:
	pme_object_free(obj);
	return ret;
}

/**************************************/
/* Internals required by inlined code */
/**************************************/
/* Ie. "exported" but the compiler doesn't know about it. */

/* Factor out the commonality between residue_alloc and context_alloc */
static inline int __channel_alloc_obj(struct pme_channel *c,
				struct __pme_dma_resource *res,
				unsigned int table_idx,
				unsigned int gfp_flags)
{
	int err;
	spin_lock_bh(&c->lock);
	/* 'killed' can be changed at any time. We check it before and after
	 * incrementing num_objs, such that if we pass those initial tests - we
	 * know that there is no window of opportunity during which someone
	 * else sets 'kill' and notices num_objs drop to zero (num_objs can't
	 * be changed outside the lock). The fact we can't allocate with the
	 * lock held just complicates things because if 'killed' is set,
	 * someone could be waiting for the num_objs==0 wake-up to *unload*
	 * us!! Ie. we can't just allocate outside the lock in advance becase
	 * we may get burnt. This twisting and turning is to avoid fascist
	 * locking around 'killed' checks in the fast path (where 'killed' is
	 * essentially always zero anyway). */
	if (c->killed)
		goto err;
	c->num_objs++;
	if (c->killed)
		goto err_decrement;
	spin_unlock_bh(&c->lock);
	/* At this point we're welcome to succeed allocating, even if 'killed'
	 * is set, 'killed' wasn't set while num_objs was zero. However, if
	 * 'killed' is set once we get the lock back - we still need to bail
	 * out cleanly. */
	err = channel_table_alloc(&c->table[table_idx], res, gfp_flags);
	spin_lock_bh(&c->lock);
	if (err)
		goto err_decrement;
	if (c->killed) {
		channel_table_free(&c->table[table_idx], res);
		goto err_decrement;
	}
	spin_unlock_bh(&c->lock);
	return 0;
err_decrement:
	if (!(--c->num_objs) && c->killed)
		wake_up(&c->queue);
err:
	spin_unlock_bh(&c->lock);
	return -ENODEV;
}
static inline void __channel_free_obj(struct pme_channel *c,
				struct __pme_dma_resource *res,
				unsigned int table_idx)
{
	spin_lock_bh(&c->lock);
	channel_table_free(&c->table[table_idx], res);
	if (!(--c->num_objs) && c->killed)
		wake_up(&c->queue);
	spin_unlock_bh(&c->lock);
}

static inline int channel_residue_alloc(struct pme_channel *c,
					struct __pme_dma_resource *res,
					unsigned int gfp_flags)
{
	return __channel_alloc_obj(c, res, TABLE_RESIDUE, gfp_flags);
}
static inline void channel_residue_free(struct pme_channel *c,
					struct __pme_dma_resource *res)
{
	__channel_free_obj(c, res, TABLE_RESIDUE);
}
static inline int channel_context_alloc(struct pme_channel *c,
					struct __pme_dma_resource *res,
					unsigned int gfp_flags)
{
	return __channel_alloc_obj(c, res, TABLE_CONTEXT, gfp_flags);
}
static inline void channel_context_free(struct pme_channel *c,
					struct __pme_dma_resource *res)
{
	__channel_free_obj(c, res, TABLE_CONTEXT);
}

/* Called when 'obj' has just had a sleeper removed; (a) another sleeper (if
 * any) has to be woken, and (b) 'obj' may have to be relinked. */
static inline void __cmd_next(struct pme_channel *c)
{
	struct pme_object *obj;
	struct channel_sleeper *s;
	/* We stop "waking" if;
	 *   (a) the access list is empty, or
	 *   (b) the command FIFO is full, or
	 *   (c) first item in the access list is not the topdog and
	 *       (c-ii) the channel has a topdog
	 */
	if (list_empty(&c->access_list) ||
			!channel_fifo_room(&c->fifo[FIFO_CMD]) ||
			(((obj = list_entry(c->access_list.next,
			struct pme_object, access)) != c->topdog) &&
				c->topdog)) {
		c->access_list_waking = 0;
		return;
	}
	s = list_entry(obj->sleepers.next, struct channel_sleeper, list);
	s->woken = 1;
	c->access_list_waking = 1;
	wake_up(&c->queue);
}

/* We have to handle sleep logic for various scenarios.
 * Our object may already have sleepers waiting to be woken,
 * in which case we need to sleep no matter what. The FIFO may be full, in
 * which case we need to sleep no matter what. There may be a topdog object
 * (not us), in which case we need to sleep no matter what. We may be the
 * topdog object, in which case we need to sleep if the FIFO is full or, as
 * before, our object already has sleepers. If we sleep, we're only woken when
 * we can legimately begin a command or the channel is being killed, anything
 * else is a bug. Also, if objects are waiting in the 'access_list' and there
 * is a topdog object with sleepers for it, that object is always at the head
 * of the 'access_list'. When cmd_finish() is called, he checks if there were
 * other sleepers for the same object. If so, then if (a) the object is
 * 'topdog', he wakes the next sleeper, or (b) the object isn't 'topdog', he
 * relinks the object to the tail of the 'access_list' and wakes the first
 * sleeper on the object now at the head of 'access_list'. If there are no
 * other sleepers for the same object, the object is removed from the
 * 'access_list' and the first sleeper for the next is woken. */
static inline void *channel_cmd_start(struct pme_channel *c,
				struct pme_object *obj, int *reduced,
				int blocking)
{
	struct channel_sleeper sleeper = {
		.obj = obj,
		.woken = 0
	};
	int obj_empty, fifo_room;
	*reduced = (c->fifo[FIFO_CMD].type == fifo_cmd_reduced);
	spin_lock_bh(&c->lock);
	/* We check 'killed' within the lock, after which we're safe. */
	if (c->killed) {
		spin_unlock_bh(&c->lock);
		*reduced = -EIO;
		return NULL;
	}
	obj_empty = list_empty(&obj->sleepers);
	fifo_room = channel_fifo_room(&c->fifo[FIFO_CMD]);
	/* Check for fast-path first */
	if (likely(obj_empty && fifo_room && ((c->topdog == obj) ||
			(!c->topdog && list_empty(&c->access_list)))))
		goto done;
	if (!blocking) {
		spin_unlock_bh(&c->lock);
		*reduced = -EBUSY;
		return NULL;
	}
	might_sleep();
	/* Anything else involves sleeping, for one reason or another. */
	list_add_tail(&sleeper.list, &obj->sleepers);
	/* First, if we are the only sleeper on this object, add it to the
	 * access_list appropriately. */
	if (obj_empty) {
		/* Only go to the front if we're the topdog */
		if (c->topdog == obj)
			list_add(&obj->access, &c->access_list);
		else
			list_add_tail(&obj->access, &c->access_list);
	}
	spin_unlock_bh(&c->lock);
	wait_event_interruptible(c->queue, sleeper.woken);
	spin_lock_bh(&c->lock);
	list_del(&sleeper.list);
	if (list_empty(&obj->sleepers))
		/* No more sleepers for this object, unlink it */
		list_del(&obj->access);
	else if (!obj->topdog) {
		/* More sleepers for this object, but we're not 'topdog' so we
		 * should reschedule to the end of the list. */
		list_del(&obj->access);
		list_add_tail(&obj->access, &c->access_list);
	}
	/* If we weren't 'woken', we were interrupted - the other sleepers get
	 * woken in an ordered manner so don't interfere. */
	if (unlikely(!sleeper.woken)) {
		/* So we do the normal thing of "pass it on". */
		__cmd_next(c);
		spin_unlock_bh(&c->lock);
		*reduced = -EINTR;
		return NULL;
	}
done:
	/* Return a writable FIFO pointer and don't release 'lock' */
	return channel_fifo_produce_ptr(&c->fifo[FIFO_CMD]);
}

static inline void channel_cmd_finish(struct pme_channel *c,
				struct pme_object *obj)
{
	channel_fifo_produced(&c->fifo[FIFO_CMD], c->reg);
	__cmd_next(c);
	spin_unlock_bh(&c->lock);
}

static inline void channel_cmd_revert(struct pme_channel *c,
				struct pme_object *obj)
{
	__cmd_next(c);
	spin_unlock_bh(&c->lock);
}

/* We cheat by stealing the cmd_start() implementation. We request as an object
 * without 'topdog' status and when we're given access to the FIFO (with lock
 * held), we take 'topdog' status and revert the command. :-) */
static inline int channel_topdog(struct pme_channel *c,
			struct pme_object *obj, int take)
{
	int unused;
	might_sleep();
	if (!channel_cmd_start(c, obj, &unused, 1)) {
		/* If it failed because we were trying to release 'topdog' and
		 * the channel has been killed *and* it was otherwise valid,
		 * don't return an error. */
		if (!take && obj->topdog && (c->topdog == obj) && c->killed) {
			c->topdog = NULL;
			obj->topdog = 0;
			return 0;
		}
		return -ENODEV;
	}
	/* We hold the lock and go to the head of the 'access_list' */
	if (take) {
		c->topdog = obj;
		obj->topdog = 1;
	} else {
		c->topdog = NULL;
		obj->topdog = 0;
	}
	channel_cmd_revert(c, obj);
	return 0;
}

static inline void channel_fb_owned(struct pme_channel *channel,
				struct pme_fbchain *chain)
{
	channel_vfreelist_fb_owned(&channel->vfl[chain->freelist_id],
					&channel->fbthread, chain);
}

static inline void channel_inputrelease_add(struct pme_channel *channel,
					struct pme_fbchain *chain)
{
	channel_vfreelist_inputrelease_add(&channel->vfl[chain->freelist_id],
						&channel->fbthread, chain);
}

static inline void channel_inputrelease_consume(struct pme_channel *channel,
					struct pme_fbchain *chain)
{
	channel_vfreelist_inputrelease_consume(
			&channel->vfl[chain->freelist_id],
			&channel->fbmfifo, &channel->fbthread, chain);
}

static inline void channel_inputrelease_abort(struct pme_channel *channel,
					struct pme_fbchain *chain)
{
	channel_vfreelist_inputrelease_abort(&channel->vfl[chain->freelist_id],
						&channel->fbthread, chain);
}

/******************************/
/* Private pme_base.h functions */
/******************************/

int pme_channel_init(struct pme_channel **c, struct pme_ctrl_load *p)
{
	int err;
	struct pme_channel *ret = kmalloc(sizeof(struct pme_channel),
						GFP_KERNEL);
	if (!ret)
		return -ENOMEM;
	memset(ret, 0, sizeof(*ret));
	ret->idx = p->channel;
	/* Our initial state is that we need a reset to kickstart */
	atomic_set(&ret->alive, 0);
	ret->killed = 1;
	spin_lock_init(&ret->lock);
	init_waitqueue_head(&ret->queue);
	atomic_set(&ret->bhsuspend, 1);
	tasklet_init(&ret->tasklet, channel_tasklet, (unsigned long)ret);
	ret->num_objs = 0;
	sema_init(&ret->sem, 1);
	/* Channel initialisation is followed by an internal reset prior to the
	 * channel being visible. We want the reset counters to start at zero
	 * so we stack the deck in our favour. */
	ret->monitor_uid = (u32)-1;
	INIT_LIST_HEAD(&ret->obj_list);
	INIT_LIST_HEAD(&ret->access_list);
	ret->access_list_waking = 0;
	ret->topdog = NULL;
	INIT_LIST_HEAD(&ret->sleeper_list);
	INIT_LIST_HEAD(&ret->yawner_list);
	ret->stat_cmd_trigger = p->cmd_trigger;
	ret->stat_not_trigger = p->not_trigger;
	ret->stat_cmd = ret->stat_not = 0;
	err = channel_table_init(&ret->table[TABLE_RESIDUE], ret, "residue",
				ret->idx, p->residue_size, p->residue_table);
	if (err) {
		printk(KERN_ERR PMMOD "residue for channel %d failed\n",
				ret->idx);
		goto err_residue;
	}
	err = channel_table_init(&ret->table[TABLE_CONTEXT], ret, "context",
			ret->idx, PME_DMA_CONTEXT_SIZE, p->context_table);
	if (err) {
		printk(KERN_ERR PMMOD "context for channel %d failed\n",
				ret->idx);
		goto err_context;
	}
	ret->reg = pme_regmap_map(1 + ret->idx);
	if (!ret->reg) {
		printk(KERN_ERR PMMOD "map for channel %d failed\n", ret->idx);
		err = -ENODEV;
		goto err_map;
	}
	err = channel_fifo_init(&ret->fifo[FIFO_FBM], fifo_fbm,
					p->fb_fifo);
	if (err) {
		printk(KERN_ERR PMMOD "FBM FIFO for channel %d failed\n",
				ret->idx);
		goto err_fifo_fbm;
	}
	err = channel_fifo_init(&ret->fifo[FIFO_CMD], (p->fifo_reduced ?
					fifo_cmd_reduced : fifo_cmd_normal),
					p->cmd_fifo);
	if (err) {
		printk(KERN_ERR PMMOD "CMD FIFO for channel %d failed\n",
				ret->idx);
		goto err_fifo_cmd;
	}
	err = channel_fifo_init(&ret->fifo[FIFO_NOT], fifo_notify,
					p->not_fifo);
	if (err) {
		printk(KERN_ERR PMMOD "NOT FIFO for channel %d failed\n",
				ret->idx);
		goto err_fifo_not;
	}
	err = REQUEST_IRQ(PME_CH2IRQ(ret->idx), channel_isr,
					IRQF_DISABLED, "pme_channel", ret);
	if (err) {
		printk(KERN_ERR PMMOD "IRQ for channel %d failed\n", ret->idx);
		goto err_irq;
	}
	err = channel_fbthread_start(&ret->fbthread, ret->idx);
	if (err) {
		printk(KERN_ERR PMMOD "fbthread for channel %d failed\n",
				ret->idx);
		goto err_fbthread;
	}
	channel_fbmfifo_init(&ret->fbmfifo, &ret->fifo[FIFO_FBM],
				&ret->fbthread, ret, ret->reg);
	err = channel_vfreelist_init(&ret->vfl[0], &ret->fbmfifo,
				&ret->fbthread, &p->fb[0], ret->idx, 0);
	if (err) {
		printk(KERN_ERR PMMOD "vfl[0] for channel %d failed\n",
				ret->idx);
		goto err_vfl0;
	}
	err = channel_vfreelist_init(&ret->vfl[1], &ret->fbmfifo,
				&ret->fbthread, &p->fb[1], ret->idx, 1);
	if (err) {
		printk(KERN_ERR PMMOD "vfl[1] for channel %d failed\n",
				ret->idx);
		goto err_vfl1;
	}
	reg_set(ret->reg, CH_NDLL, p->limit_deflate_blks);
	reg_set(ret->reg, CH_NRLL, p->limit_report_blks);
	*c = ret;
	return 0;
err_vfl1:
	channel_vfreelist_kill(&ret->vfl[0], &ret->fbmfifo, &ret->fbthread);
	channel_vfreelist_wait(&ret->vfl[0], &ret->fbthread, 0, 0);
	channel_vfreelist_finish(&ret->vfl[0], &ret->fbmfifo, &ret->fbthread);
err_vfl0:
	channel_fbmfifo_finish(&ret->fbmfifo, &ret->fbthread);
	channel_fbthread_end(&ret->fbthread);
err_fbthread:
	FREE_IRQ(PME_CH2IRQ(ret->idx), ret);
err_irq:
	channel_fifo_finish(&ret->fifo[FIFO_NOT]);
err_fifo_not:
	channel_fifo_finish(&ret->fifo[FIFO_CMD]);
err_fifo_cmd:
	channel_fifo_finish(&ret->fifo[FIFO_FBM]);
err_fifo_fbm:
	pme_regmap_unmap(ret->reg);
err_map:
	channel_table_finish(&ret->table[TABLE_CONTEXT]);
err_context:
	channel_table_finish(&ret->table[TABLE_RESIDUE]);
err_residue:
	tasklet_kill(&ret->tasklet);
	kfree(ret);
	return err;
}

int pme_channel_finish(struct pme_channel *c)
{
	if (!__channel_synced(c))
		return -EBUSY;
	channel_vfreelist_finish(&c->vfl[0], &c->fbmfifo, &c->fbthread);
	channel_vfreelist_finish(&c->vfl[1], &c->fbmfifo, &c->fbthread);
	/* We're idle, disable the command FIFO and turn off all interrupts */
	__channel_enable(c, 0);
	reg_set(c->reg, CH_ISD, ~(u32)0);
	FREE_IRQ(PME_CH2IRQ(c->idx), c);
	tasklet_kill(&c->tasklet);
	channel_fbmfifo_finish(&c->fbmfifo, &c->fbthread);
	channel_fbthread_end(&c->fbthread);
	channel_fifo_finish(&c->fifo[FIFO_NOT]);
	channel_fifo_finish(&c->fifo[FIFO_CMD]);
	channel_fifo_finish(&c->fifo[FIFO_FBM]);
	pme_regmap_unmap(c->reg);
	channel_table_finish(&c->table[TABLE_CONTEXT]);
	channel_table_finish(&c->table[TABLE_RESIDUE]);
	kfree(c);
	return 0;
}

static inline int __ioctl_dump_channel(struct pme_channel *c, void *a)
{
	int err = 0;
	struct list_head *l;
	struct pme_dump_channel cmd;
	if (copy_from_user(&cmd, a, sizeof(cmd)))
		return -EFAULT;
	down(&c->sem);
	spin_lock_bh(&c->lock);
	cmd.idx = c->idx;
	cmd.killed = c->killed;
	channel_fifo_dump(&c->fifo[FIFO_FBM], &cmd.fifos[FIFO_FBM]);
	channel_fifo_dump(&c->fifo[FIFO_CMD], &cmd.fifos[FIFO_CMD]);
	channel_fifo_dump(&c->fifo[FIFO_NOT], &cmd.fifos[FIFO_NOT]);
	channel_fbmfifo_dump(&c->fbmfifo, &c->fbthread, &cmd.fbm);
	channel_vfreelist_dump_admin(&c->vfl[0], &cmd.vfl[0]);
	channel_vfreelist_dump_admin(&c->vfl[1], &cmd.vfl[1]);
	channel_table_dump(&c->table[TABLE_RESIDUE], &cmd.table[TABLE_RESIDUE]);
	channel_table_dump(&c->table[TABLE_CONTEXT], &cmd.table[TABLE_CONTEXT]);
	cmd.num_objs = c->num_objs;
	cmd.monitor_IS = c->monitor_IS;
	cmd.monitor_uid = c->monitor_uid;
	cmd.num_to_notify = cmd.num_access = cmd.num_sleeper =
		cmd.num_yawner = 0;
	list_for_each(l, &c->obj_list) cmd.num_to_notify++;
	list_for_each(l, &c->access_list) cmd.num_access++;
	list_for_each(l, &c->sleeper_list) cmd.num_sleeper++;
	list_for_each(l, &c->yawner_list) cmd.num_yawner++;
	cmd.access_waking = c->access_list_waking;
	cmd.topdog = (c->topdog ? 1 : 0);
	cmd.stat_cmd_trigger = c->stat_cmd_trigger;
	cmd.stat_not_trigger = c->stat_not_trigger;
	spin_unlock_bh(&c->lock);
	up(&c->sem);
	if (copy_to_user(a, &cmd, sizeof(cmd)))
		err = -EFAULT;
	return err;
}

static inline int __ioctl_dump_freelist(struct pme_channel *channel, void *a)
{
	int err;
	struct pme_dump_freelist cmd;
	if (copy_from_user(&cmd, a, sizeof(cmd)))
		return -EFAULT;
	if (cmd.freelist_id > 1)
		return -ERANGE;
	down(&channel->sem);
	err = channel_vfreelist_dump(&channel->vfl[cmd.freelist_id],
			&channel->fbmfifo, &channel->fbthread, &cmd);
	up(&channel->sem);
	if (!err && copy_to_user(a, &cmd, sizeof(cmd)))
		err = -EFAULT;
	return err;
}

/* Macro to encapsulate the condition used in wait_event_*** for the monitoring
 * ioctl. It is true if an un-ignored IS error bit has been set or a reset
 * occured since we released the semaphore */
#define monitor_condition(c, p, u) \
	((c->monitor_IS & ~(p->ignore_mask)) || (c->monitor_uid != u))
static inline int __ioctl_monitor(struct pme_channel *c, void *a)
{
	int res = 0;
	struct pme_channel_monitor cmd;
	struct pme_monitor_poll *p;
	if (copy_from_user(&cmd, a, sizeof(cmd)))
		return -EFAULT;
	p = &cmd.monitor;
	down(&c->sem);
	if (c->monitor_uid != cmd.reset_counter) {
		cmd.reset_counter = c->monitor_uid;
		up(&c->sem);
		if (copy_to_user(a, &cmd, sizeof(cmd)))
			return -EFAULT;
		return 0;
	}
	/* Do our IS/ISD manipulations while holding the mutex - these are safe
	 * against the ISR and tasklet but not against reset or other callers
	 * of this ioctl. */
	if (p->disable_mask) {
		reg_set(c->reg, CH_ISD, reg_get(c->reg, CH_ISD) |
						p->disable_mask);
		c->monitor_IS &= ~(p->disable_mask);
	}
	if (p->clear_mask | p->disable_mask) {
		c->monitor_IS &= ~p->clear_mask;
		/* Clear the monitor_IS value *first*, because write-to-clear
		 * of CH_IS may fire an interrupt right away if an error bit
		 * reasserts */
		smp_mb();
		reg_set(c->reg, CH_IS, p->clear_mask | p->disable_mask);
	}
	up(&c->sem);
	if (p->block) {
		if (p->timeout_ms) {
			res = wait_event_interruptible_timeout(c->queue,
				monitor_condition(c, p, cmd.reset_counter),
				msecs_to_jiffies(p->timeout_ms));
			if (res > 0)
				res = 0;
			else if (!res)
				res = 1;
		} else
			res = wait_event_interruptible(c->queue,
				monitor_condition(c, p, cmd.reset_counter));
		if (res < 0)
			res = -EINTR;
	}
	p->status = c->monitor_IS;
	cmd.error_code = reg_get(c->reg, CH_FBERR);
	cmd.reset_counter = c->monitor_uid;
	if (copy_to_user(a, &cmd, sizeof(cmd)))
		return -EFAULT;
	return res;
}

static inline int __ioctl_block(struct pme_channel *c, int cmd, void *a)
{
	int e;
	if (copy_from_user(&e, a, sizeof(e)))
		return -EFAULT;
	switch (cmd) {
	case PME_CHANNEL_IOCTL_CPI_BLOCK:
		channel_fifo_block(&c->fifo[FIFO_CMD], e, c->reg);
		break;
	case PME_CHANNEL_IOCTL_NCI_BLOCK:
		channel_fifo_block(&c->fifo[FIFO_NOT], e, c->reg);
		break;
	case PME_CHANNEL_IOCTL_FPI_BLOCK:
		channel_fifo_block(&c->fifo[FIFO_FBM], e, c->reg);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static inline int __ioctl_stats(struct pme_channel *c, void *a)
{
	struct pme_channel_stats stats;
	if (copy_from_user(&stats, a, sizeof(stats)))
		return -EFAULT;
	if (stats.flags & PME_CHANNEL_ST_TRUNCIC)
		stats.truncic = reg_get(c->reg, CH_TRUNCIC);
	if (stats.flags & PME_CHANNEL_ST_RBC)
		stats.rbc = reg_get(c->reg, CH_RBC);
	if (stats.flags & PME_CHANNEL_ST_CA) {
		u32 val = reg_get(c->reg, CH_FBL_AD(0));
		stats.ca_acnt = val >> 16;
		stats.ca_dcnt = val & 0xffff;
	}
	if (stats.flags & PME_CHANNEL_ST_CB) {
		u32 val = reg_get(c->reg, CH_FBL_AD(1));
		stats.cb_acnt = val >> 16;
		stats.cb_dcnt = val & 0xffff;
	}
	stats.overflow = reg_get(c->reg, CH_SCOS) & stats.flags;
	/* Write-to-clear the overflow bits */
	reg_set(c->reg, CH_SCOS, stats.overflow);
	if (copy_to_user(a, &stats, sizeof(stats)))
		return -EFAULT;
	return 0;
}

static inline int __ioctl_sleep(struct pme_channel *c, void *a)
{
	u32 ms;
	struct stat_sleeper sleeper;
	if (copy_from_user(&ms, a, sizeof(ms)))
		return -EFAULT;
	INIT_LIST_HEAD(&sleeper.list);
	init_waitqueue_head(&sleeper.queue);
	sleeper.woken = 0;
	spin_lock_bh(&c->lock);
	list_add_tail(&sleeper.list, &c->sleeper_list);
	spin_unlock_bh(&c->lock);
	if (ms)
		wait_event_interruptible_timeout(sleeper.queue,
			sleeper.woken != 0, msecs_to_jiffies(ms));
	else
		wait_event_interruptible(sleeper.queue, sleeper.woken != 0);
	spin_lock_bh(&c->lock);
	list_del(&sleeper.list);
	spin_unlock_bh(&c->lock);
	if (signal_pending(current))
		return -EINTR;
	return 0;
}

static int __ioctl_wait(struct pme_channel *c, void *arg)
{
	u32 ms;
	if (copy_from_user(&ms, arg, sizeof(ms)))
		return -EFAULT;
	return __channel_wait(c, 1, ms);
}

int pme_channel_ioctl(struct pme_channel *channel, int cmd,
			unsigned long arg)
{
	switch (cmd) {
	case PME_CHANNEL_IOCTL_DUMP:
		return __ioctl_dump_channel(channel, (void *)arg);
	case PME_CHANNEL_IOCTL_DUMP_FREELIST:
		return __ioctl_dump_freelist(channel, (void *)arg);
	case PME_CHANNEL_IOCTL_STATS:
		return __ioctl_stats(channel, (void *)arg);
	case PME_CHANNEL_IOCTL_SLEEP:
		return __ioctl_sleep(channel, (void *)arg);
	case PME_CHANNEL_IOCTL_WAIT:
		return __ioctl_wait(channel, (void *)arg);
	case PME_CHANNEL_IOCTL_MONITOR:
		return __ioctl_monitor(channel, (void *)arg);
	case PME_CHANNEL_IOCTL_CPI_BLOCK:
	case PME_CHANNEL_IOCTL_NCI_BLOCK:
	case PME_CHANNEL_IOCTL_FPI_BLOCK:
		return __ioctl_block(channel, cmd, (void *)arg);
	}
	return -EINVAL;
}

int pme_channel_kill(struct pme_channel *c, int block)
{
	int res = 0;
	down(&c->sem);
	__channel_kill(c, 0);
	if (block)
		res = __channel_wait(c, 0, 0);
	up(&c->sem);
	return res;
}
EXPORT_SYMBOL(pme_channel_kill);

int pme_channel_reset(struct pme_channel *c, int block)
{
	static atomic_t do_once = ATOMIC_INIT(1);
	int res;
	down(&c->sem);
	if (!c->killed) {
		res = -EBUSY;
		goto end;
	}
	if (block) {
		res = __channel_wait(c, 0, 0);
		if (res)
			goto end;
	}
	if (!__channel_synced(c)) {
		res = -EBUSY;
		goto end;
	}
	res = __channel_reset(c);
	if (res)
		goto end;
	__channel_set(c);
	channel_fifo_set(&c->fifo[FIFO_FBM], c->reg);
	channel_fifo_set(&c->fifo[FIFO_CMD], c->reg);
	channel_fifo_set(&c->fifo[FIFO_NOT], c->reg);
	channel_table_set(&c->table[TABLE_CONTEXT], c->reg, CH_SC_CONFIG);
	channel_table_set(&c->table[TABLE_RESIDUE], c->reg, CH_RES_CONFIG);
	if (channel_vfreelist_reset(&c->vfl[0], &c->fbmfifo, &c->fbthread) ||
			channel_vfreelist_reset(&c->vfl[1], &c->fbmfifo,
						&c->fbthread)) {
		printk(KERN_ERR PMMOD "BUG, synced reset should never fail\n");
		res = -EBUSY;
		goto end;
	}
	atomic_inc(&c->alive);
	c->killed = 0;
	/* Clear stale error bits from the monitoring interface, up the reset
	 * counter and wake any monitors. */
	c->monitor_IS = 0;
	c->monitor_uid++;
	wake_up(&c->queue);

	/* Only initialize memories on the first channel load */
	if (!atomic_dec_and_test(&do_once)) {
		atomic_inc(&do_once);
		goto end;
	}
	/* Initialize the internal pattern matcher memories */
	res = init_internal_memories(c);
	if (res)
		goto end;
	/* Extra initialization step required for the deflate engine. It is an
	 * initial deflate operation that primes the deflate internal memory
	 * appropriately. It is required to be done prior to normal deflate
	 * operation. Software does this as part of deflate engine
	 * initialization. */
	res = __init_deflate(c);
end:
	up(&c->sem);
	return res;
}
EXPORT_SYMBOL(pme_channel_reset);

u32 pme_channel_freelists(struct pme_channel *c)
{
	return (c->vfl[0].enabled ? PME_DMA_CHANNEL_FL_A_ENABLED : 0) |
		(c->vfl[1].enabled ? PME_DMA_CHANNEL_FL_B_ENABLED : 0);
}
EXPORT_SYMBOL(pme_channel_freelists);

size_t pme_channel_freelist_blksize(struct pme_channel *c, u8 freelist_id)
{
	freelist_id = freelist_id ? 1 : 0;
	return c->vfl[freelist_id].enabled ? \
			c->vfl[freelist_id].blocksize : 0;
}
EXPORT_SYMBOL(pme_channel_freelist_blksize);

void pme_fbchain_recycle(struct pme_fbchain *chain)
{
	struct pme_channel *channel = chain->channel;
	channel_vfreelist_recycle(&channel->vfl[chain->freelist_id],
			&channel->fbmfifo, &channel->fbthread, chain);
}
EXPORT_SYMBOL(pme_fbchain_recycle);

int pme_channel_err_register(struct pme_channel *c,
			struct pme_channel_err_ctx *ctx)
{
	int res;
	/* There is no way to synchronise on 'killed' itself, but whoever sets
	 * 'killed' goes on to iterate the 'obj_list' with 'lock' held. As
	 * such, we can obtain the same lock before checking 'killed' and
	 * thereby know whether we should or shouldn't be adding ourselves to
	 * the list. Also, object_wait() deregisters in advance and reregisters
	 * after waiting - so the (re)registration could fail if we were killed
	 * in the mean time. If so, leave 'notified' set so his object_free()
	 * can still deregister sanely. */
	spin_lock_bh(&c->lock);
	if (!c->killed) {
		list_add_tail(&ctx->list, &c->obj_list);
		ctx->notified = ctx->complete = 0;
		res = 0;
	} else {
		ctx->notified = ctx->complete = 1;
		res = -ENODEV;
	}
	spin_unlock_bh(&c->lock);
	return res;
}
EXPORT_SYMBOL(pme_channel_err_register);

void pme_channel_err_deregister(struct pme_channel *c,
			struct pme_channel_err_ctx *d,
			int block)
{
	int notified;
	might_sleep_if(block);
	spin_lock_bh(&c->lock);
	/* If the callback occurred before we obtained the lock, we're already
	 * detached from the list. */
	notified = d->notified;
	if (!notified) {
		list_del(&d->list);
		d->notified = 1;
	}
	spin_unlock_bh(&c->lock);
	if (!notified) {
		if (d->cb_on_deregister)
			d->cb(d, 0);
		d->complete = 1;
		wake_up(&c->queue);
	} else if (block)
		/* The error path has already been here or just won a race but
		 * not have executed the callback yet - make sure. */
		wait_event(c->queue, d->complete);
}
EXPORT_SYMBOL(pme_channel_err_deregister);

u32 pme_channel_reg_get(struct pme_channel *channel, unsigned int offset)
{
	BUG_ON(offset >= (4096>>2));
	return reg_get(channel->reg, offset);
}
EXPORT_SYMBOL(pme_channel_reg_get);

void pme_channel_reg_set(struct pme_channel *channel, unsigned int offset,
			u32 value)
{
	BUG_ON(offset >= (4096>>2));
	reg_set(channel->reg, offset, value);
}
EXPORT_SYMBOL(pme_channel_reg_set);

/* Context and their methods */
#include "base_context.h"

/***************/
/* ISR/tasklet */
/***************/

static irqreturn_t channel_isr(int irq, void *dev_id)
{
	struct pme_channel *channel = dev_id;
	u32 is;
	reg_set(channel->reg, CH_II, CH_II_INHIBIT);
	/* Inhibit interrupts prior to reading the IS register. The write is
	 * posted and so may not have an affect prior to the ISR exiting (so
	 * the ISR could fire again). Use a memory-barrier and then read the IS
	 * register (over the same bus as the inhibit-write), which should
	 * ensure that the write completes before the ISR exits. */
	smp_mb();
	is = reg_get(channel->reg, CH_IS);
	if (unlikely(!is)) {
		/* For whatever reason, the ISR fired while there's nothing to
		 * do. Uninhibit interrupts. */
		reg_set(channel->reg, CH_II, CH_II_UNINHIBIT);
		return IRQ_NONE;
	}
	tasklet_schedule(&channel->tasklet);
	return IRQ_HANDLED;
}

static void __wake_stats(struct pme_channel *c)
{
	spin_lock_bh(&c->lock);
	while (!list_empty(&c->sleeper_list)) {
		struct stat_sleeper *item =
				list_entry(c->sleeper_list.next,
					struct stat_sleeper, list);
		list_del(&item->list);
		list_add_tail(&item->list, &c->yawner_list);
		item->woken = 1;
		wake_up(&item->queue);
	}
	spin_unlock_bh(&c->lock);
	pme_wake_stats();
}

/* This routine is the meat of the channel tasklet, but is also used in a
 * tight-loop by the object's blocking interface after the real tasklet has
 * been suspended. This factors out what is common to both (the main difference
 * between this and the channel tasklet itself is that we don't check the
 * 'suspend' semaphore and we don't uninhibit interrupts). */
static inline void channel_sub_tasklet(struct pme_channel *c)
{
	void *ptr;
	u32 is = reg_get(c->reg, CH_IS);
	/* We update the notification FIFO hardware-written index (NPI) before
	 * that of the command FIFO (CCI) so that we never catch a notification
	 * before the completion of its command. For the same reason, we run
	 * the completion loops in the opposite order. */
	if (is & CHANNEL_IR_FIFO_NOT)
		channel_fifo_read_hwidx(&c->fifo[FIFO_NOT], c->reg);
	if (is & CHANNEL_IR_FIFO_CMD) {
		int full;
		u32 expired;
		/* Locking: see the note in fifo.h about fifo_read_hwidx() and
		 * fifo_room(). */
		spin_lock_bh(&c->lock);
		channel_fifo_read_hwidx(&c->fifo[FIFO_CMD], c->reg);
		full = !channel_fifo_room(&c->fifo[FIFO_CMD]);
		spin_unlock_bh(&c->lock);
		/* Expiries are lock-free */
		expired = channel_fifo_expire(&c->fifo[FIFO_CMD], ptr,
			if (object_complete_cmd(c, ptr, 0,
						(c->fifo[FIFO_CMD].type ==
							fifo_cmd_reduced)))
				printk(KERN_ERR PMMOD
					"command completion failed\n"););
		/* We need the lock back briefly to synchronise wake-list
		 * manipulations. */
		spin_lock_bh(&c->lock);
		channel_fifo_expired(&c->fifo[FIFO_CMD], c->reg, expired);
		if (unlikely(full && !c->access_list_waking && !c->killed))
			__cmd_next(c);
		spin_unlock_bh(&c->lock);
		if (c->stat_cmd_trigger) {
			c->stat_cmd += expired;
			if (unlikely(c->stat_cmd >= c->stat_cmd_trigger)) {
				c->stat_cmd = 0;
				__wake_stats(c);
			}
		}
	}
	if (is & CHANNEL_IR_FIFO_NOT) {
		u32 expired = channel_fifo_expire(&c->fifo[FIFO_NOT], ptr,
			if (object_complete_notification(c, ptr))
				printk(KERN_ERR PMMOD
					"notification completion failed\n"););
		channel_fifo_expired(&c->fifo[FIFO_NOT], c->reg, expired);
		if (c->stat_not_trigger) {
			c->stat_not += expired;
			if (unlikely(c->stat_not >= c->stat_not_trigger)) {
				c->stat_not = 0;
				__wake_stats(c);
			}
		}
	}
	if (unlikely(is & CHANNEL_IR_ERR & ~(c->monitor_IS))) {
		printk(KERN_INFO PMMOD "Error interrupt 0x%08x\n",
				is & CHANNEL_IR_ERR);
		/* Update the monitor bits and wake any sleepers */
		c->monitor_IS |= (is & CHANNEL_IR_ERR);
		/* We'll disable these error bits from asserting interrupts.
		 * This is safe without locking because IE is only set in two
		 * other places; on initialisation (~0) and on cleanup (0). */
		reg_set(c->reg, CH_IE, reg_get(c->reg, CH_IE) &
					~(c->monitor_IS));
		wake_up(&c->queue);
		/* Initiate the kill state */
		__channel_kill(c, 1);
	}
	/* The 3 freelist IR flags could be checked one after the other without
	 * the extra layer of "if" - however we do things this way based on an
	 * expectation of what the fast-path should be. */
	if (unlikely(is & CHANNEL_IR_FREELIST)) {
		if (is & CHANNEL_IR_FREELIST_A)
			channel_vfreelist_interrupt(&c->vfl[0], &c->fbmfifo,
						&c->fbthread);
		if (is & CHANNEL_IR_FREELIST_B)
			channel_vfreelist_interrupt(&c->vfl[1], &c->fbmfifo,
						&c->fbthread);
		if (is & CHANNEL_IR_FIFO_FB)
			channel_fbmfifo_interrupt(&c->fbmfifo, &c->fbthread);
	}
	reg_set(c->reg, CH_IS, is);
}
static inline void channel_suspend(struct pme_channel *c, int suspended)
{
	if (!suspended) {
		/* We're releasing the suspension, undo the below steps */
		atomic_inc(&c->bhsuspend);
		reg_set(c->reg, CH_II, CH_II_UNINHIBIT);
		local_bh_enable();
		return;
	}
	/* First, poll until we've grabbed the semaphore that stops the tasklet
	 * doing anything useful */
	while (!atomic_dec_and_test(&c->bhsuspend))
		atomic_inc(&c->bhsuspend);
	/* Second, inhibit interrupts (otherwise, if we're in the idle state,
	 * the first thing we will do will interrupt us and the ISR will
	 * inhibit interrupts and schedule the tasklet which will then run and
	 * notice we hold the semaphore). This saves an interrupt switch and a
	 * schedule to the tasklet. */
	reg_set(c->reg, CH_II, CH_II_INHIBIT);
	/* Third, prevent the switching to bottom-halves altogether */
	local_bh_disable();
}
static void channel_tasklet(unsigned long __channel)
{
	struct pme_channel *c = (struct pme_channel *)__channel;
	if (!atomic_dec_and_test(&c->bhsuspend)) {
		/* We're suspended due to a blocking operation (but the caller
		 * hasn't yet disabled bottom-halves on the local CPU or we got
		 * scheduled to another CPU:-)), bail out. */
		atomic_inc(&c->bhsuspend);
		return;
	}
	channel_sub_tasklet(c);
	reg_set(c->reg, CH_II, CH_II_UNINHIBIT);
	atomic_inc(&c->bhsuspend);
}

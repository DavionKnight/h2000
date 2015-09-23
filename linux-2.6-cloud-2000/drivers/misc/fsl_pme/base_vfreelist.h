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
 * This file implements (virtual) freelist handling for the pattern-matcher
 * driver and is inlined into base_channel.c as it is per-channel logic. The
 * logic represents an "fbthread" (kernel thread that performs deallocation
 * work to ensure APIs are non-blocking/drop-and-go), an "fbmfifo" (the
 * deallocate FIFO for a channel), and "vfreelist"s (the 0, 1, or 2 virtual
 * freelists per channel). Presently, "fbthread"s and "fbmfifos" are 1-to-1
 * with channels, but this arrangement would allow easy modification to, say,
 * share threads between channels. This also handles "mastering" of freelists,
 * when a channel's virtual freelist is tagged (in common-control
 * configuration) as being responsible for seeding the freelist with buffers
 * and so cleaning them up on teardown.
 *
 */

/*********************/
/* Virtual freelists */
/*********************/

/*
 * This is an "abstraction" that gets inlined into base_channel.c, thus
 * encapsulation isn't enforced by the compiler. This data-path code ought to
 * be a single compiler object so we can't break it into C files, yet it's
 * preferable to keep some separation.
 */

/*
 * Initialisation:
 *     [...fbmfifo.fifo + ISR should already be initialised...]
 *     channel_fbthread_start()   - thread running
 *     channel_fbmfifo_init()     - hooked up to fbthread, linked 'nosched'
 *     channel_vfreelist_init()   - hooked up to fbmfifo, linked 'sched'
 *                                  low-water interrupts will fire right away
 *
 * Kill:
 *     channel_vfreelist_sync()   - wait until vfreelist is synced, linked
 *                                  'nosched'
 *
 * Reset:
 *     [...fbmfifo.fifo + ISR should already be reset...]
 *     channel_vfreelist_reset()  - restart, linked 'sched'
 *                                  low-water interrupts will fire right away
 *
 * Cleanup:
 *     channel_vfreelist_sync()
 *     channel_vfreelist_finish() - unhook from fbmfifo, cleanup
 *     channel_fbmfifo_finish()   - unhook from fbmfifo, cleanup
 *     channel_fbthread_end()     - thread exited
 *     [...fbmfifo.fifo should cleanup after this point...]
 */

#include <linux/kthread.h>
/* Encapsulate a kernel thread, to which we can attach fbmfifos and manage
 * scheduling. */
struct channel_fbthread {
	int id;
	/* Locks all scheduling (of vfreelists that belong to fbmfifos that
	 * belong to this thread). */
	spinlock_t lock;
	struct list_head fb_sched;   /* List of fbmfifos to process */
	struct list_head fb_nosched; /* List of idle fbmfifos */
	int in_exit;                 /* User waiting for thread exit */
	enum {
		p_steady,
		p_pause,
		p_unpause
	} pause;                     /* Used to pause/unpause */
	wait_queue_head_t queue;
	struct task_struct *kthread_task; /* task returned by kthread_run */
};

/* Derive fbmfifo from channel_fifo, to which we can attach vfreelists. */
struct channel_fbmfifo {
	struct channel_fifo *fifo;
	struct pme_channel *channel;
	struct pme_regmap *reg;
	struct list_head run_sched;   /* linked into thread scheduling */
	int scheduled;
	int interrupted;
	/* State used by this FB FIFO */
	struct list_head vfl_reset;   /* List of vfreelists to (re)set */
	struct list_head vfl_sched;   /* List of vfreelists to deallocate */
	struct list_head vfl_wait;    /* waiting for FIFO interrupt */
	struct list_head vfl_nosched; /* List of idle vfreelists */
};

/* Encapsulate a virtual freelist */
struct channel_vfreelist {
	/* The parameters for this vfreelist, as passed in the "load" ioctl. */
	struct __pme_ctrl_load_fb params;
	u8 freelist_id;
	/* The configuration for this vfreelist, as mirrored to the channel
	 * registers from the CC register configuration. */
	u8 enabled, master;
	u32 blocksize;
	/* Run-time state */
	struct list_head sched;        /* linked into fbmfifo scheduling */
	int scheduled;                 /* ==0 iff linked to 'vfl_nosched' */
	struct list_head release_list; /* fbchains to deallocate to h/w */
	unsigned int num_infifo;       /* # of buffers in FB FIFO - non-zero
					* only if we're completing. */
	unsigned int num_waiting;      /* # of buffers in 'release_list'. */
	unsigned int num_owned;        /* # of buffers not-yet-recycled. */
	unsigned int num_input;        /* # of buffers in the CMD FIFO via
					* "input-release". */
	int in_syncing;                /* User waiting for sync */
	int in_reset;                  /* User waiting for (re)set */
	int synced;                    /* vfreelist stopped, needs reset */
	/* Run-time state: only when mastering */
	char name_slab[24];            /* pme_channel_slab[AB]_n" */
	struct fbuffer_slab slab;
	void **delta;                  /* Storage when allocating new chains */
	unsigned int num_allocated;    /* # of buffers from 'slab', total */
	enum __pme_vfl_lowwater lowwater;
};

/********************/
/* channel_fbthread */
/********************/

static inline void LOCK(struct channel_fbthread *fbthread)
{ spin_lock_bh(&fbthread->lock); }

static inline void UNLOCK(struct channel_fbthread *fbthread)
{ spin_unlock_bh(&fbthread->lock); }

/* helper to set vfreelist thresholds */
static inline void __vfreelist_thresh(struct channel_vfreelist *vfl,
					struct channel_fbmfifo *fbmfifo,
					int lw_enable)
{
	reg_set(fbmfifo->reg, CH_FBL_THRESH(vfl->freelist_id),
		vfl->params.starve |
		((vfl->master && lw_enable) ? (vfl->params.low << 16) : 0));
}

/* helper to control on-deck - disable sleeps */
static inline void __vfreelist_odd(struct channel_vfreelist *vfl,
					struct channel_fbmfifo *fbmfifo,
					int odd_enable)
{
	might_sleep_if(!odd_enable);
	if (!vfl->master)
		return;
	reg_set(fbmfifo->reg, CH_FBL_ODD(vfl->freelist_id), odd_enable ? 0 : 1);
	if (!odd_enable)
		msleep(PME_DMA_CH_ODDSYNC_MS);
}

/* helper to check if syncing has completed */
static inline void __vfreelist_synced(struct channel_vfreelist *vfl,
				struct channel_fbmfifo *fbmfifo,
				struct channel_fbthread *fbthread)
{
	if (unlikely(vfl->in_syncing && !vfl->num_infifo && !vfl->num_waiting &&
				!vfl->num_owned && !vfl->num_input)) {
		vfl->synced = 1;
		vfl->in_syncing = 0;
		__vfreelist_thresh(vfl, fbmfifo, 0);
		wake_up(&fbthread->queue);
	}
}
static inline void __vfreelist_lowwater(struct channel_vfreelist *vfl,
				struct channel_fbmfifo *fbmfifo,
				struct channel_fbthread *fbthread)
{
	if (unlikely(vfl->master && (vfl->lowwater == lw_triggered))) {
		struct pme_fbchain *chain;
		unsigned int loop;
		UNLOCK(fbthread);
		for (loop = 0; loop < vfl->params.delta; loop++) {
			vfl->delta[loop] = fbuffer_alloc(&vfl->slab);
			if (!vfl->delta[loop])
				goto err_alloc;
		}
		chain = fbchain_alloc_ll(fbmfifo->channel, vfl->freelist_id,
					vfl->delta, vfl->params.delta);
		if (chain) {
			vfl->lowwater = lw_wait;
			LOCK(fbthread);
			list_add_tail(&chain->list, &vfl->release_list);
			vfl->num_waiting += vfl->params.delta;
			vfl->num_allocated += vfl->params.delta;
			return;
		}
err_alloc:
		while (loop--)
			fbuffer_free(&vfl->slab, vfl->delta[loop]);
		printk(KERN_ERR PMMOD "low-water allocation failed\n");
		vfl->lowwater = lw_idle;
		LOCK(fbthread);
	}
}

static int __fbthread_routine(void *__fbthread__)
{
	char threadname[12];
	struct channel_vfreelist *vfl;
	struct channel_fbmfifo *fbmfifo;
	struct channel_fbthread *fbthread = __fbthread__;

	/* Detach from relationship with the parent process */
	snprintf(threadname, sizeof(threadname), "fbthread%d", fbthread->id);
	lock_kernel();
	daemonize(threadname);
	unlock_kernel();

	/************************/
	/* Thread state-machine */
	/************************/
main_loop:
	wait_event_interruptible(fbthread->queue,
			!list_empty(&fbthread->fb_sched) || fbthread->in_exit ||
				(fbthread->pause != p_steady));
	if (signal_pending(current)) {
		printk(KERN_ERR PMMOD "fbthread%d: got signalled, ignoring\n",
				fbthread->id);
		flush_signals(current);
		goto main_loop;
	}
	/* There is a handshake mechanism to allow a caller to pause the
	 * fbthread, do something once it has safely paused, and then unpause
	 * the fbthread; */
	if (unlikely(fbthread->pause != p_steady)) {
		fbthread->pause = p_steady;
		wake_up(&fbthread->queue);
		wait_event(fbthread->queue, fbthread->pause != p_steady);
		fbthread->pause = p_steady;
		goto main_loop;
	}
	LOCK(fbthread);
	if (fbthread->in_exit) {
		fbthread->in_exit = 0;
		UNLOCK(fbthread);
		wake_up(&fbthread->queue);
		return 0;
	}
	/* Extract fbmfifo to work on */
	fbmfifo = list_entry(fbthread->fb_sched.next, struct channel_fbmfifo,
				run_sched);
	list_del(&fbmfifo->run_sched);
	/****************************/
	/* vfreelist post-interrupt */
	/****************************/
	if (fbmfifo->interrupted) {
		fbmfifo->interrupted = 0;
		channel_fifo_read_hwidx(fbmfifo->fifo, fbmfifo->reg);
		while (!list_empty(&fbmfifo->vfl_wait)) {
			vfl = list_entry(fbmfifo->vfl_wait.next,
					struct channel_vfreelist, sched);
			list_del(&vfl->sched);
			vfl->num_infifo = 0;
			__vfreelist_lowwater(vfl, fbmfifo, fbthread);
			__vfreelist_synced(vfl, fbmfifo, fbthread);
			if (vfl->num_waiting)
				list_add_tail(&vfl->sched,
						&fbmfifo->vfl_sched);
			else {
				list_add_tail(&vfl->sched,
						&fbmfifo->vfl_nosched);
				vfl->scheduled = 0;
				if (unlikely(vfl->master && (vfl->lowwater ==
								lw_wait))) {
					if (vfl->params.max &&
							(vfl->num_allocated >
							vfl->params.max))
						vfl->lowwater = lw_maxed;
					else {
						vfl->lowwater = lw_idle;
						__vfreelist_thresh(vfl,
								fbmfifo, 1);
					}
				}
			}
		}
	}
	/********************/
	/* vfreelist resets */
	/********************/
	while (!list_empty(&fbmfifo->vfl_reset)) {
		vfl = list_entry(fbmfifo->vfl_reset.next,
				struct channel_vfreelist, sched);
		list_del(&vfl->sched);
		list_add_tail(&vfl->sched, &fbmfifo->vfl_nosched);
		vfl->in_syncing = vfl->synced = 0;
		/* Before setting registers that might trigger interrupts, exit
		 * "reset" (otherwise we might trap on debugging checks) */
		vfl->in_reset = 0;
		wake_up(&fbthread->queue);
		UNLOCK(fbthread);
		__vfreelist_odd(vfl, fbmfifo, 1);
		if (vfl->master && (vfl->lowwater != lw_maxed))
			__vfreelist_thresh(vfl, fbmfifo, 1);
		else
			__vfreelist_thresh(vfl, fbmfifo, 0);
		LOCK(fbthread);
	}
	/***********************/
	/* vfreelist schedules */
	/***********************/
	/* WARNING: I've intentionally not indented the while() branch - so
	 * consider the if() while() construct as a single one. */
	if (likely(list_empty(&fbmfifo->vfl_wait)))
		while (channel_fifo_room(fbmfifo->fifo) &&
				!list_empty(&fbmfifo->vfl_sched)) {
			u32 reclaim;
			int vfl_used = 0;
			vfl = list_entry(fbmfifo->vfl_sched.next,
					struct channel_vfreelist, sched);
			__vfreelist_lowwater(vfl, fbmfifo, fbthread);
			/* If we're above "high", we reclaim buffers instead of
			 * passing them to hardware. */
			reclaim = reg_get(fbmfifo->reg,
					CH_FBL_NB(vfl->freelist_id));
			if (vfl->params.high && (reclaim > vfl->params.high))
				reclaim -= vfl->params.high;
			else
				reclaim = 0;
			/* We only want to reclaim if the given chain will not
			 * take us *lower* than the 'high' watermark. To avoid
			 * infinite loops we only consider reclaiming if putting
			 * the chain on the FIFO is available as an alternative.
			 * */
			while (!list_empty(&vfl->release_list) &&
					channel_fifo_room(fbmfifo->fifo)) {
				struct pme_fbchain *chain =
					list_entry(vfl->release_list.next,
						struct pme_fbchain, list);
				list_del(&chain->list);
				vfl->num_waiting -= chain->num_blocks;
				/* We put even zero-length chains on the recycle
				 * list to be recycled by the fbthread - the
				 * alternative is to free them *instead* of
				 * putting them on the list.  However this
				 * typically involves tasklet context, so as
				 * recycling is not latency-critical, so we do
				 * it this way. */
				if (!chain->num_blocks) {
					fbchain_free(chain);
					continue;
				}
				/* If we're reclaiming and this chain isn't
				 * going to take us below our reclaim threshold,
				 * put the buffers back in the slab. */
				if (reclaim >= chain->num_blocks) {
					vfl->num_allocated -= chain->num_blocks;
					reclaim -= chain->num_blocks;
					fbchain_free_all(chain, &vfl->slab);
					if (unlikely(
						(vfl->lowwater == lw_maxed)
						&& (vfl->num_allocated
						< vfl->params.max))) {
						vfl->lowwater = lw_idle;
						__vfreelist_thresh(vfl,
								fbmfifo, 1);
					}
					continue;
				}
				/* otherwise, send the chain back to the
				 * freelist */
				vfl->num_infifo += chain->num_blocks;
				UNLOCK(fbthread);
				fbchain_send(chain,
				channel_fifo_produce_ptr(fbmfifo->fifo));
				channel_fifo_produced(fbmfifo->fifo,
						fbmfifo->reg);
				vfl_used = 1;
				LOCK(fbthread);
			}
			list_del(&vfl->sched);
			if (likely(vfl_used))
				list_add_tail(&vfl->sched,
						&fbmfifo->vfl_wait);
			else {
				list_add_tail(&vfl->sched,
						&fbmfifo->vfl_nosched);
				vfl->scheduled = 0;
				__vfreelist_synced(vfl, fbmfifo,
						fbthread);
			}
		}
	if (likely(!list_empty(&fbmfifo->vfl_wait)))
		/* Interrupt us when the FB FIFO is empty. */
		reg_set(fbmfifo->reg, CH_FB_FIFO_THRESH, 1);
	if (fbmfifo->interrupted || !list_empty(&fbmfifo->vfl_reset))
		/* fbmfifo stays scheduled */
		list_add_tail(&fbmfifo->run_sched, &fbthread->fb_sched);
	else {
		fbmfifo->scheduled = 0;
		list_add_tail(&fbmfifo->run_sched, &fbthread->fb_nosched);
	}
	UNLOCK(fbthread);
	goto main_loop;
}

static inline int channel_fbthread_start(struct channel_fbthread *fbthread,
					int id)
{
	fbthread->id = id;
	fbthread->in_exit = 0;
	fbthread->pause = p_steady;
	spin_lock_init(&fbthread->lock);
	INIT_LIST_HEAD(&fbthread->fb_sched);
	INIT_LIST_HEAD(&fbthread->fb_nosched);
	init_waitqueue_head(&fbthread->queue);
	fbthread->kthread_task = kthread_run(
			__fbthread_routine, fbthread, "%p", fbthread);
	if (IS_ERR(fbthread->kthread_task)) {
		printk(KERN_ERR PMMOD "failed to start freelist thread\n");
		return PTR_ERR(fbthread->kthread_task);
	}
	return 0;
}

static inline void channel_fbthread_end(struct channel_fbthread *fbthread)
{
	LOCK(fbthread);
	fbthread->in_exit = 1;
	UNLOCK(fbthread);
	if (kthread_stop(fbthread->kthread_task))
		printk(KERN_ERR PMMOD "failed to stop freelist thread\n");
}

/*******************/
/* channel_fbmfifo */
/*******************/

static inline void __fbmfifo_kick(struct channel_fbmfifo *fbmfifo,
				struct channel_fbthread *fbthread)
{
	if (!fbmfifo->scheduled) {
		fbmfifo->scheduled = 1;
		list_del(&fbmfifo->run_sched);
		list_add_tail(&fbmfifo->run_sched, &fbthread->fb_sched);
		wake_up(&fbthread->queue);
	}
}

static inline void channel_fbmfifo_init(struct channel_fbmfifo *fbmfifo,
				struct channel_fifo *fifo,
				struct channel_fbthread *fbthread,
				struct pme_channel *channel,
				struct pme_regmap *reg)
{
	fbmfifo->fifo = fifo;
	fbmfifo->channel = channel;
	fbmfifo->reg = reg;
	fbmfifo->scheduled = 0;
	fbmfifo->interrupted = 0;
	INIT_LIST_HEAD(&fbmfifo->vfl_reset);
	INIT_LIST_HEAD(&fbmfifo->vfl_sched);
	INIT_LIST_HEAD(&fbmfifo->vfl_wait);
	INIT_LIST_HEAD(&fbmfifo->vfl_nosched);
	LOCK(fbthread);
	list_add_tail(&fbmfifo->run_sched, &fbthread->fb_nosched);
	UNLOCK(fbthread);
}

static inline void channel_fbmfifo_finish(struct channel_fbmfifo *fbmfifo,
				struct channel_fbthread *fbthread)
{
	LOCK(fbthread);
	list_del(&fbmfifo->run_sched);
	UNLOCK(fbthread);
}

static inline void channel_fbmfifo_interrupt(struct channel_fbmfifo *fbmfifo,
				struct channel_fbthread *fbthread)
{
	LOCK(fbthread);
	reg_set(fbmfifo->reg, CH_FB_FIFO_THRESH, 0);
	fbmfifo->interrupted = 1;
	__fbmfifo_kick(fbmfifo, fbthread);
	UNLOCK(fbthread);
}

static void channel_fbmfifo_dump(struct channel_fbmfifo *f,
				struct channel_fbthread *fbthread,
				struct __pme_dump_fbm *d)
{
	struct list_head *l;
	LOCK(fbthread);
	d->scheduled = f->scheduled;
	d->interrupted = f->interrupted;
	d->vfl_reset = d->vfl_sched = d->vfl_wait = d->vfl_nosched = 0;
	list_for_each(l, &f->vfl_reset) d->vfl_reset++;
	list_for_each(l, &f->vfl_sched) d->vfl_sched++;
	list_for_each(l, &f->vfl_wait) d->vfl_wait++;
	list_for_each(l, &f->vfl_nosched) d->vfl_nosched++;
	UNLOCK(fbthread);
}

/*********************/
/* channel_vfreelist */
/*********************/

/* The fbthread lock must already be held */
static inline void __vfreelist_kick(struct channel_vfreelist *vfl,
				struct channel_fbmfifo *fbmfifo,
				struct channel_fbthread *fbthread)
{
	if (!vfl->scheduled) {
		list_del(&vfl->sched);
		if (unlikely(vfl->in_reset))
			list_add_tail(&vfl->sched, &fbmfifo->vfl_reset);
		else {
			vfl->scheduled = 1;
			list_add_tail(&vfl->sched, &fbmfifo->vfl_sched);
		}
		__fbmfifo_kick(fbmfifo, fbthread);
	}
}

static inline void __vfreelist_zap_hw(struct channel_fbmfifo *fbmfifo,
					struct channel_vfreelist *vfl)
{
#define ODD_RST_BIT (u32)(1 << 4)
	reg_set(fbmfifo->reg, CH_FBL_ODD(vfl->freelist_id), ODD_RST_BIT);
	reg_set(fbmfifo->reg, CH_FBL_ODD(vfl->freelist_id), 0);
}

static inline int channel_vfreelist_init(struct channel_vfreelist *vfl,
				struct channel_fbmfifo *fbmfifo,
				struct channel_fbthread *fbthread,
				const struct __pme_ctrl_load_fb *params,
				unsigned int channel_idx,
				u8 freelist_id)
{
	u32 reg = reg_get(fbmfifo->reg, CH_FBL_BS(freelist_id));
	vfl->freelist_id = freelist_id;
	if (!FBL_BS_ENABLED(reg)) {
		vfl->enabled = 0;
		return 0;
	}
	vfl->enabled = 1;
	/* We allow a NULL params when reinitialising */
	if (params)
		memcpy(&vfl->params, params, sizeof(*params));
	vfl->blocksize = FBL_BS_SIZE(reg);
	LOCK(fbthread);
	vfl->scheduled = 0;
	UNLOCK(fbthread);
	vfl->num_infifo = vfl->num_waiting = vfl->num_owned =
		vfl->num_input = 0;
	vfl->in_syncing = vfl->in_reset = 0;
	/* Our initial state is such that a reset is needed to kickstart */
	vfl->synced = 1;
	INIT_LIST_HEAD(&vfl->release_list);
	if (!FBL_BS_MASTER(reg)) {
		vfl->master = 0;
		goto success;
	}
	vfl->master = 1;
	vfl->num_allocated = 0;
	vfl->lowwater = lw_idle;
	snprintf(vfl->name_slab, sizeof(vfl->name_slab), "pme_channel_%d_%c",
				channel_idx, vfl->freelist_id ? 'B' : 'A');
	if (fbuffer_create(&vfl->slab, vfl->blocksize, vfl->name_slab)) {
		printk(KERN_ERR PMMOD "slab setup failed\n");
		return -ENOMEM;
	}
	vfl->delta = kmalloc(vfl->params.delta * sizeof(void *), GFP_KERNEL);
	if (!vfl->delta) {
		printk(KERN_ERR PMMOD "delta allocation failed\n");
		fbuffer_destroy(&vfl->slab);
		return -ENOMEM;
	}
	__vfreelist_zap_hw(fbmfifo, vfl);
success:
	list_add_tail(&vfl->sched, &fbmfifo->vfl_nosched);
	return 0;
}

static inline void channel_vfreelist_finish(struct channel_vfreelist *vfl,
				struct channel_fbmfifo *fbmfifo,
				struct channel_fbthread *fbthread)
{
	if (!vfl->enabled)
		return;
	LOCK(fbthread);
	list_del(&vfl->sched);
	UNLOCK(fbthread);
	if (vfl->master) {
		u64 head_v, head_p, tail_p, ondeck_v;
		u32 ondeck_valid, num;
		might_sleep();
		/* Disable automatic ODD population and take a small sleep */
		__vfreelist_odd(vfl, fbmfifo, 0);
		/* Capture on-deck, head, tail, list-length */
		head_v = reg_get64(fbmfifo->reg,
				CH_FBL_VHH(vfl->freelist_id));
		head_p = reg_get64(fbmfifo->reg,
				CH_FBL_PHH(vfl->freelist_id));
		tail_p = reg_get64(fbmfifo->reg,
				CH_FBL_PTH(vfl->freelist_id));
		ondeck_v = reg_get64(fbmfifo->reg,
				CH_FBL_VDH(vfl->freelist_id));
		ondeck_valid = FBL_BS_ONDECK(reg_get(fbmfifo->reg,
					CH_FBL_BS(vfl->freelist_id)));
		num = reg_get(fbmfifo->reg, CH_FBL_NB(vfl->freelist_id));
		/* Hit the ODD "RST" bit to reset the physical freelist */
		__vfreelist_zap_hw(fbmfifo, vfl);
		/* Reap the buffers that were in the physical freelist before
		 * we told h/w to forget about them. */
		if (ondeck_valid)
			num++;
		vfl->num_allocated = 0;
		if (ondeck_valid) {
			u64 ondeck_p = (u64)DMA_MAP_SINGLE(u64_to_ptr(ondeck_v),
							1, DMA_TO_DEVICE);
			nochain_free_all(&vfl->slab, ondeck_v, ondeck_p,
						tail_p, num);
		} else
			nochain_free_all(&vfl->slab, head_v, head_p,
						tail_p, num);
		kfree(vfl->delta);
		fbuffer_destroy(&vfl->slab);
	}
}

static inline int channel_vfreelist_kill(struct channel_vfreelist *vfl,
				struct channel_fbmfifo *fbmfifo,
				struct channel_fbthread *fbthread)
{
	if (!vfl->enabled)
		return 0;
	LOCK(fbthread);
	if (vfl->synced) {
		UNLOCK(fbthread);
		return 0;
	}
	if (vfl->in_reset) {
		UNLOCK(fbthread);
		return -EBUSY;
	}
	if (!vfl->in_syncing) {
		vfl->in_syncing = 1;
		/* Even if the vfreelist is already essentially synced, we'll
		 * need to wake up the thread to notice. */
		__vfreelist_kick(vfl, fbmfifo, fbthread);
	}
	UNLOCK(fbthread);
	return 0;
}

static inline int channel_vfreelist_synced(struct channel_vfreelist *vfl)
{
	int val = (vfl->enabled ? vfl->synced : 1);
	return val;
}

static inline int channel_vfreelist_wait(struct channel_vfreelist *vfl,
					struct channel_fbthread *fbthread,
					int interruptible, u32 ms)
{
	int res;
	if (channel_vfreelist_synced(vfl))
		return 0;
	if (!vfl->in_syncing)
		return -EBUSY;
	if (interruptible) {
		if (ms) {
			res = wait_event_interruptible_timeout(fbthread->queue,
							msecs_to_jiffies(ms),
							vfl->synced);
			if (res < 0)
				res = -EINTR;
			else if (!res)
				res = -ETIMEDOUT;
			else
				res = 0;
		} else {
			res = wait_event_interruptible(fbthread->queue,
							vfl->synced);
			if (res < 0)
				res = -EINTR;
		}
	} else {
		if (ms) {
			res = wait_event_timeout(fbthread->queue,
						msecs_to_jiffies(ms),
						vfl->synced);
			if (!res)
				res = -ETIMEDOUT;
			else
				res = 0;
		} else {
			wait_event(fbthread->queue, vfl->synced);
			res = 0;
		}
	}
	return res;
}

static inline int channel_vfreelist_reset(struct channel_vfreelist *vfl,
				struct channel_fbmfifo *fbmfifo,
				struct channel_fbthread *fbthread)
{
	if (!vfl->enabled)
		return 0;
	LOCK(fbthread);
	if (!vfl->synced) {
		UNLOCK(fbthread);
		return -EBUSY;
	}
	vfl->in_reset = 1;
	__vfreelist_kick(vfl, fbmfifo, fbthread);
	UNLOCK(fbthread);
	wait_event(fbthread->queue, !vfl->in_reset);
	return 0;
}

static inline void channel_vfreelist_recycle(struct channel_vfreelist *vfl,
					struct channel_fbmfifo *fbmfifo,
					struct channel_fbthread *fbthread,
					struct pme_fbchain *chain)
{
	if (!chain->num_blocks) {
		fbchain_free(chain);
		return;
	}

	LOCK(fbthread);
	/* If there is space on the FIFO, do a quick recycle */
	channel_fifo_read_hwidx(fbmfifo->fifo, fbmfifo->reg);
	if (channel_fifo_room(fbmfifo->fifo)) {
		fbchain_send(chain,
			channel_fifo_produce_ptr(fbmfifo->fifo));
		channel_fifo_produced(fbmfifo->fifo, fbmfifo->reg);
		vfl->num_owned -= chain->num_blocks;
		if (!fbmfifo->interrupted) {
			/* Interrupt us when the FB FIFO is empty. */
			reg_set(fbmfifo->reg, CH_FB_FIFO_THRESH, 500);
		}
		UNLOCK(fbthread);
		return;
	}

	list_add_tail(&chain->list, &vfl->release_list);
	vfl->num_waiting += chain->num_blocks;
	vfl->num_owned -= chain->num_blocks;
	__vfreelist_kick(vfl, fbmfifo, fbthread);
	UNLOCK(fbthread);
}

static inline void channel_vfreelist_interrupt(struct channel_vfreelist *vfl,
				struct channel_fbmfifo *fbmfifo,
				struct channel_fbthread *fbthread)
{
	LOCK(fbthread);
	vfl->lowwater = lw_triggered;
	__vfreelist_thresh(vfl, fbmfifo, 0);
	__vfreelist_kick(vfl, fbmfifo, fbthread);
	UNLOCK(fbthread);
}

static inline void channel_vfreelist_fb_owned(struct channel_vfreelist *vfl,
					struct channel_fbthread *fbthread,
					struct pme_fbchain *chain)
{
	LOCK(fbthread);
	vfl->num_owned += chain->num_blocks;
	UNLOCK(fbthread);
}

static inline void channel_vfreelist_inputrelease_add(
					struct channel_vfreelist *vfl,
					struct channel_fbthread *fbthread,
					struct pme_fbchain *chain)
{
	LOCK(fbthread);
	vfl->num_owned -= chain->num_blocks;
	vfl->num_input += chain->num_blocks;
	UNLOCK(fbthread);
}

static inline void channel_vfreelist_inputrelease_consume(
				struct channel_vfreelist *vfl,
				struct channel_fbmfifo *fbmfifo,
				struct channel_fbthread *fbthread,
				struct pme_fbchain *chain)
{
	unsigned int cnt = chain->num_blocks;
	fbchain_free(chain);
	LOCK(fbthread);
	vfl->num_input -= cnt;
	__vfreelist_synced(vfl, fbmfifo, fbthread);
	UNLOCK(fbthread);
}

static inline void channel_vfreelist_inputrelease_abort(
				struct channel_vfreelist *vfl,
				struct channel_fbthread *fbthread,
				struct pme_fbchain *chain)
{
	LOCK(fbthread);
	vfl->num_owned += chain->num_blocks;
	vfl->num_input -= chain->num_blocks;
	UNLOCK(fbthread);
}

static int channel_vfreelist_dump(struct channel_vfreelist *vfl,
				struct channel_fbmfifo *fbmfifo,
				struct channel_fbthread *fbthread,
				struct pme_dump_freelist *dump)
{
	int res;
	u32 odd;
	u64 head_virt, head_phys, tail_phys;
	struct __pme_dump_ned ned;
	struct __ll_ned *pned = NULL;
	unsigned int loop = 0;
	u8 id = vfl->freelist_id;
	/* We rely on the freelist being idle - so we pause the fbthread and
	 * temporarily set the ODD register. */
	fbthread->pause = p_pause;
	wake_up(&fbthread->queue);
	wait_event(fbthread->queue, fbthread->pause == p_steady);
	odd = reg_get(fbmfifo->reg, CH_FBL_ODD(id));
	reg_set(fbmfifo->reg, CH_FBL_ODD(id), 0);
	msleep(PME_DMA_CH_ODDSYNC_MS);

	if (!vfl->enabled) {
		res = -EINVAL;
		goto end;
	}
	dump->freelist_len = reg_get(fbmfifo->reg, CH_FBL_NB(id));
	dump->ondeck_virt = reg_get64(fbmfifo->reg, CH_FBL_VDH(id));
	dump->bsize = reg_get(fbmfifo->reg, CH_FBL_BS(id));
	head_phys = reg_get64(fbmfifo->reg, CH_FBL_PHH(id));
	head_virt = reg_get64(fbmfifo->reg, CH_FBL_VHH(id));
	tail_phys = reg_get64(fbmfifo->reg, CH_FBL_PTH(id));
	while ((loop < dump->neds_num) && (loop < dump->freelist_len)) {
		if (!loop) {
			ned.virt = head_virt;
			ned.phys = head_phys;
		} else {
			ned.virt = pned->virt;
			ned.phys = pned->phys;
		}
		if (copy_to_user(dump->neds + loop, &ned, sizeof(ned))) {
			res = -EFAULT;
			goto end;
		}
		pned = u64_to_ptr(ned.virt);
		loop++;
	}
	res = 0;

end:
	reg_set(fbmfifo->reg, CH_FBL_ODD(id), odd);
	fbthread->pause = p_unpause;
	wake_up(&fbthread->queue);
	return res;
}

static void channel_vfreelist_dump_admin(struct channel_vfreelist *v,
					struct __pme_dump_vfl *d)
{
	struct list_head *l;
	memcpy(&d->params, &v->params, sizeof(d->params));
	d->freelist_id = v->freelist_id;
	d->enabled = v->enabled;
	d->master = v->master;
	d->blocksize = v->blocksize;
	d->scheduled = v->scheduled;
	d->in_syncing = v->in_syncing;
	d->in_reset = v->in_reset;
	d->synced = v->synced;
	d->chains_to_release = 0;
	if (v->enabled)
		list_for_each(l, &v->release_list) d->chains_to_release++;
	d->num_infifo = v->num_infifo;
	d->num_waiting = v->num_waiting;
	d->num_owned = v->num_owned;
	d->num_input = v->num_input;
	d->num_allocated = v->num_allocated;
	d->lowwater = v->lowwater;
}

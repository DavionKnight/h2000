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
 * This file implements wrapper logic to encapsulate each of the 3 FIFOs a
 * channel has. It deals with the PM block's peculiar "extra bit" index
 * counters (a FIFO of size n uses indices from 0 to 2n-1, to distinguish
 * between full and empty), as well iterators and "blocking" (where
 * writeback to hardware can be stalled in software to allow production or
 * completion work to be flushed to hardware in batches).
 *
 */

/*********/
/* FIFOs */
/*********/

/*
 * This is an "abstraction" that gets inlined into base_channel.c, thus
 * encapsulation isn't enforced by the compiler. This data-path code ought to
 * be a single compiler object so we can't break it into C files, yet it's
 * preferable to keep some separation.
 */

struct channel_fifo {
	/* Static configuration */
	void *kptr;
	struct page *page;
	enum __pme_channel_fifo_type type;
	size_t entry_size;
	u32 num, idx_mask, entry_mask;
	unsigned int offset_sw1, offset_hw2, offset_sw3;
	int block;
	/* Dynamic state. sw1, hw2, and sw3 allow whichever r/w model is
	 * appropriate. Command FIFOs use all 3, whereas notification FIFOs use
	 * hw2/sw3 and FBM FIFOs use sw1/hw2. 'fill12' and 'fill23' maintain
	 * the deltas, so changes from hardware (hw2) require cyclic arithmetic
	 * but changes by software (sw1/sw3) can use inc/decrements, the latter
	 * occur more frequently whenever expiry loops cover more than one
	 * entry per interrupt. */
	u32 sw1, hw2, sw3, fill12, fill23;
};

/* Initialise a channel FIFO */
static int channel_fifo_init(struct channel_fifo *fifo,
			enum __pme_channel_fifo_type type, u32 num)
{
	size_t sz;
	if (!ISEXP2(num))
		return -EINVAL;
	switch (type) {
	case fifo_cmd_reduced:
		fifo->entry_size = sizeof(struct fifo_cmd) / 2;
		break;
	case fifo_cmd_normal:
		fifo->entry_size = sizeof(struct fifo_cmd);
		break;
	case fifo_notify:
		fifo->entry_size = sizeof(struct fifo_notify);
		break;
	case fifo_fbm:
		fifo->entry_size = sizeof(struct fifo_fbm);
		break;
	}
	sz = num * fifo->entry_size;
	fifo->page = alloc_pages(GFP_KERNEL, get_order(sz));
	if (!fifo->page)
		return -ENOMEM;
	fifo->kptr = page_address(fifo->page);
	fifo->type = type;
	fifo->num = num;
	fifo->idx_mask = (2 * num) - 1;
	fifo->entry_mask = num - 1;
	fifo->fill12 = fifo->fill23 = 0;
	return 0;
}

/* Strangely enough, we should only need locking with the command FIFO, between
 * read_hwidx(), fifo_room(), and channel_fifo_expired(). This is because
 * otherwise the cmd_start() logic might go to sleep due to a "full FIFO" when
 * it actually wasn't, then the tasklet (who calls the read_hwidx()) won't
 * initiate wake-ups after expiries. Ie. the race is with respect to production
 * on the command FIFO. The only other cases that might have raced are safe
 * because of other considerations; expiry on the command FIFO is in the same
 * tasklet as the read_hwidx() call and we update the FIFO afterwards in the
 * expired() call, FB FIFO fill-levels are on/off - they're either
 * produced-and-waiting or empty-and-idle, and notification FIFOs don't care
 * about fill-levels at all.
 *
 * So we leave this locking case to the caller and put nothing in the fifo
 * logic per se. */

static inline void channel_fifo_read_hwidx(struct channel_fifo *fifo,
					struct pme_regmap *map)
{
	fifo->hw2 = reg_get(map, fifo->offset_hw2);
	/* NB, these are unsigned, so we must use the if/else construct rather
	 * than subtracting to a negative and correcting. */
	if (fifo->type != fifo_notify) {
		if (fifo->sw1 >= fifo->hw2)
			fifo->fill12 = fifo->sw1 - fifo->hw2;
		else
			fifo->fill12 = (fifo->sw1 + (2 * fifo->num)) -
				fifo->hw2;
	}
	if (fifo->type != fifo_fbm) {
		if (fifo->hw2 >= fifo->sw3)
			fifo->fill23 = fifo->hw2 - fifo->sw3;
		else
			fifo->fill23 = (fifo->hw2 + (2 * fifo->num)) -
				fifo->sw3;
	}
	BUG_ON((fifo->fill12 + fifo->fill23) > fifo->num);
}

/* Initialise FIFO registers (and read index registers) */
static void channel_fifo_set(struct channel_fifo *fifo, struct pme_regmap *map)
{
	unsigned int map_idx = 0xffffffff;
	dma_addr_t addr = DMA_MAP_SINGLE(fifo->kptr,
				fifo->num * fifo->entry_size, DMA_TO_DEVICE);
	/* Choose the existing register values as initial software values */
	switch (fifo->type) {
	case fifo_cmd_reduced:
	case fifo_cmd_normal:
		map_idx = CH_CMD_FIFO_BASE_H;
		fifo->sw1 = reg_get(map, (fifo->offset_sw1 = CH_CPI));
		fifo->sw3 = reg_get(map, (fifo->offset_sw3 = CH_CEI));
		fifo->offset_hw2 = CH_CCI;
		break;
	case fifo_notify:
		map_idx = CH_NOT_FIFO_BASE_H;
		fifo->sw3 = reg_get(map, (fifo->offset_sw3 = CH_NCI));
		fifo->offset_hw2 = CH_NPI;
		break;
	case fifo_fbm:
		map_idx = CH_FB_FIFO_BASE_H;
		fifo->sw1 = reg_get(map, (fifo->offset_sw1 = CH_FBFPI));
		fifo->offset_hw2 = CH_FBFCI;
		break;
	}
	/* To avoid compiler warnings (that observe dma_addr_t to be 32-bit!),
	 * force the issue. */
	reg_set64(map, map_idx, (u64)addr);
	reg_set(map, map_idx + 2, ilog2(fifo->num) |
			/* CMD_FIFO_DEPTH also has a bit for 128-byte size */
			((fifo->type == fifo_cmd_normal) ? (1 << 8) : 0));
	reg_set(map, map_idx + 3, 0);
	/* Read the initial hardware value, which will calculate deltas against
	 * the initial software values - these deltas are zero at any time that
	 * is sane to be initialising. */
	channel_fifo_read_hwidx(fifo, map);
	BUG_ON(fifo->fill12 || fifo->fill23);
	/* When programming registers, use this an appropriate hook to (re)set
	 * the blocking to disabled. */
	fifo->block = 0;
}

/* Cleanup a FIFO */
static void channel_fifo_finish(struct channel_fifo *fifo)
{
	size_t sz = fifo->entry_size * fifo->num;
	__free_pages(fifo->page, get_order(sz));
}

/* Returns true if the CMD/FB FIFO has room */
static inline int channel_fifo_room(struct channel_fifo *fifo)
{
	int val = (fifo->num - fifo->fill12 - fifo->fill23);
	return val;
}

/* Returns pointer for formatting a new FIFO entry */
static inline void *channel_fifo_produce_ptr(struct channel_fifo *fifo)
{
	return fifo->kptr + (fifo->entry_size * (fifo->sw1 & fifo->entry_mask));
}

/* Increments CPI/FPI */
static inline void channel_fifo_produced(struct channel_fifo *fifo,
					struct pme_regmap *map)
{
	fifo->sw1 = (fifo->sw1 + 1) & fifo->idx_mask;
	if (likely(!fifo->block))
		reg_set(map, fifo->offset_sw1, fifo->sw1);
	fifo->fill12++;
}

/* Blocks/unblocks CPI/NCI/FPI */
static inline void channel_fifo_block(struct channel_fifo *fifo, int enable,
					struct pme_regmap *map)
{
	if (enable) {
		fifo->block = 1;
		return;
	}
	if (fifo->type == fifo_notify)
		reg_set(map, fifo->offset_sw3, fifo->sw3);
	else
		reg_set(map, fifo->offset_sw1, fifo->sw1);
	fifo->block = 0;
}

/* FIFO loop helper */
#define __fifo_loop(f, start, end, var, code) \
do { \
	var = f->kptr + (f->entry_size * (start & f->entry_mask)); \
	while (start != end) { \
		code; \
		start = (start + 1) & f->idx_mask; \
		if (!(start & f->entry_mask)) \
			var = f->kptr; \
		else \
			var = (void *)var + f->entry_size; \
	} \
} while (0)

/* Loop [expiring CEI->CCI | consuming NCI->NPI]. NB, the subtlety of this
 * macro is that it evaluates to an index value which must then be passed to
 * channel_fifo_expired(). In this way, the register isn't set until the loop
 * is complete and the caller can avoid locking problems for threads that wait
 * on full FIFOs, [etc]. */
#define channel_fifo_expire(fifo, var, code) \
({ \
	struct channel_fifo *__fifo = (fifo); \
	u32 __s = __fifo->sw3, __e = __fifo->hw2; \
	__fifo_loop(__fifo, __s, __e, var, code); \
	__fifo->fill23; \
})
#define channel_fifo_expired(fifo, map, val) \
do { \
	struct channel_fifo *__fifo = (fifo); \
	struct pme_regmap *__map = (map); \
	u32 __val = (val); \
	__fifo->fill23 -= __val; \
	__fifo->sw3 = (__fifo->sw3 + __val) & __fifo->idx_mask; \
	if (likely(!__fifo->block)) \
		reg_set(__map, __fifo->offset_sw3, __fifo->sw3); \
} while (0)

/* Loop abandoning CCI->CPI. */
#define channel_fifo_abandon(fifo, var, code) \
do { \
	struct channel_fifo *__fifo = (fifo); \
	u32 __cci = __fifo->hw2, __cpi = __fifo->sw1; \
	if (__cci != __cpi) \
		__fifo_loop(__fifo, __cci, __cpi, var, \
			{ code ; __fifo->fill12--; }); \
} while (0)

static void channel_fifo_dump(struct channel_fifo *f,
				struct __pme_dump_fifo *d)
{
	d->type = f->type;
	d->kptr = f->kptr;
	d->entry_size = (unsigned int)f->entry_size;
	d->num = f->num;
	d->block = f->block;
	d->sw1 = f->sw1;
	d->hw2 = f->hw2;
	d->sw3 = f->sw3;
	d->fill12 = f->fill12;
	d->fill23 = f->fill23;
}

/*****************/
/* FIFO Encoding */
/*****************/

/* This implements a get_*** and set_*** function-pair to read/write a given
 * element of a FIFO entry. 'idx' is the index to the 64-bit dword in the FIFO
 * entry where the element is found, 'offset' is how many bits the element
 * needs to be scrolled right (ie. until it's least-significant bit is the
 * least-significant bit of the result), 'width' is the number of bits wide the
 * element is, and 'type' is the C type for the get/set prototypes. 'p' is a
 * function-naming prefix to localise these functions to the type of FIFO entry
 * (eg. so command/notification/FB FIFO entries with the same names don't get
 * mismatched). Likewise 's' is the struct name used to represent these FIFO
 * entries. Between 'p' and 's', type-safety between FIFO types should be
 * ensured. */
#define CODEC_FN(s, p, name, idx, offset, width, type) \
static inline type p##_get_##name(struct s *obj) { \
	type p##name##_671 = 	(type)((obj->blk[idx] >> offset) & \
			(((u64)1 << width) - 1)); \
	return  p##name##_671; \
} \
static inline void p##_set_##name(struct s *obj, type val) { \
	obj->blk[idx] &= ~((((u64)1 << width) - 1) << offset); \
	obj->blk[idx] |= (u64)val << offset; \
}

/* Simplistic version where the element is the entire 64-bit dword. Apart from
 * being easier, this handles the degenerate case that the above macros don't,
 * namely when 'width' is 64. */
#define CODEC_BASIC(s, p, name, idx) \
static inline u64 p##_get_##name(struct s *obj) { \
	return obj->blk[idx]; \
} \
static inline void p##_set_##name(struct s *obj, u64 val) { \
	obj->blk[idx] = val; \
}

/* Non-copy version where a pointer accessor is returned. In this macro, we
 * return the u64-aligned pointer at index 'idx', and 'type' is the pointer
 * type we want (minus the '*' character). */
#define CODEC_PTR(s, p, name, idx, type) \
static inline type *p##_get_##name(struct s *obj) { \
	type *p##name##_673 = (void *)(obj->blk + idx); \
	return p##name##_673; \
}

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
 * This file implements buffer management logic for the pattern-matcher
 * driver. Buffers are managed in h/w-native linked-list format as well
 * as IEEE1212.1 scatter-gather descriptors.
 *
 */

/************/
/* fbchains */
/************/

/*
 * This is an "abstraction" that gets inlined into base_channel.c, thus
 * encapsulation isn't enforced by the compiler. This data-path code ought to
 * be a single compiler object so we can't break it into C files, yet it's
 * preferable to keep some separation.
 */

/* Linked-list definitions */
#define LL_NED_SIZE	64
#define ll_virt_get(ned) \
({ \
	struct __ll_ned *__ned = (ned); \
	u64_to_ptr(__ned->virt); \
})
static inline void ll_virt_set(struct __ll_ned *ned, void *val)
{
	ned->virt = ptr_to_u64(val);
}

/* Scatter-gather definitions */
/* The DMA engine packs SG tables with entries this wide. NB, the formatting of
 * freelist buffers into SG tables splits them in two halves, the first half
 * uses physical addresses, the second half using virtual addresses.
 * Additionally, the last entry in each half is reserved to point to a
 * subsequent table if required. */
#define SG_ENTRY_SIZE	16
#define SG_BUFFERS_PER_TABLE(blksize) \
			((blksize) / (2 * SG_ENTRY_SIZE) - 1)
#define sg_virt_get(sg) \
({ \
	struct __sg_entry *__sg = (sg); \
	u64_to_ptr(__sg->addr); \
})
static inline void sg_virt_set(struct __sg_entry *sg, void *val)
{
	sg->addr = ptr_to_u64(val);
}
static inline enum fifo_ext sg_ext_get(struct __sg_entry *sg)
{
	int val = (sg->__extoffset >> 31);
	return val;
}
static inline enum fifo_ext sg_offset_get(struct __sg_entry *sg)
{
	int val = (sg->__extoffset & 0x7fffffff);
	return val;
}

/* Format a deallocate FIFO entry. This will generate codec functions, eg;
 *   static inline u16  f_get_buf_count(struct fifo_fbm *s);
 *   static inline void f_set_buf_count(struct fifo_fbm *s, u16 val);
 */
CODEC_BASIC(fifo_fbm, f, head_phys, 0)
CODEC_BASIC(fifo_fbm, f, head_virt, 1)
CODEC_FN(fifo_fbm, f, buf_count, 2, 48, 16, u16)
CODEC_FN(fifo_fbm, f, freelist, 2, 31, 1, u8)
CODEC_FN(fifo_fbm, f, fmt, 2, 30, 1, enum fifo_fmt)
CODEC_BASIC(fifo_fbm, f, tail_phys, 3)
CODEC_BASIC(fifo_fbm, f, phys_ptr, 0)
CODEC_BASIC(fifo_fbm, f, virt_ptr, 1)
CODEC_FN(fifo_fbm, f, extension, 2, 47, 1, enum fifo_ext)

/*************************/
/* Free-buffer allocator */
/*************************/

/* We encapsulate the slab type so that we can perform special handling for
 * certain blocksizes. In particular, if the allocations will be page-aligned
 * and larger than a single page, we post-process allocations by incrementing
 * each page's reference count and pre-process deallocations with a
 * corresponding decrement. This simplifies life for zero-copy user-space
 * interfaces because multi-page allocations usually have zero ref-counts on
 * all but the first page (and so page-faulting parts of a large mapped block
 * would grab a reference, tearing down the mapping will release that
 * reference, and then the system thinks the page is being deallocated because
 * the count reaches zero ... <kaboom>). Ensuring the block pages always have a
 * non-zero count avoids this issue. */
struct fbuffer_slab {
	struct kmem_cache *slab;
	u32 blocksize;
};

static int fbuffer_create(struct fbuffer_slab *slab, u32 blocksize,
				const char *name)
{
	slab->slab = kmem_cache_create(name, blocksize, 0,
			SLAB_HWCACHE_ALIGN, NULL);
	if (!(slab->slab))
		return -ENOMEM;
	slab->blocksize = blocksize;
	return 0;
}

static void fbuffer_destroy(struct fbuffer_slab *slab)
{
	kmem_cache_destroy(slab->slab);
}

static inline void *fbuffer_alloc(struct fbuffer_slab *slab)
{
	struct page *p;
	void *ret = kmem_cache_alloc(slab->slab, GFP_KERNEL);
	u32 bs = slab->blocksize;
	if (!ret || (bs <= PAGE_SIZE) || (bs % PAGE_SIZE))
		return ret;
	p = virt_to_page(ret);
	while (bs) {
		get_page(p++);
		bs -= PAGE_SIZE;
	}
	return ret;
}

static inline void fbuffer_free(struct fbuffer_slab *slab, void *ptr)
{
	u32 bs = slab->blocksize;
	if ((bs > PAGE_SIZE) && !(bs % PAGE_SIZE)) {
		struct page *p = virt_to_page(ptr);
		while (bs) {
			put_page_testzero(p);
			p++;
			bs -= PAGE_SIZE;
		}
	}
	kmem_cache_free(slab->slab, ptr);
}

/* Internal API */
/* A scatter-gather chain keeps its iterator pointing to a data buffer, we
 * always run this fixup when it might have been left pointing to a table. */
static inline void sg_fixup(struct pme_fbchain *chain)
{
	while (sg_ext_get(chain->sg.cursor) == EXT_SCATTERGATHER)
		chain->sg.cursor = sg_virt_get(chain->sg.cursor) +
					(chain->blocksize / 2);
}

static inline struct pme_fbchain *fbchain_alloc(struct pme_channel *c,
							unsigned int gfp_flags)
{
	struct pme_fbchain *ret = kmalloc(sizeof(*ret), gfp_flags);
	if (!ret)
		return NULL;
	ret->channel = c;
	return ret;
}

static inline struct pme_fbchain *fbchain_alloc_ll(struct pme_channel *c,
			u8 freelist_id, void **batch, unsigned int batch_num)
{
	unsigned int loop;
	struct __ll_ned *ned = NULL;
	/* Only used from the freelist thread, use GFP_KERNEL */
	struct pme_fbchain *ret = fbchain_alloc(c, GFP_KERNEL);
	if (!ret)
		return NULL;
	ret->freelist_id = freelist_id;
	/* We use the 'null' type as an internal indicator that the payload
	 * consists of newly-allocated buffers rather than recycled from
	 * earlier notifications. (The FB thread can track it within the
	 * release_list and use it to throttle the low-water threshold
	 * interrupt.) */
	ret->type = fbchain_null;
	ret->num_blocks = batch_num;
	ll_virt_set(&ret->ll.head, batch[0]);
	ret->ll.head.phys = (u64)DMA_MAP_SINGLE(batch[0], 1, DMA_TO_DEVICE);
	for (loop = 1; loop < batch_num; loop++) {
		ned = batch[loop - 1];
		ll_virt_set(ned, batch[loop]);
		ned->phys = (u64)DMA_MAP_SINGLE(batch[loop], 1, DMA_TO_DEVICE);
	}
	if (ned) {
		/* Grab the tail entries from the last NED */
		ret->ll.tail_virt = ll_virt_get(ned);
		ret->ll.tail_phys = ned->phys;
	} else {
		/* The tail is the head */
		ret->ll.tail_virt = batch[0];
		ret->ll.tail_phys = ret->ll.head.phys;
	}
	return ret;
}

static inline void fbchain_setup_ll(struct pme_fbchain *chain,
				u64 head_virt, u64 head_phys, u64 tail_virt,
				u32 offset, u32 bufflen)
{
	chain->type = fbchain_ll;
	chain->ll.head.virt = head_virt;
	chain->ll.head.phys = head_phys;
	chain->ll.head.offset = offset;
	chain->ll.head.bufflen = bufflen;
	chain->ll.tail_virt = u64_to_ptr(tail_virt);
	chain->ll.tail_phys = (u64)DMA_MAP_SINGLE(chain->ll.tail_virt, 1,
						DMA_FROM_DEVICE);
	if (chain->num_blocks)
		chain->ll.cursor = &chain->ll.head;
	else
		chain->ll.cursor = NULL;
	chain->ll.idx = 0;
	chain->ll.bytes = 0;
}

static inline void fbchain_setup_sg(struct pme_fbchain *chain, u64 virt,
				u64 phys, enum fifo_ext ext, u32 tbl_count,
				u32 offset, u32 bufflen)
{
	chain->type = fbchain_sg;
	chain->sg.head.addr = virt;
	chain->sg.head.bufflen = bufflen;
	chain->sg.head.__extoffset = offset & 0x7fffffff;
	if (ext == EXT_SCATTERGATHER)
		chain->sg.head.__extoffset |= 0x80000000;
	chain->sg.phys = phys;
	chain->sg.tbls = tbl_count;
	chain->sg.blks_left = chain->num_blocks - chain->sg.tbls;
	chain->sg.bytes = 0;
	if (chain->num_blocks) {
		chain->sg.cursor = &chain->sg.head;
		sg_fixup(chain);
	} else
		chain->sg.cursor = NULL;
}

static inline void fbchain_free(struct pme_fbchain *chain)
{
	kfree(chain);
}

/* format a sg/ll chain onto the FB deallocate FIFO */
static inline int fbchain_send(struct pme_fbchain *chain,
				struct fifo_fbm *fifo)
{
	int ret;
	f_set_freelist(fifo, chain->freelist_id);
	f_set_buf_count(fifo, chain->num_blocks);
	if (chain->type == fbchain_sg) {
		f_set_fmt(fifo, BV);
		f_set_virt_ptr(fifo, chain->sg.head.addr);
		f_set_phys_ptr(fifo, chain->sg.phys);
		f_set_extension(fifo, sg_ext_get(&chain->sg.head));
		f_set_buf_count(fifo, chain->num_blocks);
		f_set_tail_phys(fifo, 0x0);
	} else {
		f_set_fmt(fifo, LL);
		f_set_head_phys(fifo, chain->ll.head.phys);
		f_set_head_virt(fifo, chain->ll.head.virt);
		f_set_tail_phys(fifo, chain->ll.tail_phys);
	}
	/* Return non-zero if this was a low-water allocation */
	ret = (chain->type == fbchain_null);
	/* Free the chain */
	fbchain_free(chain);
	return ret;
}

/* iterate a linked-list of buffers deallocating them to a slab */
static inline void nochain_free_all(struct fbuffer_slab *slab, u64 head_virt,
				u64 head_phys, u64 tail_phys, u32 num)
{
	struct __ll_ned ned;
	while (num--) {
		memcpy(&ned, u64_to_ptr(head_virt), sizeof(ned));
		fbuffer_free(slab, u64_to_ptr(head_virt));
		head_virt = ned.virt;
		head_phys = ned.phys;
	}
}

/* helper for fbchain_free_all() */
static inline void __sgchain_block_free(struct __sg_entry *sg,
					unsigned int num, size_t blksize,
					struct fbuffer_slab *slab)
{
	struct __sg_entry *virt;
	void *prevtable;
	/* We need to free everything that 'sg' refers to. If that's a data
	 * block, we free it and we're done - if it's a table, we iterate the
	 * table entries and if there's a link to another table, we destroy the
	 * table block before moving to the subsequent table and continuing. */
	if (!sg_ext_get(sg)) {
		fbuffer_free(slab, sg_virt_get(sg));
		num--;
		return;
	}
loop:
	/* sg refers to a table, iterate the virtual (latter) half */
	virt = (prevtable = sg_virt_get(sg)) + (blksize / 2);
	while (num && (sg_ext_get(virt) == EXT_CONTIGUOUS)) {
		fbuffer_free(slab, sg_virt_get(virt++));
		num--;
	}
	/* Either we're finished or we've reached a table jump entry. Assume the
	 * latter by copying the link entry before deallocating the current
	 * table - but then only proceed if there were buffers remaining. */
	memcpy(sg, virt, sizeof(*sg));
	fbuffer_free(slab, prevtable);
	if (num)
		goto loop;
}
/* iterate a sg/ll chain deallocating to a slab */
static inline void fbchain_free_all(struct pme_fbchain *chain,
					struct fbuffer_slab *slab)
{
	if (chain->type == fbchain_sg) {
		/* Pass off to a helper here. */
		__sgchain_block_free(&chain->sg.head, chain->num_blocks -
				chain->sg.tbls, chain->blocksize, slab);
	} else {
		while (chain->num_blocks--) {
			void *ptr = ll_virt_get(&chain->ll.head);
			memcpy(&chain->ll.head, ptr, sizeof(struct __ll_ned));
			fbuffer_free(slab, ptr);
		}
	}
	fbchain_free(chain);
}

/**********************/
/* pme_fbchain API */
/**********************/

enum pme_fbchain_e pme_fbchain_type(struct pme_fbchain *chain)
{
	return chain->type;
}
EXPORT_SYMBOL(pme_fbchain_type);

size_t pme_fbchain_max(struct pme_fbchain *chain)
{
	if (chain->type == fbchain_ll)
		return chain->blocksize - LL_NED_SIZE;
	return chain->blocksize;
}
EXPORT_SYMBOL(pme_fbchain_max);

unsigned int pme_fbchain_num(struct pme_fbchain *chain)
{
	if (chain->type == fbchain_ll)
		return chain->num_blocks;
	return chain->num_blocks - chain->sg.tbls;
};
EXPORT_SYMBOL(pme_fbchain_num);

size_t pme_fbchain_length(struct pme_fbchain *chain)
{
	return chain->data_len;
}
EXPORT_SYMBOL(pme_fbchain_length);

void *pme_fbchain_current(struct pme_fbchain *chain)
{
	if (chain->type == fbchain_ll) {
		if (!chain->ll.cursor)
			return NULL;
		return ll_virt_get(chain->ll.cursor) + LL_NED_SIZE;
	}
	if (!chain->sg.cursor)
		return NULL;
	/* the cursor should always be on a data buffer entry */
	return sg_virt_get(chain->sg.cursor);
}
EXPORT_SYMBOL(pme_fbchain_current);

void *pme_fbchain_next(struct pme_fbchain *chain)
{
	if (chain->type == fbchain_ll) {
		if (!chain->ll.cursor)
			return NULL;
		chain->ll.idx++;
		chain->ll.bytes += chain->ll.cursor->bufflen;
		if (ll_virt_get(chain->ll.cursor) == chain->ll.tail_virt) {
			chain->ll.cursor = NULL;
			return NULL;
		}
		chain->ll.cursor = ll_virt_get(chain->ll.cursor);
		return pme_fbchain_current(chain);
	}
	if (!chain->sg.cursor)
		return NULL;
	chain->sg.bytes += chain->sg.cursor->bufflen;
	/* If this was the last data block, EOL */
	if (!--chain->sg.blks_left) {
		chain->sg.cursor = NULL;
		return NULL;
	}
	/* Increment the cursor pointer to the next table location and ensure
	 * we are not pointing to a link entry. */
	chain->sg.cursor++;
	sg_fixup(chain);
	return pme_fbchain_current(chain);
}
EXPORT_SYMBOL(pme_fbchain_next);

void pme_fbchain_reset(struct pme_fbchain *chain)
{
	if (chain->type == fbchain_ll) {
		chain->ll.idx = 0;
		chain->ll.bytes = 0;
		if (!chain->num_blocks)
			chain->ll.cursor = NULL;
		else
			chain->ll.cursor = &chain->ll.head;
		return;
	}
	chain->sg.blks_left = chain->num_blocks - chain->sg.tbls;
	chain->sg.bytes = 0;
	if (!chain->sg.blks_left) {
		chain->sg.cursor = NULL;
		return;
	}
	chain->sg.cursor = &chain->sg.head;
	sg_fixup(chain);
}
EXPORT_SYMBOL(pme_fbchain_reset);

struct pme_fbchain *pme_fbchain_new(struct pme_fbchain *compat,
						unsigned int gfp_flags)
{
	struct pme_fbchain *ret;
	/* We must necessarily be of the linked-list type */
	if (compat->type != fbchain_ll)
		return NULL;
	ret = fbchain_alloc(compat->channel, gfp_flags);
	if (!ret)
		return NULL;
	ret->freelist_id = compat->freelist_id;
	ret->type = compat->type;
	ret->data_len = 0;
	ret->num_blocks = 0;
	ret->blocksize = compat->blocksize;
	ret->ll.head.virt = 0;
	ret->ll.head.phys = 0;
	ret->ll.tail_virt = NULL;
	ret->ll.tail_phys = 0;
	ret->ll.idx = 0;
	ret->ll.bytes = 0;
	ret->ll.cursor = NULL;
	return ret;
}
EXPORT_SYMBOL(pme_fbchain_new);

int pme_fbchain_is_compat(const struct pme_fbchain *chain1,
				const struct pme_fbchain *chain2)
{
	int val = ((chain1->channel == chain2->channel) &&
		(chain1->type == chain2->type) &&
		(chain1->freelist_id == chain2->freelist_id));
	return val;
}
EXPORT_SYMBOL(pme_fbchain_is_compat);

int pme_fbchain_mv(struct pme_fbchain *dest,
			struct pme_fbchain *source)
{
	if ((dest->channel != source->channel) ||
			(dest->freelist_id != source->freelist_id) ||
			(dest->type != source->type))
		return -EINVAL;
	if (dest->type != fbchain_ll)
		return -EINVAL;
	if (!source->num_blocks)
		return 0;
	if (!dest->num_blocks) {
		dest->ll.head.virt = source->ll.head.virt;
		dest->ll.head.phys = source->ll.head.phys;
		dest->ll.tail_virt = source->ll.tail_virt;
		dest->ll.tail_phys = source->ll.tail_phys;
		memcpy(&dest->ll.head, &source->ll.head, sizeof(dest->ll.head));
		dest->ll.cursor = &dest->ll.head;
	} else {
		/* Write source's head onto dest's tail */
		memcpy(dest->ll.tail_virt, &source->ll.head,
				sizeof(source->ll.head));
		/* If dest's cursor was EOF, point it to the new entries. */
		if (!dest->ll.cursor)
			dest->ll.cursor = dest->ll.tail_virt;
		dest->ll.tail_virt = source->ll.tail_virt;
		dest->ll.tail_phys = source->ll.tail_phys;
	}
	dest->data_len += source->data_len;
	dest->num_blocks += source->num_blocks;
	source->data_len = 0;
	source->num_blocks = 0;
	source->ll.cursor = NULL;
	return 0;
}
EXPORT_SYMBOL(pme_fbchain_mv);

int pme_fbchain_crop(struct pme_fbchain *dest,
				struct pme_fbchain *source)
{
	if ((dest->channel != source->channel) ||
			(dest->freelist_id != source->freelist_id) ||
			(dest->type != source->type))
		return -EINVAL;
	if (dest->type != fbchain_ll)
		return -EINVAL;
	if (!source->ll.idx)
		return 0;
	if (!source->ll.cursor)
		return pme_fbchain_mv(dest, source);
	if (!dest->num_blocks) {
		dest->ll.head.virt = source->ll.head.virt;
		dest->ll.head.phys = source->ll.head.phys;
		dest->ll.tail_virt = source->ll.cursor;
		dest->ll.tail_phys = (u64)DMA_MAP_SINGLE(source->ll.cursor, 1,
							DMA_TO_DEVICE);
		memcpy(&dest->ll.head, &source->ll.head, sizeof(dest->ll.head));
		dest->ll.cursor = &dest->ll.head;
	} else {
		/* Write source's head onto dest's tail */
		memcpy(dest->ll.tail_virt, &source->ll.head,
				sizeof(source->ll.head));
		/* If dest's cursor was EOF, point it to the new entries. */
		if (!dest->ll.cursor)
			dest->ll.cursor = dest->ll.tail_virt;
		dest->ll.tail_virt = source->ll.cursor;
		dest->ll.tail_phys = (u64)DMA_MAP_SINGLE(source->ll.cursor, 1,
							DMA_TO_DEVICE);
	}
	dest->num_blocks += source->ll.idx;
	source->num_blocks -= source->ll.idx;
	dest->data_len += source->ll.bytes;
	source->data_len -= source->ll.bytes;
	source->ll.head.virt = source->ll.cursor->virt;
	source->ll.head.phys = source->ll.cursor->phys;
	source->ll.idx = 0;
	source->ll.bytes = 0;
	if (source->num_blocks)
		/* The head of 'source' comes from the cursor */
		memcpy(&source->ll.head, source->ll.cursor,
				sizeof(source->ll.head));
	source->ll.cursor = &source->ll.head;
	return 0;
}
EXPORT_SYMBOL(pme_fbchain_crop);

size_t pme_fbchain_current_bufflen(struct pme_fbchain *chain)
{
	/* For NULL iterators, simply return zero */
	if (chain->type == fbchain_ll) {
		if (!chain->ll.cursor)
			return 0;
		return chain->ll.cursor->bufflen;
	}
	if (!chain->sg.cursor)
		return 0;
	return chain->sg.cursor->bufflen;
}
EXPORT_SYMBOL(pme_fbchain_current_bufflen);

int pme_fbchain_crop_recycle(struct pme_fbchain *chain)
{
	int dbg;
	struct pme_fbchain *tmp;
	if (chain->type != fbchain_ll)
		return -EINVAL;
	if (!chain->ll.idx)
		return 0;
	tmp = pme_fbchain_new(chain, in_atomic() ? GFP_ATOMIC : GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;
	dbg = pme_fbchain_crop(tmp, chain);
	pme_fbchain_recycle(tmp);
	return 0;
}
EXPORT_SYMBOL(pme_fbchain_crop_recycle);

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
 * This file provides a slab-allocator-based pool of scatter-gather tables to
 * assist in dynamically building zero-copy descriptors for user-space data.
 */

#include "user_private.h"

/* This file implements a pool of sg_pool_entries,
 * which are used to construct scatter/gather tables
 * as needed.
 * The entry size,_SGTABLE_ENTRY_COUNT, defines
 * how many data pointers are stored in a SG Table
 * before an extention entry is used.  That value
 * should be tuned according to the most optimal
 * ratio of data to extention entries */
static struct kmem_cache *_sg_pool_cache;

/* A pme_data structure can only reference up to 64Kb data
 * Therefore, the maximum size of a scatter/gather table is 64Kb*/
#define MAX_SCATTER_GATHER_ENTRY_SIZE		65536

/* The number of Scatter/Gather entries per pool entry
 * Setting this value too big will waste space if most
 * S/G Tables need few entries
 * Of course, making it too small will cause the overhead of
 * linking the tables togther too large.
 * And the value can't exceed 64K due to HW Limitations */
#define _SGTABLE_ENTRY_COUNT	15

/* Extension values */
#define _EXTENSION_CONTIG	0x0
#define _EXTENSION_SG		0x1

/* Scatter/Gather table entry as per the block guide */
struct _sg_entry {
	u64 address;
	u32 bufflen;
	u32 extension:1;
	u32 reserved2:14;
	u32 offset:17;
};

/* Defines a structure used to set up scatter/gather tables */
struct _sg_table_entry {
	/* The extra entry is so we can always link
	 * them together.  This field must be first
	 * in the structure so it is properly aligned */
	struct _sg_entry entries[_SGTABLE_ENTRY_COUNT + 1];
	dma_addr_t dma_addr;
	u8 num_used;
	struct list_head list;
};

/* Initalize the pool */
int pme_sg_tbl_pool_init(void)
{
	int sg_size = sizeof(struct _sg_entry) * (_SGTABLE_ENTRY_COUNT + 1);

	/* Fail to initialize if the scatter/gather entry is larger than
	 * the DMA Engine can handle.  This should ripple back to cause
	 * the module load to fail */
	if (sg_size > MAX_SCATTER_GATHER_ENTRY_SIZE) {
		printk(KERN_ERR PMMOD
		       "Scatter/Gather tables must be less than %d,"
		       " current value is %d\n",
		       MAX_SCATTER_GATHER_ENTRY_SIZE, sg_size);
		return -EINVAL;
	}
	_sg_pool_cache = kmem_cache_create("pme_mem_scatter_gather",
			sizeof(struct _sg_table_entry), 0,
			SLAB_HWCACHE_ALIGN, NULL);
	if (!_sg_pool_cache)
		return -ENOMEM;
	return 0;
}

/* Release the pool */
void pme_sg_tbl_pool_destroy(void)
{
	kmem_cache_destroy(_sg_pool_cache);
}

/* Allocate a Scatter/Gather entry from the kmem_cache */
struct _sg_table_entry *pme_sg_table_entry_new(void)
{
	struct _sg_table_entry *pme_entry =
		kmem_cache_alloc(_sg_pool_cache, GFP_KERNEL);
	if (likely(pme_entry)) {
		pme_entry->dma_addr = DMA_MAP_SINGLE(pme_entry->entries,
						sizeof(pme_entry->entries),
						DMA_TO_DEVICE);
		pme_entry->num_used = 0;
		INIT_LIST_HEAD(&pme_entry->list);
	}
	return pme_entry;
}

/* Link 2 scatter gather entries (first will point to second) */
static void pme_sg_table_entry_link(struct _sg_table_entry *first,
				struct _sg_table_entry *second)
{
	first->entries[_SGTABLE_ENTRY_COUNT].address = second->dma_addr;
	first->entries[_SGTABLE_ENTRY_COUNT].extension = _EXTENSION_SG;
	first->entries[_SGTABLE_ENTRY_COUNT].offset = 0;
	first->entries[_SGTABLE_ENTRY_COUNT].bufflen = 0;
	first->entries[_SGTABLE_ENTRY_COUNT].reserved2 = 0;
}

#define __PREV_ENTRY(entry) \
	list_entry(entry->list.prev, struct _sg_table_entry, list)

/* Returns the next avaliable entry in this pool structure,
 * expanding the pool if needed */
struct _sg_entry *pme_sg_table_entry_grow(struct list_head *list)
{
	struct _sg_table_entry *new, *entry;
	if (list_empty(list)) {
		entry = pme_sg_table_entry_new();
		if (!entry)
			return NULL;
		list_add_tail(&entry->list, list);
	}
	entry = list_entry(list->prev, struct _sg_table_entry, list);
	if (likely(entry->num_used < _SGTABLE_ENTRY_COUNT)) {
		/* If there is a previous S/G Table Increase its size */
		if (entry->list.prev != list)
			__PREV_ENTRY(entry)->
				entries[_SGTABLE_ENTRY_COUNT].bufflen +=
					sizeof(struct _sg_entry);
		return &entry->entries[entry->num_used++];
	}
	/* This pool entry is full, allocate another */
	new = pme_sg_table_entry_new();
	if (!new)
		return NULL;
	list_add_tail(&new->list, list);
	pme_sg_table_entry_link(entry, new);
	new->num_used++;
	/* Increase the  byte size of the previous entry */
	entry->entries[_SGTABLE_ENTRY_COUNT].bufflen +=
		sizeof(struct _sg_entry);
	/* If the prev has a previous, we need to increase its size as well */
	if (entry->list.prev != list)
		__PREV_ENTRY(entry)->entries[_SGTABLE_ENTRY_COUNT].bufflen +=
				sizeof(struct _sg_entry);
	return &new->entries[0];
}

int pme_sg_list_add(struct list_head *list, dma_addr_t address, size_t size)
{
	struct _sg_entry *target = pme_sg_table_entry_grow(list);
	if (!target)
		return -ENOMEM;
	target->address = address;
	target->bufflen = size;
	target->offset = 0;
	target->extension = _EXTENSION_CONTIG;
	return 0;
}

/* Puts an sg_entry back into the kmem_cache */
void pme_sg_table_entry_free(struct list_head *list)
{
	struct _sg_table_entry *entry , *prev;
	int i, used, remaining;
	struct _sg_entry *sg_entry;
	dma_addr_t pos;

	list_for_each_entry_safe(entry, prev, list, list) {
		DMA_UNMAP_SINGLE(entry->dma_addr, sizeof(entry->entries),
					DMA_TO_DEVICE);
		used = entry->num_used;
		sg_entry = entry->entries;
		for (i = 0; i < used; i++) {
			pos = sg_entry->address & PAGE_MASK;
			remaining = sg_entry->bufflen +
				offset_in_page(sg_entry->address);
			while (remaining > 0) {
				DMA_UNMAP_PAGE(pos, PAGE_SIZE, DMA_TO_DEVICE);
				page_cache_release(pfn_to_page(pos >>
							PAGE_SHIFT));
				pos += PAGE_SIZE;
				remaining -= PAGE_SIZE;
			}
			++sg_entry;
		}
		list_del(&entry->list);
		kmem_cache_free(_sg_pool_cache, entry);
	}
}

static inline enum pme_data_type get_data_type(int direction)
{
	int val = (direction == DMA_TO_DEVICE ? data_in_sg : data_out_sg);
	return val;
}

void pme_sg_complete(struct pme_mem_mapping *result, size_t total_size,
			int direction)
{
	struct _sg_table_entry *entry =
		list_entry(result->list.next, struct _sg_table_entry, list);

	result->data.addr = entry->dma_addr;
	if (entry->list.next != &result->list)
		result->data.size = (sizeof(struct _sg_entry) *
			(_SGTABLE_ENTRY_COUNT + 1));
	else
		result->data.size = (sizeof(struct _sg_entry) *
					entry->num_used);
	result->data.length = total_size;
	result->data.type = get_data_type(direction);
}

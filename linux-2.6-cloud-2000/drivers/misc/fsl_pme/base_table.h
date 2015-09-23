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
 * This file implements an encapsulation of the per-channel "tables" for both
 * context and residue resources. It hides the behavioural difference between
 * dynamic and tabular modes, where the former allocates resources directly
 * from the kernel memory allocator (and so is address-based) whereas the
 * latter allocates unused array entries from a pre-allocated table in
 * physically contiguous memory (and so is index-based). The dynamic mode is
 * larger a wrapper for a slab-allocator object, the tabular mode manages a
 * bit-field using the kernel's own optimised bitfield routines.
 *
 */

/*****************/
/* channel_table */
/*****************/

/*
 * This is an "abstraction" that gets inlined into channel.c, thus
 * encapsulation isn't enforced by the compiler.
 */

/* This structure is used within channel objects to contextualise residue and
 * context table manipulations. */
struct channel_table {
	/* Channel that owns us */
	struct pme_channel *channel;
	/* We need this for the slab */
	char		unique_name[TABLE_NAME_LEN];
	/* Size (in bytes) of each table entry */
	size_t		size;
	/* Number of entries in the table. If zero, the table isn't used and we
	 * use dynamic allocation through 'kmem'. */
	unsigned int	num;
	/* A slab allocator if we're using dynamic allocation. */
	struct kmem_cache *kmem;
	/* Number of entries in the table that are owned. */
	unsigned int	used;
	/* Base address of the allocated table */
	dma_addr_t	base;
	/* Allocation optimisation. When 'used' is less than 'num', it is the
	 * index of an available table entry. Otherwise the table is full and
	 * this will be set as soon as the next deallocation occurs. */
	unsigned int	next_alloc;
	/* A bit-field representation of the table usage. */
	unsigned long	*bitfield;
};

static int channel_table_init(struct channel_table *table,
			struct pme_channel *channel,
			const char *type, unsigned int idx,
			size_t size, unsigned int num)
{
	table->channel = channel;
	snprintf(table->unique_name, sizeof(table->unique_name),
			"pme_channel_%d_%s", idx, type);
	table->size = size;
	table->num = num;
	table->used = 0;
	if (!table->size || (table->size % PME_DMA_CACHELINE)) {
		printk(KERN_ERR PMMOD "table entry size %d is invalid\n",
					(int)table->size);
		return -EINVAL;
	}
	/* If dynamic, we defer to a slab */
	if (!table->num) {
		/* Set up our own slab allocator for cache-alignment */
		table->kmem = kmem_cache_create(table->unique_name, size, 0,
					SLAB_HWCACHE_ALIGN, NULL);
		if (!table->kmem) {
			printk(KERN_ERR PMMOD "slab setup failed\n");
			return -ENOMEM;
		}
		return 0;
	}
	/* Otherwise we manage table allocation from a block of memory */
	if (table->num & (sizeof(unsigned long) - 1)) {
		printk(KERN_ERR PMMOD "table size %d is not bitfield aligned\n",
					table->num);
		return -EINVAL;
	}
	if (!ISEXP2(table->num)) {
		printk(KERN_ERR PMMOD "table size %d is not a power of 2\n",
					table->num);
		return -EINVAL;
	}
	/* Allocate resources and initialise */
	table->bitfield = kmalloc(table->num / 8, GFP_KERNEL);
	if (!table->bitfield)
		return -ENOMEM;
	table->base = pme_mem_phys_alloc(table->num * table->size);
	if (!table->base) {
		kfree(table->bitfield);
		return -ENOMEM;
	}
	table->next_alloc = 0;
	memset(table->bitfield, 0, table->num / 8);
	return 0;
}

static void channel_table_set(struct channel_table *table,
			struct pme_regmap *map, unsigned int map_idx)
{
	/* We perform residue_size conversion irrespective of whether this is
	 * the residue table. The corresponding bits for the context table
	 * register are unused and so the write to those bits has no effect.
	 * The encoding of the size bits is as follows;
	 *          0->32, 1->64, 2->96, 3->128
	 */
	u32 szbits = ((table->size >> 5) - 1) & 3;
	if (!table->num)
		/* Non-tabular, just set the block size */
		reg_set(map, map_idx, szbits << 16);
	else {
		/* Tabular, set block size, table size, and the tabular bit */
		reg_set(map, map_idx, (szbits << 16) |
				(ilog2(table->num) << 8) | 1);
		reg_set64(map, map_idx + 1, (u64)table->base);
	}
}

static void channel_table_finish(struct channel_table *table)
{
	/* Do nothing if the table's dynamic */
	if (table->num) {
		pme_mem_phys_free(table->base);
		kfree(table->bitfield);
	} else
		kmem_cache_destroy(table->kmem);
}

static inline int channel_table_alloc(struct channel_table *table,
					struct __pme_dma_resource *res,
					unsigned int gfp_flags)
{
	int ret;
	if (!table->num) {
		/* Dynamic allocation */
		res->ptr = kmem_cache_alloc(table->kmem, gfp_flags);
		if (!res->ptr)
			return -ENOMEM;
		res->addr = DMA_MAP_SINGLE(res->ptr, table->size,
					DMA_FROM_DEVICE);
		table->used++;
		return 0;
	}
	/* Table-based allocation */
	if (table->used == table->num)
		return -ENOMEM;
	/* 'next_alloc' locates the available entry */
	ret = __test_and_set_bit(table->next_alloc, table->bitfield);
	/* In the tabular case, we pass the caller the table index rather than
	 * the hardware address. */
	res->addr = (dma_addr_t)table->next_alloc;
	if (++table->used == table->num)
		/* Table is now full, no next_alloc */
		return 0;
	/* Find the next_alloc */
	table->next_alloc = find_next_zero_bit(table->bitfield, table->num,
						table->next_alloc);
	if (table->next_alloc >= table->num)
		table->next_alloc = find_first_zero_bit(table->bitfield,
							table->num);
	return 0;
}

static inline void channel_table_free(struct channel_table *table,
					struct __pme_dma_resource *res)
{
	int ret;
	unsigned int offset;
	if (!table->num) {
		DMA_UNMAP_SINGLE(res->addr, table->size, DMA_FROM_DEVICE);
		kmem_cache_free(table->kmem, res->ptr);
		table->used--;
		goto done;
	}
	/* Table-based allocation */
	offset = (unsigned int)res->addr;
	ret = __test_and_clear_bit(offset, table->bitfield);
	if (table->used-- == table->num)
		/* If the table was full, this becomes our next_alloc */
		table->next_alloc = offset;
done:
	pme_dma_resource_init(res);
}

static void channel_table_dump(struct channel_table *t,
				struct __pme_dump_table *d)
{
	memcpy(d->unique_name, t->unique_name, sizeof(d->unique_name));
	d->entry_size = (unsigned int)t->size;
	d->table_num = t->num;
	d->owned = t->used;
}

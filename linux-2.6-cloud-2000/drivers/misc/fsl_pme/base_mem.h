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
 * This file implements support for allocation of physically-contiguous memory
 * for the pattern-matcher driver. Memory must be preallocated during boot
 * using the fsl_pme_8572_pages=<num> parameter, this code merely imposes an
 * allocator on that memory for allocating PM tables.
 *
 */

/*
 * This is an "abstraction" that gets inlined into base_module.c, thus
 * encapsulation isn't enforced by the compiler. It gives us support for
 * physically contiguous memory carved out using the
 * "fsl_pme_8572_pages=<pages>" kernel boot parameter.
 */

struct phys_chunk {
	struct list_head list;
	dma_addr_t addr;
	size_t sz;
};
static LIST_HEAD(__physmem_list);

/* Start of physically reserved memory. Based on the "fsl_pme_8572_pages=..."
 * boot parameter. */
static void *__physmem_kaddr_start;
static dma_addr_t __physmem_dma_start;
static unsigned long __physmem_size;

/* The platform/SoC code exports these hooks to access pre-allocated memory */
extern int fsl_pme_8572_num_pages(void);
extern void *fsl_pme_8572_mem(void);

/**************************************************/
/* Interface - init/finish is private to module.c */
/**************************************************/

static void pme_mem_init(void)
{
	__physmem_size = fsl_pme_8572_num_pages() << PAGE_SHIFT;
	__physmem_kaddr_start = fsl_pme_8572_mem();
	if (__physmem_size) {
		__physmem_dma_start = DMA_MAP_SINGLE(__physmem_kaddr_start,
				__physmem_size, DMA_TO_DEVICE);
		printk(KERN_INFO PMMOD "enabling region at %p:0x%08lx\n",
				__physmem_kaddr_start, __physmem_size);
	}
}

static void pme_mem_finish(void)
{
}

/***********************************/
/* Alloc/free - visible to pme_base */
/***********************************/

dma_addr_t pme_mem_phys_alloc(size_t sz)
{
	struct list_head *list;
	unsigned long tmp, hole = __physmem_dma_start;
	struct phys_chunk *next, *chunk = kmalloc(sizeof(struct phys_chunk),
						GFP_KERNEL);
	if (!chunk)
		return 0;
	sz = ((sz + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

	/* Search the existing items checking if there's a large enough hole */
	list_for_each(list, &__physmem_list) {
		next = list_entry(list, struct phys_chunk, list);
		tmp = next->addr;
		if ((tmp - hole) >= sz)
			goto found;
		/* Move our search to the tail of 'next' */
		hole = tmp + next->sz;
	}
	/* No holes, we append to the end of the list */
	next = NULL;
found:
	/* If this allocation would take us out of range, give up */
	if ((hole + sz) > (__physmem_dma_start + __physmem_size)) {
		printk(KERN_ERR PMMOD "failed to allocate 0x%08x bytes\n",
				(int)sz);
		kfree(chunk);
		return 0;
	}
	chunk->addr = hole;
	chunk->sz = sz;
	if (next)
		/* Insert prior to 'next' */
		list_add(&chunk->list, next->list.prev);
	else
		list_add_tail(&chunk->list, &__physmem_list);
	memset(__physmem_kaddr_start + (hole - __physmem_dma_start), 0, sz);
	printk(KERN_INFO PMMOD "allocation: 0x%08lx:0x%08x\n",
			(unsigned long)chunk->addr, (int)sz);
	return chunk->addr;
}

void pme_mem_phys_free(dma_addr_t addr)
{
	struct list_head *list;
	struct phys_chunk *chunk;
	list_for_each(list, &__physmem_list) {
		chunk = list_entry(list, struct phys_chunk, list);
		if (chunk->addr == addr)
			goto found;
	}
	panic("non matching address!\n");
	return;
found:
	printk(KERN_INFO PMMOD "deallocation: 0x%08lx:0x%08x\n",
			(unsigned long)chunk->addr, (int)chunk->sz);
	list_del(&chunk->list);
	kfree(chunk);
}

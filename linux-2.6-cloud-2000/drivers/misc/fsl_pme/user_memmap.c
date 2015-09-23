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
 * This file implements zero-copy mapping of user-space data descriptors for
 * passing to pattern-matcher commands.
 */

#include "user_private.h"

/* This file implements the logic to build pme_data structures
 * when given user space memory */

/* Returns the absolute maximum number of pages a block of
 * data can be occupying */
static inline int calculate_max_pages(unsigned long user_addr, int size)
{
	return (size + offset_in_page(user_addr) + PAGE_SIZE - 1) >> PAGE_SHIFT;
}

#define LOCAL_PAGE_LIST_SIZE 16

/* Function used to map a process' memory into a set of physical addesses.
 * Returns the number of result buffers used */
static int map_user_buf_local(unsigned long user_addr,
			      int size, struct list_head *list,
			      int direction)
{
	unsigned long offset;
	dma_addr_t paddr = 0;
	struct page *page_list[LOCAL_PAGE_LIST_SIZE];
	int total_pages, page_list_used, i, ret;
	int total_pages_used = 0, sg_list_size = 0;
	size_t mapped_size;

	/* Calclulate the offset in the page of the users pointer */
	offset = offset_in_page(user_addr);
	total_pages = calculate_max_pages(user_addr, size);

	/* Loop until we've mapped every page */
	while (total_pages_used != total_pages) {
		/* Get the user pages for this buffer */
		down_read(&current->mm->mmap_sem);
		page_list_used = get_user_pages(current,
						current->mm,
						(user_addr & PAGE_MASK) +
						(total_pages_used * PAGE_SIZE),
						min(LOCAL_PAGE_LIST_SIZE,
						    total_pages -
						    total_pages_used),
						direction, 0, page_list, NULL);
		up_read(&current->mm->mmap_sem);
		if (page_list_used < 1)
			/* Something is wrong with the users memory space
			 * Cleanup will be performed when the caller releases
			 * the entry the pages were attached to */
			return -EFAULT;
		for (i = 0; i < page_list_used; i++) {
			/* Compute the dma address of the page
			 * No need to map the page into the kernel, since
			 * we don't look at the data. (NB, we don't use the
			 * DMA_MAP_PAGE() wrapper because we're not dealing
			 * with any PCI-vs-SoC distinction). */
			mapped_size = PAGE_SIZE;
			paddr = DMA_MAP_PAGE(page_list[i], 0, PAGE_SIZE,
						direction);
			/* We need to apply the offset to the
			 * first and last buffers */
			if (!total_pages_used && !i) {
				/* First mapping */
				paddr += offset;
				mapped_size -= offset;
			}
			if ((page_list_used == (i+1)) &&
			   (page_list_used + total_pages_used == total_pages)) {
				/* Adjust the size of the last SG entry. The
				 * total size of the last block is: (Size of a
				 * page) - (Size of all pages) - offset -
				 * sizeof data */
				mapped_size -= (total_pages * PAGE_SIZE)
					- offset - size;
			}
			/* This need to be an entry in the SG List */
			ret = pme_sg_list_add(list, paddr, mapped_size);
			if (ret) {
				/* We need to do a page_cache_release on all
				 * pages that haven't been mapped yet
				 * Other unmapping will be done when the
				 * caller releases the entry structure */
				while (i < page_list_used)
					page_cache_release(page_list[i++]);
				return ret;
			}
			++sg_list_size;
		}
		total_pages_used += page_list_used;
	}
	return sg_list_size;
}

/* Maps a user buffer into a pme_data structure.
 * The pme_data structure need to be freed once the
 * user is done with it */
int pme_mem_map(unsigned long user_addr, size_t size,
			   struct pme_mem_mapping *result, int direction)
{
	int results_used;
	INIT_LIST_HEAD(&result->list);

	/* Map the user address into a scatter/gather table
	 * hardware will understand */
	results_used = map_user_buf_local(user_addr, size, &result->list,
				direction);
	if (results_used < 0) {
		/* Error - cleanup the pool_entry (as well as any
		 * entiries that were added in the map function before the
		 * error was encountered */
		pme_sg_table_entry_free(&result->list);
		return results_used;
	}
	pme_sg_complete(result, size, direction);
	return 0;
}

/* Creates a pme_data structure from a users iovec */
int pme_mem_map_vector(const struct iovec *user_vec,
			   int vector_size, struct pme_mem_mapping *result,
			   int direction)
{
	int i, ret;
	u32 total_size = 0;
	INIT_LIST_HEAD(&result->list);

	/* Map each user buf into a scatter/gather table entry */
	for (i = 0; i < vector_size; i++) {
		ret = map_user_buf_local((unsigned long)user_vec[i].iov_base,
			user_vec[i].iov_len, &result->list, direction);
		if (ret < 0) {
			/* Error occured */
			pme_sg_table_entry_free(&result->list);
			return ret;
		}
		total_size += user_vec[i].iov_len;
	}
	/* WARNING and reminder:
	 * It's possible to try and consolidate the SG list even
	 * more by walking the list and checking to see if any
	 * of the buffers are contigous.
	 * However, doing this will cause issues because at this
	 * point we have mapped the user pages more than once.
	 * This means that we need to release the user pages as
	 * many times as we locked them.
	 * It may be possible to do this, but the data is in a
	 * format that HW can understand anyway, so unless there
	 * is significant advantage to having smaller scatter/gather
	 * lists, the complexity of doing this most likely isn't
	 * worth the gain */
	pme_sg_complete(result, total_size, direction);
	return 0;
}

/* This function unmaps the pages mapping references */
void pme_mem_unmap(struct pme_mem_mapping *mapping)
{
	/* Unmap the scatter/gather mapping
	 * Do this first so the cache will be flushed/updated if needed */
	 pme_sg_table_entry_free(&mapping->list);
}

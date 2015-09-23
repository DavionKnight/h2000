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
 * Author: Roy Pledge, Roy.Pledge@freescale.com
 *
 * Description:
 * This file declares shared declarations, flags and types for the
 * user-interface components of the pattern-matcher driver.
 *
 */

#include "common.h"

/*********************/
/* init/finish hooks */
/*********************/
int pmscanner_init(void);
void pmscanner_finish(void);
int database_init(void);
void database_finish(void);

/**************************/
/* memory-mapping assists */
/**************************/

struct pme_mem_mapping {
	struct list_head list;
	struct pme_data data;
};

/* Populates a pme_mem_mapping structure from a user buffer
 * Returns 0 if success, negative value on error */
int pme_mem_map(unsigned long user_addr, size_t size,
		struct pme_mem_mapping *result, int direction);

/* Populates a pme_mem_mapping structure from a users iovec */
int pme_mem_map_vector(const struct iovec *user_vec, int vector_len,
			struct pme_mem_mapping *result, int direction);

void pme_mem_unmap(struct pme_mem_mapping *data_buf);

/* APIs for mapping fbchains into user space
 * Returns PME_MEM_SG if the result is an IOVEC
 * Returns PME_MEM_CONTIG if the result is a single buffer
 * Returns less than zero on error */
#define PME_MEM_CONTIG	0
#define PME_MEM_SG	1

int pme_mem_fb_map(struct file *filep,
		struct pme_fbchain *buffers,
		unsigned long *user_addr, size_t *size);

int pme_mem_fb_unmap(struct file *filep, unsigned long user_addr);

/* Useful utilities to copy from a fbchain to user space */
size_t pme_mem_fb_copy_to_user(void *user_data, size_t user_data_len,
		struct pme_fbchain *fbdata, size_t *offset);

size_t pme_mem_fb_copy_to_user_iovec(const struct iovec *user_data,
		size_t user_data_len, struct pme_fbchain *fbdata,
		size_t *offset);

/* All devices that use the above APIs to map/unmap
 * memory to user space will need to set the following API as
 * it's mmap handler in the modules fops handler */
int pme_mem_fops_mmap(struct file *filep, struct vm_area_struct *vma);

/* Scatter/Gather pool setup functions */
int pme_sg_tbl_pool_init(void);
void pme_sg_tbl_pool_destroy(void);

/* Release a chain of entries from the pool */
void pme_sg_table_entry_free(struct list_head *list);

/* Add a new entry to an sg structure */
int pme_sg_list_add(struct list_head *list, dma_addr_t address, size_t size);

/* Complete a sg result */
void pme_sg_complete(struct pme_mem_mapping *result, size_t total_size,
			int direction);

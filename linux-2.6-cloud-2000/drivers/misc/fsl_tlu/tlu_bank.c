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
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains an implementation of managing TLU bank memories.
 */
#include <asm/tlu_bank.h>
#include <asm/tlu_access.h>

static inline struct tlu_bank_mem_block *tlu_bank_mem_alloc_node(
		struct tlu_bank_mem *bank)
{
	struct tlu_bank_mem_block *node;

	node = bank->node_list;
	if (node != NULL)
		bank->node_list = node->next;

	return node;
}

static inline void tlu_bank_mem_free_node(struct tlu_bank_mem *bank,
		struct tlu_bank_mem_block *node)
{
	if (bank->node_list == NULL)
		node->next = NULL;
	else
		node->next = bank->node_list;
	bank->node_list = node;
}

void tlu_bank_mem_create(struct tlu_bank_mem *bank, int start, int size)
{
	int i;
	struct tlu_bank_mem_block *block;

	TLU_ACC_LOG("%s: bank = %d start = %x  size = %x\n",
			__func__, bank->index, start, size);

	bank->block_list = NULL;
	bank->node_list = NULL;
	for (i = 0; i < TLU_BANK_MAX_BLOCK_NUM; i++)
		tlu_bank_mem_free_node(bank, bank->block_buf + i);

	/* allocate a block */
	block = tlu_bank_mem_alloc_node(bank);
	block->addr = start;
	block->size = size;
	block->allocated = 0;
	block->next = NULL;
	bank->block_list = block;
}

int _tlu_bank_mem_config(struct tlu_bank_mem *bank, tlu_handle_t tlu,
		int bank_index, int par, int tgt, unsigned long base_addr,
		int size)
{
	int rc;
	int base, start;
	int trim_size;

	base = (base_addr >> TLU_BANK_BIT_BOUNDARY) << TLU_BANK_BIT_BOUNDARY;
	start = base_addr & (TLU_BANK_BOUNDARY - 1);

	if (start + size > TLU_BANK_BOUNDARY) {
		TLU_ACC_WARN("Bank memory accross 256M boundary. base = %x "
				"size = %x\n", base_addr, size);
		return TLU_INVALID_PARAM;
	}

	/* make it TLU_TABLE_BIT_BOUNDARY aligned */
	trim_size = (1 << TLU_TABLE_BIT_BOUNDARY)
		- (start & ((1 << TLU_TABLE_BIT_BOUNDARY) - 1));
	start += trim_size;
	size -= trim_size;
	if (size < 0) {
		TLU_ACC_WARN("Bank memory too small after aligned. base = %x "
				"size = %x\n", base_addr, size);
		return TLU_INVALID_PARAM;
	}

	tlu_bank_mem_create(bank, start, size);
	bank->index =  bank_index;
	rc = _tlu_bank_config(tlu, bank_index, par, tgt, base);
	if (rc < 0)
		return rc;

	return 0;
}

int _tlu_bank_mem_alloc(struct tlu_bank_mem *bank, int size)
{
	struct tlu_bank_mem_block *best_block, *block;

	TLU_ACC_LOG("%s: bank = %d  size = %x\n", __func__, bank->index, size);
	/* Finds The best block to allocate*/
	size = (size + TLU_BANK_BLOCK_ALIGN - 1)
		& (~(TLU_BANK_BLOCK_ALIGN - 1));
	best_block = NULL;
	block = bank->block_list;
	while (block) {
		if (block->allocated == 0 && block->size >= size) {
			if (best_block == NULL
					|| best_block->size > block->size) {
				best_block = block;
			}
		}
		block = block->next;
	}

	/* Not found */
	if (best_block == NULL)
		return -1;

	/* Assign the whole block if block size is too small or no more nodes */
	if (best_block->size - size >= TLU_BANK_MIN_BLOCK_SIZE) {
		block = tlu_bank_mem_alloc_node(bank);
		if (block != NULL) {
			/* Alloc a part of the block */
			block->size =  best_block->size - size;
			block->addr = best_block->addr + size;
			block->allocated = 0;
			best_block->size = size;
			block->next = best_block->next;
			best_block->next = block;
		}
	}
	best_block->allocated = 1;

	TLU_ACC_LOG("%s: bank = %d  addr = %x\n", __func__, bank->index,
			best_block->addr);
	return best_block->addr;
}

int _tlu_bank_mem_free(struct tlu_bank_mem *bank, int addr)
{
	struct tlu_bank_mem_block *prev_block, *block, *next;

	TLU_ACC_LOG("%s: bank = %d  addr = %x\n", __func__, bank->index, addr);

	prev_block = NULL;
	block = bank->block_list;
	while (block && addr > block->addr) {
		prev_block = block;
		block = block->next;
	}

	/* This should never happen */
	if (block->addr != addr || block->allocated == 0) {
		TLU_ACC_CRIT("Internal Error. Free addr = %x, block: "
				"allocated = %d addr = %x size = %x\n",
				addr, block->allocated, block->addr,
				block->size);
		return -1;
	}

	block->allocated = 0;

	/* Merge with the previous block */
	if (prev_block && prev_block->allocated == 0) {
		prev_block->size += block->size;
		prev_block->next = block->next;
		tlu_bank_mem_free_node(bank, block);
		block = prev_block;
	}
	/* Merge with the next block */
	next = block->next;
	if (next && next->allocated == 0) {
		block->size += next->size;
		block->next = next->next;
		tlu_bank_mem_free_node(bank, next);
	}

	/* Dump the block list for debug */
	block =  bank->block_list;
	while (block) {
		TLU_ACC_LOG("Allocated=%d  block=%p addr=%x  size=%d next=%p\n",
				block->allocated, block, block->addr,
				block->size, block->next);
		block = block->next;
	}
	return 0;
}

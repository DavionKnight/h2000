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
 * This file contains an implementation of TLU heap, a fixed size memory block
 * pool.
 */
#include <asm/tlu_heap.h>

tlu_heap_t tlu_heap_create(tlu_handle_t tlu, int table, int32_t start,
		int block_num, int block_size)
{
	int i;
	int32_t entry;

	entry = start;
	for (i = 1; i < block_num; i++) {
		if (tlu_heap_write(tlu, table, entry, entry + block_size)
				== TLU_HEAP_FAIL) {
			return TLU_HEAP_FAIL;
		}
		entry += block_size;
	}

	if (tlu_heap_write(tlu, table, entry, TLU_HEAP_END) == TLU_HEAP_FAIL)
		return TLU_HEAP_FAIL;

	return (tlu_heap_t)start;
}

tlu_heap_t tlu_heap_create_data(tlu_handle_t tlu, int table, int32_t start,
		int entry_num)
{
	int scale;

	scale = tlu_get_table_index_scale(tlu, table);
	return tlu_heap_create(tlu, table, start << scale, entry_num,
			1 << scale);
}

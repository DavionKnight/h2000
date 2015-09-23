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
 * This file contains utility funtions for iptd (or rundelta) tables.
 */
#include <asm/tlu_iptd.h>

/*******************************************************************************
 * Description:
 *   This function is to get total number of real entries that an entry given by
 *   <key> is compressed from.
 * Parameters:
 *   entropy - The entropy of the IPTD table.
 *   bits    - The bit size of the IPTD table
 *   key     - The key of the entry.
 * Return:
 *   The number of real entries the entry is compressed from.
 ******************************************************************************/
int _rundelta_get_entry_count(uint32_t entropy, int bits, int key)
{
	int count, code, block_size, bindex, eindex;

	if (entropy == 0)
		return 1;

	block_size = 1 << (bits - 4);
	count = 0;
	bindex = key >> (bits - 4);
	eindex = key & (block_size - 1);
	while (bindex < RUNDELTA_BLOCK_NUM) {
		code = entropy_get(entropy, 30 - (bindex) * 2);
		if (code == ENTROPY_NONE || code == ENTROPY_SINGLE) {
			bindex++;
			eindex = 0;
			count += block_size;
			if (code == ENTROPY_SINGLE)
				break;

		} else if (code == ENTROPY_HALF) {
			if (eindex == 0)
				count += block_size / 2 + 1;
			else
				count++;
			if (++eindex == block_size / 2) {
				eindex = 0;
				bindex++;
			}
			break;
		} else {
			/* ENTROPY_FULL */
			count++;
			if (++eindex == block_size) {
				eindex = 0;
				bindex++;
			}
			break;
		}
	}
	return count;
}

/*******************************************************************************
 * Description:
 *   This function is to get total number of real entries that the compressed
 *   entry at index <index> is compressed from.
 * Parameters:
 *   entropy - The entropy of the IPTD table.
 *   bits	  - The bit size of the IPTD table
 *   index   - Index in the compressed table.
 * Return:
 *   The number of real entries the entry is compressed from.
 ******************************************************************************/
int rundelta_get_entry_count(uint32_t entropy, int bits, int index)
{
	int i, entry_index, count;

	entry_index = 0;
	count = 0;  /* Avoid warning */
	for (i = 0; i <= index; i++) {
		count = _rundelta_get_entry_count(entropy, bits, entry_index);
		entry_index += count;
	}
	return count;
}

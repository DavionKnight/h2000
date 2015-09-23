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
 * This file contains an implementation of basic TLU functions.
 */
#include <asm/tlu_hw.h>

#define BYTE_MASK(bnum) (0xff << ((bnum)<<3))

/* TLU command names for debugging purpose */
char *tlu_cmd_str[] = {
	"ERR", "WRITE", "ERR", "ADD", "READ", "ERR", "ACCHASH", "ERR",
	"FIND", "ERR", "FINDR", "ERR", "FINDW", "ERR", "ERR", "ERR"
};

/*******************************************************************************
 * Description:
 *   This function calculates basic hash in the same way that TLU is doing.
 *   This is taken from PowerQUICC III Reference Manual.
 * Parameters:
 *   a  - The 1st 32 bit word
 *   b  - The 2nd 32 bit word
 *   j  - The shift coefficient.
 * Return:
 *   A 32 bit hash value
 ******************************************************************************/
static inline uint32_t tlu_hg_remix8(uint32_t a, uint32_t b, uint32_t j)
{
	static uint32_t shift_rights[9] =
	   /* j = 0 j = 0 j = 0 j = 3 j = 3 j = 3 j = 6 j = 6 j = 6 */
		{ 13, 8, 13, 12, 16, 5, 3, 10, 15 };
	static int shift_lefts[9]
		= { 32-13, 32-8, 32-13, 32-12, 32-16, 32-5, 32-3, 32-10, 32-15};
	uint32_t c;

	/* mix a and b, byte by byte */
	a = (((a & BYTE_MASK(0)) - (b & BYTE_MASK(0))) & BYTE_MASK(0)) |
	(((a & BYTE_MASK(1)) - (b & BYTE_MASK(1))) & BYTE_MASK(1)) |
	(((a & BYTE_MASK(2)) + (b & BYTE_MASK(2))) & BYTE_MASK(2)) |
	(((a & BYTE_MASK(3)) + (b & BYTE_MASK(3))) & BYTE_MASK(3));
	/* XOR a by b rotated right random number of bits */
	a ^= (b>>shift_rights[j]) | (b<<shift_lefts[j]);
	/* mix b and new a, byte by byte */
	b = (((b & BYTE_MASK(0)) + (a & BYTE_MASK(0))) & BYTE_MASK(0)) |
	(((b & BYTE_MASK(1)) + (a & BYTE_MASK(1))) & BYTE_MASK(1)) |
	(((b & BYTE_MASK(2)) - (a & BYTE_MASK(2))) & BYTE_MASK(2)) |
	(((b & BYTE_MASK(3)) - (a & BYTE_MASK(3))) & BYTE_MASK(3));
	/* XOR b by a rotated right random number of bits */
	b ^= (a>>shift_rights[j + 1]) | (a<<shift_lefts[j + 1]);
	/* mix a and b again, byte by byte */
	c = (((a & BYTE_MASK(0)) - (b & BYTE_MASK(0))) & BYTE_MASK(0)) |
	(((a & BYTE_MASK(1)) - (b & BYTE_MASK(1))) & BYTE_MASK(1)) |
	(((a & BYTE_MASK(2)) + (b & BYTE_MASK(2))) & BYTE_MASK(2)) |
	(((a & BYTE_MASK(3)) + (b & BYTE_MASK(3))) & BYTE_MASK(3));
	/* XOR c by b rotated right random number of bits */
	c ^= (b>>shift_rights[j + 2]) | (b<<shift_lefts[j + 2]);
	/* return a mixed with b three times */
	return c;
}

uint32_t tlu_hash_cont32(void *key, int len, uint32_t c)
{
	uint32_t a, b;
	int i;

	for (i = 0; i < len - 1; i += 2) {
		a = tlu_hg_remix8(((uint32_t *)key)[i],
				((uint32_t *)key)[i + 1], 0);
		b = tlu_hg_remix8(a, c, 3);
		c = tlu_hg_remix8(c, b, 0);
	}

	if (i < len) {
		a = tlu_hg_remix8(((uint32_t *)key)[i], 0, 0);
		b = tlu_hg_remix8(a, c, 3);
		c = tlu_hg_remix8(c, b, 0);
	}

	return c;
}

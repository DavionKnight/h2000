/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains definitions, macros and inline implementations for the
 * IPTD Table of TLU.
 * Rundelta is a commpressed IPTD tabel.
 *
 * This file is part of the Linux kernel
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef TLU_IPTD_H
#define TLU_IPTD_H

#include "tlu_access.h"

/* IPTD Entry Types */
#define IPTD_ETYPE_FAIL 	0
#define IPTD_ETYPE_DATA 	1
#define IPTD_ETYPE_SIMPLE 	2
#define IPTD_ETYPE_HASH 	3
#define IPTD_ETYPE_RUNDELTA 	4

/* IPTD Fields */
#define IPTD_ETYPE_SHIFT 	(63-63)
#define IPTD_ETYPE_MASK 	0x0F
#define IPTD_KEYSHL_SHIFT 	(63-59)
#define IPTD_KEYSHL_MASK 	0x0F
#define IPTD_BASE_SHIFT 	(63-55)
#define IPTD_BASE_MASK 		0xFFFFFF

/* bits: IPTD table size in bits */
#define RUNDELTA_FULL_BLOCK_SIZE(bits) (1 << ((bits) - 4))
#define RUNDELTA_HALF_BLOCK_SIZE(bits) RUNDELTA_FULL_BLOCK_SIZE((bits) - 1)
#define RUNDELTA_BLOCK_NUM 	16

/* Entropy Codes */
#define ENTROPY_NONE 	0
#define ENTROPY_SINGLE 	1
#define ENTROPY_HALF 	2
#define ENTROPY_FULL 	3

/* TLU hardware requires minium 5 bits of RUNDELTA table. */
#define RUNDELTA_MIN_BITS 	5

typedef struct tlu_iptd_entry iptd_entry_t;

int _rundelta_get_entry_count(uint32_t entropy, int bits, int key);
int rundelta_get_entry_count(uint32_t entropy, int bits, int index);

/* Functions to access fields of IPTD entry */
/*---------------Functions to access fields of an IPTD entry -----------------*/
static inline int iptd_get_etype(iptd_entry_t *entry)
{
	return (entry->words[1] >> IPTD_ETYPE_SHIFT) & IPTD_ETYPE_MASK;
}

static inline int iptd_get_entropy(iptd_entry_t *entry)
{
	return entry->entropy;
}

static inline int iptd_get_base(iptd_entry_t *entry)
{
	return (entry->words[1] >> IPTD_BASE_SHIFT) & IPTD_BASE_MASK;
}

static inline int iptd_get_keyshl(iptd_entry_t *entry)
{
	return (entry->words[1] >> IPTD_KEYSHL_SHIFT) & IPTD_KEYSHL_MASK;
}

static inline int iptd_get_bits(iptd_entry_t *entry)
{
	return iptd_get_keyshl(entry) + 1;
}

static inline void iptd_fill_entry(iptd_entry_t *entry,  uint32_t entropy,
		int base, int keyshl, int etype)
{
	entry->words[0] = entropy;
	entry->words[1] = (base << IPTD_BASE_SHIFT)
		| (keyshl << IPTD_KEYSHL_SHIFT)
		| (etype << IPTD_ETYPE_SHIFT);
}

/*---------------Functions to manipulate entropy -----------------------------*/
/* Note: Offset counts 0 from the least significant bit */
static inline int entropy_get(int entropy, int offset)
{
	return (entropy >> offset) & 3;
}

static inline uint32_t entropy_set(int entropy, int offset, int code)
{
	return (entropy & (~(3 << offset))) | (code << offset);
}

/*-------------------------- Rundelta functions ------------------------------*/
/******************************************************************************
 * Description:
 *   Calculate block size from the code.
 * Parameters:
 *   code - entropy code
 *   bits - table size in bits
 * Return:
 *   The total entry number in the block.
 *****************************************************************************/
static inline int rundelta_block_size(int code, int bits)
{
	if ((code & 2) ==  0)
		return code;
	return 1 << (bits - 4 - ((code & 1) ^ 1));
}

/******************************************************************************
 * Description:
 *   Calculate the size of the top part of a run delta table. The table must be
 *   compressed.
 * Parameters:
 *   entropy   - entropy fo the table
 *   block_num - Total block number to be counted from teh 1st block of the
 *               table.
 *   bits      - table size in bits
 * Return:
 *   The total entry number in the blocks.
 *****************************************************************************/
static inline int _rundelta_get_size(uint32_t entropy, int block_num, int bits)
{
	int i, size;

	size = 0;
	for (i = 0; i < block_num; i++) {
		size += rundelta_block_size(entropy_get(entropy, 30 - i*2),
				bits);
	}
	return size;
}

/******************************************************************************
 * Description:
 *   Calculate the size of a rundelta table. The table can be not compressed.
 * Parameters:
 *   entropy  - entropy fo the table
 *   bits     - table size in bits
 * Return:
 *   The total entry number in the table.
 *****************************************************************************/
static inline int rundelta_get_size(uint32_t entropy, int bits)
{
	if (entropy == 0) {
		/* It is an uncompressed table */
		return 1 << bits;
	}
	return _rundelta_get_size(entropy, 16, bits);
}

/******************************************************************************
 * Description:
 *   Get the index of an entry associated with key value 'key' in a rundelta
 *   table.
 * Parameters:
 *   entropy	 - entropy fo the table
 *   block_index - Index of the block that the entry is belonging to.
 *   bits	 - table size in bits
 *   key	 - The key of the entry.
 * Return:
 *   The index of the entry
 *****************************************************************************/
static inline int rundelta_get_index(uint32_t entropy, int block_index,
		int bits, int key)
{
	int index, code;

	code = entropy_get(entropy, 30 - block_index*2);

	if ((code & 2) == 0)
		index = code - 1;
	else if (code == 2) {
		if (key & (1 << (bits - 5)))
			index = key & (RUNDELTA_HALF_BLOCK_SIZE(bits) - 1);
		else
			index = -1;
	} else
		index = key & (RUNDELTA_FULL_BLOCK_SIZE(bits) - 1);
	return index;
}

#endif

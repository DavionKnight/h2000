/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains definitions, macros and inline functions for Compressed
 * Radix Trie (CRT) table lookup and management.
 *
 * This file is part of the Linux kernel
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef CRT_H
#define CRT_H

#include <asm/tlu_access.h>
#include <asm/tlu_heap.h>
#include <asm/tlu_iptd.h>
#include <asm/tlu_bank.h>

/* Keys will be stored in local memory if it is defined */
#define CRT_STORE_KEY

/* CRT_STORE_KEY must be enabled to enable CRT_REDUNDANT */
#ifdef CRT_STORE_KEY
/* Allow redundant entries in a table if it is defined */
#define CRT_REDUNDANT
#endif

/* A data structure to keep information is a CRT data entry */
struct crt_data_info {
	int ref_count;	  /* Make it int so we can validate it when it
			   * accidently becomes negative
			   */
#ifdef CRT_STORE_KEY
	int key_size;	   /* Key size in bits. Data is invalid if it is -1 */
	uint8_t key[TLU_MAX_KEY_BYTES];
#endif
};

/* CRT data structure used by this software */
struct crt {
	tlu_handle_t tlu;
	int table;
	int key_bits;
	int empty;
	int kdata_size;		 /* Data entry size in bytes */
	struct tlu_bank_mem *bank;
	int compress;
	int max_iptd_bits;
	int min_iptd_bits;
	int max_iptd_levels;
	uint32_t free_start;
	uint32_t free_size;
	long table_start;
	long data_start;		/* Data index base */
	tlu_heap_t data_heap;
	struct crt_data_info *data_info;
	int data_entry_num;
	tlu_heap_t mem_heap;
	int free_data_count;
	int free_mem_size;
	iptd_entry_t *tmp_iptd_buf;
};

/* The data structure defines a maximum data entry in a CRT table */
struct crt_entry_max {
	uint32_t key;
	uint32_t bits;
	uint8_t data[TLU_MAX_KDATA_BYTES];
};

/* The maximum IPTD table size in bits. The TLU hardware limit is 24 bits which
 * is limited by the maximum table entry number, 16M. It seems 8 bit is a more
 * practicable number. There are a number of issues need to be considered before
 * change it to a larger number.  1. IPTD pool is a fixed size heap. A larger
 * bit number means larger block sizes and memory usage will not be efficient
 * any more.  2. CRT insertion and deletion may take long time because updating
 * larger IPTD tables takes more time.  3. CRT insertion and deletion require
 * application pre-allocated memories. A larger CRT_MAX_IPTD_BITS means
 * requiring more memories and maybe a different allocation mechanism.
 */
#define CRT_MAX_IPTD_BITS 8
/* The minimum IPTD size the this software will normally use */
#define CRT_MIN_IPTD_BITS 4

/* Hardware limit is 255. Since the CRT implementation uses recursive algorithm
 * The more IPTD level, the more stack memory are used. To be safe in some
 * operating system, e.g. Linux, the maximum level is limited to 16.
 */
#define CRT_MAX_IPTD_LEVELS 16

#define CRT_MIN_COMPRESSED_IPTD_SIZE 1

#define CRT_MEM_BLOCK_SIZE ((1 << CRT_MAX_IPTD_BITS) * TLU_UNIT_SIZE)

#define TLU_LOG_CRT	 0x000000040
#define CRT_LOG(format, ...) TLU_LOG(TLU_LOG_CRT, "CRT", format, ##__VA_ARGS__)
#define CRT_LOG_MEM(buf, len, format, ...) \
	TLU_LOG_MEM(TLU_LOG_CRT, buf, len, "CRT", format, ##__VA_ARGS__)
#define CRT_CRIT(format, ...) \
	TLU_LOG(TLU_LOG_CRIT, "CRT", format, ##__VA_ARGS__)
#define CRT_WARN(format, ...) \
	TLU_LOG(TLU_LOG_WARN, "CRT", format, ##__VA_ARGS__)

/*******************************************************************************
 * Description:
 * This function is to create a CRT table.
 * Parameters:
 *   crt      - An empty struct crt data structure to store information of the
 *   	        created table.
 *   tlu      - Indicating the tlu on which the CRT table will be created.
 *   table    - Indicating the table on wich the CRT table will be created.
 *   size     - Total number of bytes the table will use.
 *   key_bits - The size of the key in bits. It must be 32, 64, 96 and 128.
 *   kdata_size - The size of the data entry in bytes. It must be 8, 16, 32
 *                or 64.
 *   kdata_entry_num - The maximum number of data entries in this CRT table.
 *   bank     - The memory bank on which this CRT table will be created.
 *   compress - Indicating that this is a compressed table if it is 1.
 *   buf      - A pre-allocated memory buffer which is required by this CRT
 *              table. The size of this buffer can be obtained by calling
 *               the function crt_required_mem_size.
 * Return:
 *   0    The table is careated successfully.
 *   < 0  Error code if failure
*******************************************************************************/
int crt_create(struct crt *crt, tlu_handle_t tlu, int table, int size,
		int key_bits, int kdata_size, int kdata_entry_num,
		struct tlu_bank_mem *bank, int compress, void *buf);

/*******************************************************************************
 * Description:
 *   This function is to free a previously created CRT table.
 * Parameters:
 *   crt    - Indicating the CRT table.
 * Return:
 *   = 0   The table has been successfully freed.
 *   = -1  Failed to free the table. It is an internal error in this version of
 *	   software.
 ******************************************************************************/
int crt_free(struct crt *crt);

/*******************************************************************************
 * Description:
 *   This function is to insert an entry into the CRT table.
 * Parameters:
 *   crt   - Indicating the CRT table.
 *   key   - The key of an entry to be inserted. A key should take the most
 *	     significant bits of the word if it is less than 32 bits.
 *   key_bits - Total number of bits of key.
 *   data     - Pointing to the data of the inserting entry.
 * Return:
 *   0    The entry has been successfully inserted.
 *  < 0   Error code if failure
 ******************************************************************************/
int crt_insert(struct crt *crt, void *key, int bits, void *data);

/*******************************************************************************
 * Description:
 *   This function is to delete an entry from the CRT table.
 * Parameters:
 *   crt      - Indicating the CRT table.
 *   key      - The key of an entry to be deleted. A key should take the most
 *	        significant bits if it is less than 32 bits.
 *   key_bits - Total number of bits of key to be deleted.
 * Return:
 *   = 0  The entry is successfully deleted.
 *   <0   Error code if failure
 ******************************************************************************/
int crt_delete(struct crt *crt, void *key, int key_bits);

/*******************************************************************************
 * Description:
 *   This function is to obtain the size of memory that a CRT table is required.
 *   The return value is usually to be fed into crt_create.
 * Parameters:
 *   data_entry_num - Total data entry number in the CRT table
 * Return:
 *   Required memory size in bytes.
 ******************************************************************************/
uint32_t crt_required_mem_size(int data_entry_num);

/*******************************************************************************
 * Description:
 *   This function search a key in the specified TLU table.
 * Parameters:
 *   crt	- The CRT table
 *   key	- A pointer to the key to be searched
 * Return:
 *   >= 0  The index of found key
 *   < 0   Not found or error.
 *	   TLU_NOT_FOUND  The key is not found.
 *	   TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
static inline int crt_find(struct crt *crt, void *key)
{
	int index;

	index = _tlu_find(crt->tlu, crt->table, key, crt->key_bits >> 3);
	if (index >= 0)
		return index - crt->data_start;
	return index;
}

/*******************************************************************************
 * Description:
 *   This function search a key in the specified TLU table and return data if
 *   found.
 * Parameters:
 *   crt    - The CRT table
 *   keyi   - A pointer to the key to be searched
 *   offset - A byte offset within found data entry. It must be multiple of 8.
 *   len    - Total number of bytes to be read. It must be a multiple of 8.
 *   data   - A buffer to return read data if it is not NULL. By giving a NULL,
 *            a caller can get data from TLU's data register at address
 *            TLU_KDATA_PTR(tlu).
 * Return:
 *   >= 0  The index of found key
 *   < 0   Not found or error.
 *   	   TLU_NOT_FOUND  The key is not found.
 *	   TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
static inline int crt_findr(struct crt *crt, void *key, int offset, int len,
		void *data)
{
	int index;

	index = _tlu_findr(crt->tlu, crt->table, key, crt->key_bits >> 3,
			offset, len, data);
	if (index >= 0)
		return index - crt->data_start;
	return index;
}

/*******************************************************************************
 * Description:
 *   This function search a key, then write 64 bits of data into the entry if
 *   found.
 * Parameters:
 *   crt    - The CRT table
 *   key    - A pointer to the key to be searched
 *   offset - A byte offset within found data entry where the writing
 *	      will be. It must be multiple of 8.
 *   data   - The data which will replace the found data at offset <offset>
 * Return:
 *   >= 0  The index of found key
 *   < 0   Not found or error.
 *   	   TLU_NOT_FOUND  The key is not found.
 *	   TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
static inline int crt_findw(struct crt *crt, void *key, int offset,
		uint64_t data)
{
	int index;

	index = _tlu_findw(crt->tlu, crt->table, key, crt->key_bits >> 3,
			offset, data);
	if (index >= 0)
		return index - crt->data_start;
	return index;
}


#endif

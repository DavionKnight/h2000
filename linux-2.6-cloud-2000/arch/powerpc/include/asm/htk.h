/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains definitions, macros and inline functiona for
 * Hash-Trie_Key (HTK) table lookup and management.
 *
 * This file is part of the Linux kernel
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef HTK_H
#define HTK_H

#include <asm/tlu_access.h>
#include <asm/tlu_heap.h>
#include <asm/tlu_log.h>
#include <asm/tlu_bank.h>

/* Hardware limit is 255. 250 is a safe number. */
#define HTK_MAX_TRIE_LEVELS 250

/* HTK Log */
#define TLU_LOG_HTK	 0x000000020
#define HTK_LOG(format, ...) TLU_LOG(TLU_LOG_HTK, "HTK", format, ##__VA_ARGS__)
#define HTK_LOG_MEM(buf, len, format, ...) \
	TLU_LOG_MEM(TLU_LOG_HTK, buf, len, "HTK", format, ##__VA_ARGS__)
#define HTK_CRIT(format, ...) \
	TLU_LOG(TLU_LOG_CRIT, "HTK", format, ##__VA_ARGS__)
#define HTK_WARN(format, ...) \
	TLU_LOG(TLU_LOG_WARN, "HTK", format, ##__VA_ARGS__)

/* An HTK data structure used by this software */
struct htk {
	tlu_handle_t tlu;
	int table;
	int hash_bits;
	int key_bits;
	int kdata_size;
	int data_start;	 /* The base index of data table */
	struct tlu_bank_mem *bank;
	long table_start;
	tlu_heap_t trie_heap;
	tlu_heap_t data_heap;
};

/*******************************************************************************
 * Description:
 *   This function creates a HTK table in the specified TLU and table.
 * Parameters:
 *   htk  - A pointer to an un-initialized struct htk data structure. Its
 * 	    fields will be initialized when the function is returned. It will
 *	    be used to in further calls of any HTK functions.
 *   tlu  - A handle of TLU where the HTK table is created.
 *   table      - An index of a physical table on which the HTK table is
 *                created. A valid value must be between 0 to 31 (inclusive).
 *   size       - The total table memory size in bytes.
 *   key_bits   - The size of key in bits. It must be 32, 64, 96 or 128.
 *   kdata_size - The key-data entry size in bytes. It must be 8, 16, 32 or 64
 *	          bytes.
 *   kdata_entry_num - The maximum key-data entry number
 *   hash_bits  - The hash bits. A valid value is between 1 to 16 inclusive.
 *   bank	- Pointing to a TLU bank memory data structure. The data
 *                structure must be initialized by calling _tlu_bank_mem_config.
 * Return:
 *   = 0   The table is successfully created.
 *   < 0   An error code denotes the operation failed.
 ******************************************************************************/
int htk_create(struct htk *htk, tlu_handle_t tlu, int table, int size,
		int key_bits, int kdata_size, int kdata_entry_num,
		int hash_bits, struct tlu_bank_mem *bank);

/*******************************************************************************
 * Description:
 *   This function frees a htk table.
 * Parameters:
 *   htk   - Pointing to a previously created HTK table.
 * Return:
 *  = 0  The table is successfully freed.
 *  -1	 Failed to free the table. It indicates a software internal error in
 *       this version of software.
 ******************************************************************************/
int htk_free(struct htk *htk);

/*******************************************************************************
 * Description:
 *   This function inserts a key-data entry into the HTK table.
 * Parameters:
 *   htk    - A previousely created HTK table.
 *   entry  - The key-data entry to be inserted. The key must be at offset 0 of
 *	      the entry.
 * Return:
 *  > 0	 The entry is successfully inserted. The index (from 0) of the entry is
 *	 returned.
 *  < 0  An error code denotes the operation failed.
 ******************************************************************************/
int htk_insert(struct htk *htk, void *entry);

/*******************************************************************************
 * Description:
 *   This function deletes a key-data entry from the HTK table.
 * Parameters:
 *   htk  - A previousely created HTK table.
 *   key  - The key of the entry to be delted.
 * Return:
 *   = 0   The entry is successfully deleted.
 *   < 0   An error code denotes the operation failed. Below are possible
 *         errors:
 *         TLU_MEM_ERROR - Memory error. It is an unrecoverable error
 *	   TLU_CHAIN_TOO_LONG - The HTK table has more chained levels than
 *    	   expected.
 *         TLU_ENTRY_NOT_EXIST - The table entry does not exist.
 ******************************************************************************/
int htk_delete(struct htk *htk, void *key);

/*******************************************************************************
 * Description:
 *   This function search a key in the specified TLU table.
 * Parameters:
 *   htk   - The HTK table
 *   key   - A pointer to the key to be searched
 * Return:
 *  >= 0  The index (from 0) of the found key.
 *  < 0   Not found or error.
 *	  TLU_NOT_FOUND  The key is not found.
 *	  TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
static inline int htk_find(struct htk *htk, void *key)
{
	int index;

	index = _tlu_find(htk->tlu, htk->table, key,
			htk->key_bits >> 3);
	if (index >= 0)
		return index - htk->data_start;
	return index;
}

/*******************************************************************************
 * Description:
 *   This function search a key in the specified TLU table and return data if
 *   found.
 * Parameters:
 *   htk    - The HTK table
 *   key    - A pointer to the key to be searched
 *   offset - A byte offset within found data entry
 *   len    - Total number of bytes to be read. It must be a multiple of 8 and
 *            less than or equal to 32.
 *   data   - A buffer to return read data if it is not NULL. By giving a NULL,
 *            a caller can get data from TLU's data register at address
 *	      TLU_KDATA_PTR(tlu).
 * Return:
 *   >= 0  The index of found key
 *   < 0   Not found or error.
 *         TLU_NOT_FOUND  The key is not found.
 *         TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
static inline int htk_findr(struct htk *htk, void *key, int offset, int len,
	void *data)
{
	int index;

	index = _tlu_findr(htk->tlu, htk->table, key, htk->key_bits >> 3,
			offset, len, data);
	if (index >= 0)
		return index - htk->data_start;
	return index;
}

/*******************************************************************************
 * Description:
 *   This function search a key, then write 64 bits of data into the entry if
 *   found.
 * Parameters:
 *   htk    - The HTK table
 *   key    - A pointer to the key to be searched
 *   offset - A byte offset within found data entry where the writing
 * 	      will be
 *   data   - The data which will replace the found data at offset <offset>
 * Return:
 *  >= 0  The index of found key
 *  < 0   Not found or error.
 *        TLU_NOT_FOUND  The key is not found
 *        TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
static inline int htk_findw(struct htk *htk, void *key, int offset,
		uint64_t data)
{
	int index;

	index = _tlu_findw(htk->tlu, htk->table, key, htk->key_bits >> 3,
			offset, data);
	if (index >= 0)
		return index - htk->data_start;
	return index;
}

#endif

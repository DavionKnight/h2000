/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains definitions, macros and inline implementations of TLU
 * heap which is a memory heap with fixed size blocks. Also heap functions
 * which dealing with TLU's data entries are implemented.
 *
 * Heaps are directly built in TLU memory. They are linked one after another
 * with block index.
 *
 * This file is part of the Linux kernel
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef TLU_HEAP_H
#define TLU_HEAP_H

#include <asm/tlu_osdef.h>
#include <asm/tlu_access.h>

/* The link size is 4 bytes. However, the minimum TLU access unit which is
 * 8 bytes is used as entry size. The 2nd 4 byte is not used
 */
#define TLU_HEAP_ENTRY_SIZE 8

/* TLU Heap return Codes */
/* End of Heap */
#define TLU_HEAP_END -1
/* Heap operation failed due to a memory error */
#define TLU_HEAP_FAIL -2

typedef int32_t tlu_heap_t;

/*******************************************************************************
 * Description:
 *   This function creates a TLU heap.
 * Parameters:
 *   tlu        - TLU handle
 *   table      - An index of the table where the heap is to be created
 *   start      - The start block index relative to the table base.
 *   block_num  - Total number of blocks. It must be greater or equal than 1.
 *   block_size - Block size which is total number of TLU unit each block has.
 *                It is the number of bytes of a block divided by TLU_UNIT_SIZE.
 * Return:
 *   >=0  The heap handle
 *   < 0  Error code which is the following:
 *        TLU_HEAP_FAIL  TLU memory error detected
 ******************************************************************************/
tlu_heap_t tlu_heap_create(tlu_handle_t tlu, int table, int32_t start,
		int block_num, int block_size);
/*******************************************************************************
 * Description:
 *   This function creates a TLU heap. 'start' is scaled by table's data entry
 *   size ('size' field of the table configuration register).
 * Parameters:
 *   tlu        - TLU handle
 *   table      - An index of the table where the heap is to be created
 *   start      - The start entry index relative to the table base.
 *   entry_num  - Total number of data entries. It must be greater or equal
 *                than 1.
 * Return:
 *   The heap handle
 ******************************************************************************/
tlu_heap_t tlu_heap_create_data(tlu_handle_t tlu, int table, int32_t start,
		int entry_num);

/*******************************************************************************
 * Description:
 *   This function reads the next link of the given block.
 * Parameters:
 *   tlu    - TLU handle
 *   table  - The index of the table where the heap is
 *   index  - The index of the block to be read
 * Return:
 *   >= 0  The next link of the given block
 *   < 0   Error code
 *	   TLU_HEAP_FAIL  Indicating memory error detected
 ******************************************************************************/
static inline int32_t tlu_heap_read(tlu_handle_t tlu, int table, int32_t index)
{
	uint32_t *next;

	next = (uint32_t *)_tlu_read(tlu, table, index,
					TLU_HEAP_ENTRY_SIZE, NULL);
	if (next == NULL)
		return TLU_HEAP_FAIL;
	return *next;
}

/*******************************************************************************
 * Description:
 *   This function write a next link into the given block.
 * Parameters:
 *   tlu    - TLU handle
 *   table  - The index of the table where the heap is
 *   index  - The index of the block to be written
 *   next   - The next link value
 * Return:
 *   >= 0  The given next link
 *   < 0   Error code
 *	   TLU_HEAP_FAIL  Indicating memory error detected
 ******************************************************************************/
static inline int32_t tlu_heap_write(tlu_handle_t tlu, int table,
		int32_t index, int32_t next)
{
	uint8_t tmp[TLU_HEAP_ENTRY_SIZE];

	*(int32_t *)tmp = next;
	if (_tlu_write(tlu, table, index, TLU_HEAP_ENTRY_SIZE, &tmp) < 0)
		return TLU_HEAP_FAIL;
	return next;
}

/*******************************************************************************
 * Description:
 *   This function allocates a block from the given heap.
 * Parameters:
 *   tlu      - TLU handle
 *   table    - An index of the table where the heap is
 *   tlu_heap - TLU heap handle
 * Return:
 *   >=0  The allocated block index
 *   <0   Error code listed below:
 *        TLU_HEAP_END  The heap is empty
 *        TLU_HEAP_FAIL Memory error detected
 ******************************************************************************/
static inline int32_t tlu_heap_alloc(tlu_handle_t tlu, int table,
		tlu_heap_t *tlu_heap)
{
	int32_t index;

	index = *tlu_heap;
	if (index != TLU_HEAP_END) {
		*tlu_heap = tlu_heap_read(tlu, table, index);
		if (*tlu_heap == TLU_HEAP_FAIL)
			return *tlu_heap;
	}
	return index;
}

/*******************************************************************************
 * Description:
 *   This function frees a block back into the given heap.
 * Parameters:
 *   tlu      - TLU handle
 *   table    - An index of the table where the heap is
 *   tlu_heap - TLU heap handle
 *   index    - The index of a block which is to be freed.
 * Return:
 *   >=0  The freed block index
 *   <0   Error code listed below:
 *        TLU_HEAP_FAIL Memory error detected
*******************************************************************************/
static inline int32_t tlu_heap_free(tlu_handle_t tlu, int table,
		tlu_heap_t *tlu_heap, int32_t index)
{
	if (tlu_heap_write(tlu, table, index, *tlu_heap) == TLU_HEAP_FAIL)
		return TLU_HEAP_FAIL;
	return *tlu_heap = index;
}

/*******************************************************************************
 * Description:
 *   This function peeks a heap and return the block index which is on the top
 *   of the heap.
 * Parameters:
 *   tlu_heap   - TLU heap handle
 * Return:
 *   >=0  The block index on the top of the heap
 *   < 0  The only possible value is TLU_HEAP_END which indicates the heap is
 *        empty
 ******************************************************************************/
static inline int32_t tlu_heap_peek(const tlu_heap_t *tlu_heap)
{
	return *tlu_heap;
}

/*******************************************************************************
 * Description:
 *   This function allocates a data entry from the given heap. The heap must be
 *   created by tlu_heap_create_data.
 * Parameters:
 *   tlu       - TLU handle
 *   table     - An index of the table where the heap is
 *   tlu_heap  - TLU heap handle
 * Return:
 *   >=0  The allocated data entry index
 *   <0   Error code listed below:
 *	  TLU_HEAP_END  The heap is empty
 *	  TLU_HEAP_FAIL Memory error detected
 ******************************************************************************/
static inline int32_t tlu_heap_alloc_data(tlu_handle_t tlu, int table,
		tlu_heap_t *tlu_heap)
{
	return tlu_heap_alloc(tlu, table, tlu_heap)
		>> tlu_get_table_index_scale(tlu, table);
}

/*******************************************************************************
 * Description:
 *   This function frees a data entry back into the given heap. The heap must be
 *   created by tlu_heap_create_data.
 * Parameters:
 *   tlu      - TLU handle
 *   table    - An index of the table where the heap is
 *   tlu_heap - TLU heap handle
 *   index    - The index of a data entry which is to be freed.
 * Return:
 *   >=0  The freed data entry index
 *   <0   Error code listed below:
 *	  TLU_HEAP_FAIL Memory error detected
 ******************************************************************************/
static inline int32_t tlu_heap_free_data(tlu_handle_t tlu, int table,
		tlu_heap_t *tlu_heap, int32_t index)
{
	return tlu_heap_free(tlu, table, tlu_heap, index
			<< tlu_get_table_index_scale(tlu, table));
}

#endif

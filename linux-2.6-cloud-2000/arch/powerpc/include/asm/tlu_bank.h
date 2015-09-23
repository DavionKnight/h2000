/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains definitions, macros and inline implementations of TLU
 * bank memory management.
 *
 * This file is part of the Linux kernel
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef TLU_BANK_H
#define TLU_BANK_H

#include <asm/tlu_osdef.h>
#include <asm/tlu_access.h>

#define TLU_BANK_BLOCK_ALIGN	4096
#define TLU_BANK_MIN_BLOCK_SIZE 4096
#define TLU_BANK_MAX_BLOCK_NUM  128

/* Data structure of a bank memory block */
struct tlu_bank_mem_block{
	struct tlu_bank_mem_block *next;
	int addr;
	int size;
	int allocated;
};

/* Bank memory management data structure */
struct tlu_bank_mem {
	struct tlu_bank_mem_block *block_list;
	struct tlu_bank_mem_block *node_list;
	int index;
	struct tlu_bank_mem_block block_buf[TLU_BANK_MAX_BLOCK_NUM];
};

/*******************************************************************************
 * Description:
 *   This function is to configure a bank memory.
 * Parameters:
 *   bank       - A pointer to struct tlu_bank_mem which will be initialized
 *                based on configuration parameters.
 *   tlu        - The handle of the TLU that will access this bank.
 *   bank_index - The index of the bank. It must be 0, 1, 2 or 3.
 *   par        - Indicates parity enable or disable for this bank memory.
 *                It is either TLU_MEM_PARITY_ENABLE or TLU_MEM_PARITY_DISABLE
 *   tgt	- Indicates memory type. It is either TLU_MEM_LOCAL_BUS or
 *	          TLU_MEM_SYSTEM_DDR
 *  base_addr   - The physical base address of the memory bank.
 *  size        - Total number of bytes of the bank memory. The memory must not
 *                across 256M address boundary.
 * Return:
 *   = 0  Success
 *   < 0  Error code
 ******************************************************************************/
int _tlu_bank_mem_config(struct tlu_bank_mem *bank, tlu_handle_t tlu,
		int bank_index, int par, int tgt, unsigned long base_addr,
		int size);

/*******************************************************************************
 * Description:
 *   This function is to allocate a physically contiguous memory block.
 * Parameters:
 *   bank  - A pointer to struct tlu_bank_mem which will is initialized by
 * 	     _tlu_bank_mem_config
 *   size  - Total number of reuqested bytes.
 * Return:
 *   The physical address (with most signnificant 4 bits being 0) of the
 * allocated memory is returned if success. Otherwise -1 is returned if fail.
 ******************************************************************************/
int _tlu_bank_mem_alloc(struct tlu_bank_mem *bank, int size);

/*******************************************************************************
 * Description:
 *   This function is to free a memory block allocated by _tlu_bank_mem_alloc.
 * Parameters:
 *   bank  - A pointer to struct tlu_bank_mem which will is initialized by
 *           _tlu_bank_mem_config
 *   addr  - An address returned by _tlu_bank_mem_alloc.
 * Return:
 *   0 is returned if success. Otherwise -1 is returned if fail.
*******************************************************************************/
int _tlu_bank_mem_free(struct tlu_bank_mem *bank, int addr);

#endif

/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains definitions and macros for TLU Linux APIs.
 *
 * This file is part of the Linux kernel
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef TLU_H
#define TLU_H

#include <asm/tlu_access.h>
#include <asm/tlu_bank.h>
#include <asm/crt.h>
#include <asm/htk.h>
#include <linux/version.h>

/* Each TLU will be protected by a mutex
 */
#define TLU_USE_LOCK

#ifdef TLU_USE_LOCK
 #include <linux/mutex.h>
 #define TLU_LOCK_INIT(lock)	mutex_init(lock)
 #define TLU_LOCK(lock)		mutex_lock(lock)
 #define TLU_UNLOCK(lock)	mutex_unlock(lock)
#else	/* ifdef TLU_USE_LOCK */
 #define TLU_LOCK_INIT(lock)
 #define TLU_LOCK(lock)
 #define TLU_UNLOCK(lock)
#endif	/* ifdef TLU_USE_LOCK */

#define TLU_TABLE_ALLOC_FAIL	-100
#define TLU_TABLE_ASSIGNED	-101

struct tlu_bank_param {
	unsigned long addr;	/* physical memory address of the bank */
	uint32_t size;		/* Size of the memory in bytes */
	int type;
	int parity;
};

/* Bank memory configuration */
struct tlu_bank_config {
	unsigned long vaddr;	/* Virtual memory address of the bank */
	dma_addr_t paddr;	/* Physical memory address of the bank */
	uint32_t size_order;	/* Page order of the memory size */
	int type;
	int parity;
};

/* TLU data structure */
struct tlu {
	tlu_handle_t handle;
	int ref_count;
	struct tlu_bank_mem bank_mem[TLU_MAX_BANK_NUM];
	struct mutex lock;
	unsigned long page_base;
	struct tlu_bank_config bank_cfg[TLU_MAX_BANK_NUM];
	int bank_num;
	uint32_t table_map;
};

/* HTK table */
struct tlu_htk {
	struct htk htk;
	struct tlu *tlu;
};

/* CRT table */
struct tlu_crt {
	struct crt crt;
	struct tlu *tlu;
	void *buf;
};

/*====================== TLU Raw Access Public APIs ==========================*/
/*******************************************************************************
 * Description:
 *   This function is to configure a bank register of the TLU.
 *  * Parameters::
 *   tlu     - A pointer to a tlu_t data structure of a TLU which will use this
 *  	       bank.
 *   bank    - The index of the bank. It must be 0, 1, 2 or 3.
 *   par     - Indicates parity enable or disable for this bank memory. It is
 *             either TLU_MEM_PARITY_ENABLE or TLU_MEM_PARITY_DISABLE.
 *   tgt     - Indicates memory type. It is either TLU_MEM_LOCAL_BUS or
 *	       TLU_MEM_SYSTEM_DDR
 *   base_addr  - The physical base address of the memory bank.
 *  * Return::
 *   = 0  Success
 *   < 0  Error code
 ******************************************************************************/
int tlu_bank_config(struct tlu *tlu, int bank, int par, int tgt,
		unsigned long base_addr);
void tlu_table_config(struct tlu *tlu, int table, int type, int key_bits,
		int entry_size, int mask, int bank, uint32_t base_addr);
void *tlu_read(struct tlu *tlu, int table, uint32_t index, int len, void *data);
int tlu_write(struct tlu *tlu, int table, uint32_t index, int len, void *data);
int tlu_write_byte(struct tlu *tlu, int table, uint32_t addr, int len,
		void *data);
int tlu_acchash(struct tlu *tlu, int table, void *key, int key_len);
int tlu_add(struct tlu *tlu, int table, int addr, uint16_t data);
int tlu_find(struct tlu *tlu, int table, void *key, int key_bytes);
int tlu_findr(struct tlu *tlu, int table, void *key, int key_bytes, int offset,
		int len, void *data);
int tlu_findw(struct tlu *tlu, int table, void *key, int key_bytes, int offset,
		uint64_t data);
int tlu_bank_mem_alloc(struct tlu *tlu, int bank, int size);
int tlu_bank_mem_free(struct tlu *tlu, int bank, int addr);

/*******************************************************************************
 * Description:
 *   This function is to get statistics data of a gived TLU.
 * Parameters:
 *   tlu	- A pointer to a tlu_t data structure.
 *   stats	- A data structure to return statistics data.
 * Return:
 *   None
 ******************************************************************************/
void tlu_get_stats(struct tlu *tlu, union tlu_stats *stats);

/*******************************************************************************
 * Description:
 *   This function is to reset statistics data of a gived TLU.
 * Parameters:
 *   tlu	- A pointer to a tlu_t data structure.
 * Return:
 *   None
 ******************************************************************************/
void tlu_reset_stats(struct tlu *tlu);

/*====================== HTK Public APIs =====================================*/
/*******************************************************************************
 * Description:
 *   This function creates a HTK table in the specified TLU and table.
 * Parameters:
 *   tlu_htk  - A pointer to struct tlu_htk data structure to store the created
 *	       	HTK information. It will be reference by the succeeding HTK API
 *		accesses.
 *   tlu      - A pointer to a tlu_t data structure where the table will be
 *  		created.
 *   table    - An index of a physical table on which the HTK table is created.
 *	 	A valid value must be between 0 to 31 (inclusive).
 *   size    - The total table memory size in bytes.
 *   key_bits    - The size of key in bits. It must be 32, 64, 96 or 128.
 *   kdata_size  - The key-data entry size in bytes. It must be 8, 16, 32 or 64
 * 	           bytes.
 *   kdata_entry_num - The maximum key-data entry number
 *   hash_bits   - The hash bits. A valid value is between 1 to 16 inclusive.
 *   bank        - Pointing to a TLU bank memory data structure. The data
 *                 structure must be initialized by calling
 *                 _tlu_bank_mem_config.
 * Return:
 *   = 0   The table is successfully created.
 *   < 0   An error code denotes the operation failed.
 ******************************************************************************/
int tlu_htk_create(struct tlu_htk *tlu_htk, struct tlu *tlu, int table,
		int size, int key_bits, int kdata_size, int kdata_entry_num,
		int hash_bits, int bank);

/*******************************************************************************
 * Description:
 *   This function frees a htk table created by tlu_htk_create.
 * Parameters:
 *   tlu_htk  - A pointer to struct tlu_htk data structure which is initialized
 *              by tlu_htk_create.
 * Return:
 *   = 0     The table is successfully freed.
 *   = -1    Failed to free the table. It indicates a software internal error
 *           in this version of software.
 ******************************************************************************/
int tlu_htk_free(struct tlu_htk *tlu_htk);

/*******************************************************************************
 * Description:
 *   This function inserts a key-data entry into the specified HTK table.
 * Parameters:
 *   tlu_htk  - A pointer to struct tlu_htk data structure which is initialized
 *              by tlu_htk_create.
 *    entry   - The key-data entry to be inserted. The key must be at offset 0
 *              of the entry.
 * Return:
 *   >0	The entry is successfully inserted. The index of the entry is returned.
 *   < 0   An error code denotes the operation failed.
 ******************************************************************************/
int tlu_htk_insert(struct tlu_htk *tlu_htk, void *entry);

/*******************************************************************************
 * Description:
 *   This function deletes a key-data entry from the specified HTK table.
 * Parameters:
 *   tlu_htk  - A pointer to struct tlu_htk data structure which is initialized
 *              by tlu_htk_create.
 *   key      - The key of the entry to be delted.
 * Return:
 *   0     The entry is successfully deleted.
 *   < 0   An error code denotes the operation failed. Below are possible
 *         errors:
 *         TLU_MEM_ERROR - Memory error. It is an unrecoverable error
 *         TLU_CHAIN_TOO_LONG - The HTK table has more chained levels than
 *                              expected.
 *         TLU_ENTRY_NOT_EXIST - The table entry does not exist.
 ******************************************************************************/
int tlu_htk_delete(struct tlu_htk *tlu_htk, void *key);

/*******************************************************************************
 * Description:
 *   This function search a key in the specified TLU table.
 * Parameters:
 *   tlu_htk  - A pointer to struct tlu_htk data structure which is initialized
 *              by tlu_htk_create.
 *   key      - A pointer to the key to be searched
 * Return:
 *   >= 0  The index of found key
 *   < 0   Not found or error.
 *  	   TLU_NOT_FOUND  The key is not found.
 *	   TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
int tlu_htk_find(struct tlu_htk *htk, void *key);

/*******************************************************************************
 * Description:
 *   This function search a key in the specified TLU table and return data if
 *   found.
 * Parameters:
 *   tlu_htk  - A pointer to struct tlu_htk data structure which is initialized
 *              by tlu_htk_create.
 *   key      - A pointer to the key to be searched
 *   offset   - A byte offset within found data entry
 *   len      - Total number of bytes to be read. It must be a multiple of 8 and
 *              less than or equal to 32.
 *   data     - A buffer to return read data if it is not NULL. By giving a
 *              NULL, a caller can get data from TLU's data register at address
 *	        TLU_KDATA_PTR(tlu).
 * Return:
 *   >= 0  The index of found key
 *   < 0   Not found or error.
 *	   TLU_NOT_FOUND  The key is not found.
 *	   TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
int tlu_htk_findr(struct tlu_htk *htk, void *key, int offset, int len,
		void *data);

/*******************************************************************************
 * Description:
 *   This function search a key, then write 64 bits of data into the entry if
 *   found.
 * Parameters:
 *   tlu_htk  - A pointer to struct tlu_htk data structure which is initialized
 *              by tlu_htk_create.
 *   key      - A pointer to the key to be searched
 *   offset   - A byte offset within found data entry where the writing will be
 *   data     - The data which will replace the found data at offset <offset>
 * Return:
 *   >= 0  The index of found key
 *   < 0   Not found or error.
 *	   TLU_NOT_FOUND  The key is not found.
 *	   TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
int tlu_htk_findw(struct tlu_htk *htk, void *key, int offset, uint64_t data);

/*====================== CRT Public APIs =====================================*/
/*******************************************************************************
 * Description:
 *   This function is to create a CRT table.
 * Parameters:
 *   tlu_crt         - A pointer to struct tlu_crt data structure to store the
 *                     created CRT information. It will be reference by the
 *                     succeeding CRT API accesses.
 *   tlu	     - Indicating the tlu on which the CRT table will be
 *                     created.
 *   table	     - Indicating the table on wich the CRT table will be
 *                     created.
 *   size	     - Total number of bytes the table will use.
 *   key_bits        - The size of the key in bits. It must be 32, 64, 96 or
 *                     128.
 *   kdata_size      - The size of the data entry in bytes. It must be 8, 16,
 *                     32 or 64.
 *   kdata_entry_num - The maximum number of data entries in this CRT table.
 *   bank	     - The memory bank on which this CRT table will be created.
 *   compress        - Indicating that this is a compressed table if it is 1.
 * Return:
 *   0     The table is careated successfully.
 *   < 0   Error code if failure
 ******************************************************************************/
int tlu_crt_create(struct tlu_crt *tlu_crt, struct tlu *tlu, int table,
		int size, int key_bits, int kdata_size, int kdata_entry_num,
		int bank, int compress);

/*******************************************************************************
 * Description:
 *   This function is to free a previously created CRT table.
 * Parameters:
 *   tlu_crt  - A pointer to struct tlu_crt data structure which is initialized
 *              by tlu_crt_create.
 * Return:
 *  = 0   The table has been successfully freed.
 *  = -1  Failed to free the table. It is an internal error in this version of
 *	  software.
 ******************************************************************************/
int tlu_crt_free(struct tlu_crt *tlu_crt);

/*******************************************************************************
 * Description:
 *   This function is to insert an entry into the CRT table.
 * Parameters:
 *   tlu_crt  - A pointer to struct tlu_crt data structure which is initialized
 *              by tlu_crt_create.
 *    key     - The key of an entry to be inserted. A key should take the most
 *	        significant bits of the word if it is less than 32 bits.
 *   key_bits - Total number of bits of key.
 *   data     - Pointing to the data of the inserting entry.
 * Return:
 *   0     The entry has been successfully inserted.
 *   < 0   Error code if failure
 ******************************************************************************/
int tlu_crt_insert(struct tlu_crt *tlu_crt, void *key, int bits, void *data);

/*******************************************************************************
 * Description:
 *   This function is to delete an entry from the CRT table.
 * Parameters:
 *   tlu_crt  - A pointer to struct tlu_crt data structure which is initialized
 *              by tlu_crt_create.
 *   key      - The key of an entry to be deleted. A key should take the most
 *	        significant bits if it is less than 32 bits.
 *   key_bits - Total number of bits of key to be deleted.
 * Return:
 *   0     The entry is successfully deleted.
 *   < 0   Error code if failure
 ******************************************************************************/
int tlu_crt_delete(struct tlu_crt *tlu_crt, void *key, int key_bits);

/*******************************************************************************
 * Description:
 *   This function search a key in the specified TLU table.
 * Parameters:
 *   tlu_crt  - A pointer to struct tlu_crt data structure which is initialized
 *              by tlu_crt_create.
 *   key      - A pointer to the key to be searched
 * Return:
 *   >= 0  The index of found key
 *   < 0   Not found or error.
 *	   TLU_NOT_FOUND  The key is not found.
 *	   TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
int tlu_crt_find(struct tlu_crt *tlu_crt, void *key);

/*******************************************************************************
 * Description:
 *   This function search a key in the specified TLU table and return data if
 *   found.
 * Parameters:
 *   tlu_crt  - A pointer to struct tlu_crt data structure which is initialized
 *              by tlu_crt_create.
 *   key      - A pointer to the key to be searched
 *   offset   - A byte offset within found data entry. It must be multiple of 8.
 *   len      - Total number of bytes to be read. It must be a multiple of 8.
 *   data     - A buffer to return read data if it is not NULL. By giving a
 *              NULL, a caller can get data from TLU's data register at address
 *	        TLU_KDATA_PTR(tlu).
 * Return:
 *   >= 0  The index of found key
 *   < 0   Not found or error.
 *	   TLU_NOT_FOUND  The key is not found.
 *	   TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
int tlu_crt_findr(struct tlu_crt *tlu_crt, void *key, int offset, int len,
		void *data);

/*******************************************************************************
 * Description:
 *   This function search a key, then write 64 bits of data into the entry if
 *   found.
 * Parameters:
 *   tlu_crt  - A pointer to struct tlu_crt data structure which is initialized
 *              by tlu_crt_create.
 *   key      - A pointer to the key to be searched
 *   offset   - A byte offset within found data entry where the writing will be.
 * 	        It must be multiple of 8.
 *   data     - The data which will replace the found data at offset <offset>
 * Return:
 *   >= 0  The index of found key
 *   < 0   Not found or error.
 *	   TLU_NOT_FOUND  The key is not found.
 *	   TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
int tlu_crt_findw(struct tlu_crt *tlu_crt, void *key, int offset,
		uint64_t data);

/*******************************************************************************
 * Description:
 *   This function initializes a tlu with given memory information. Upon any
 *   failure of this function, callers are not required to call tlu_free.
 * Parameters:
 *   tlu          - A pointer to tlu_t data structure which is used to store
 *	            information regarding this tlu. It is required when
 *                  creating tables on it.
 *   tlu_phy_addr - The physical address of the tlu's processor interface.
 *   bank_num     - Total number if banks in the bank list specified by the
 *                  next parameter, bank_list.
 *   bank_list    - A list of configurations of memory banks which are used by
 *                  the tlu.
 * Return:
 *   = 0    Success
 *   = -1   Failure. A kernel failure message will be printed.
 ******************************************************************************/
int tlu_init(struct tlu *tlu, unsigned long tlu_phy_addr, int bank_num,
		struct tlu_bank_param *bank_list);

/*******************************************************************************
 * Description:
 *   This function frees a previousely initialized TLU.
 * Parameters:
 *   tlu  - A pointer to tlu_t data structure which is initialized by tlu_init.
 * Return:
 *  = 0    Success
 *  = -1   Failure. An internal error is occured for this version of software.
 ******************************************************************************/
void tlu_free(struct tlu *tlu);

#endif

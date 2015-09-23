/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains definitions, macros and inline implementations of TLU
 * access layer including raw access and blocking access.
 *
 * The following are included in this file.
 *	- Error code definitions
 *	- Log macros for TLU Access Layer
 *	- Raw access inline implementations
 *	- Block access inline functions
 *
 * Changelog:
 * Mon Feb 4 2008 Zhichun Hua <zhichun.hua@freescale.com>
 * - Change mb() to iobarrier_rw().
 *
 * This file is part of the Linux kernel
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef TLU_ACCESS_H
#define TLU_ACCESS_H

#include "tlu_osdef.h"
#include "tlu_hw.h"
#include "tlu_log.h"

/* Make table 'size' field a fixed value: 8. Thus all indexes will be scaled by
 * 8 bytes instead of by the 'size' field of table configure. This makes
 * software a lot simple, also debugging. The harware seems not prevent doing
 * this.  This is internal use only. Users should not enable it.
 */
/* #define TLU_FIXED_INDEX_SCALE */

/*====================== Public TLU Error Codes ==============================*/
/* Hardware memory error e.g. parity error */
#define TLU_MEM_ERROR 		-1
/* A table is broken */
#define TLU_TABLE_ERROR	 	-2
/* A table has too many chained entries or sub-tables */
#define TLU_CHAIN_TOO_LONG  	-3
/* Invalid API parameters */
#define TLU_INVALID_PARAM   	-4
/* A TLU table is full. No more data entries. */
#define TLU_TABLE_FULL	  	-5
/* Attempting to insert an entry which already exists */
#define TLU_ENTRY_EXIST	 	-6
/* Attempting to delete an entry which does not exist */
#define TLU_ENTRY_NOT_EXIST 	-7
/* A key is not found */
#define TLU_NOT_FOUND	   	-8
/* Insufficient heap memory (e.g bank heap or pool heap) */
#define TLU_OUT_OF_MEM	  	-9
/* An entry can not be inserted by various reasons except above defined ones */
/* This error code is not used any more
#define TLU_INSERT_FAIL	 -10
*/
/* The entry is redundant */
#define TLU_REDUNDANT_ENTRY 	-11

/*======================  Data Structures and Type =====================*/
/* TLU Handle */
typedef unsigned long tlu_handle_t;

/* Please refer to PowerQUICC III Reference Manual for the meaning of
 * each field
 */
union tlu_stats {
	struct{
		uint32_t mem_read;
		uint32_t mem_write;
		uint32_t total_find;
		uint32_t htk_find;
		uint32_t crt_find;
		uint32_t chained_hash_find;
		uint32_t flat_data_find;
		uint32_t find_success;
		uint32_t find_fail;
		uint32_t hash_collision;
		uint32_t crt_levels;
		uint32_t carry_out;
		uint32_t carry_mask;
	};
	struct{
		uint32_t cramr;
		uint32_t cramw;
		uint32_t cfind;
		uint32_t cthtk;
		uint32_t ctcrt;
		uint32_t ctchs;
		uint32_t ctdat;
		uint32_t chits;
		uint32_t cmiss;
		uint32_t chcol;
		uint32_t ccrtl;
		uint32_t caro;
		uint32_t carm;
	};
};

/* --------------------- Internal Access Log Macros --------------------------*/
#define TLU_LOG_ACC	 0x00000010
#define TLU_ACC_LOG(format, ...) \
	TLU_LOG(TLU_LOG_ACC, "TLU", format, ##__VA_ARGS__)
#define TLU_ACC_LOG_MEM(buf, len, format, ...) \
	TLU_LOG_MEM(TLU_LOG_ACC, buf, len, "TLU M", format, ##__VA_ARGS__)
#define TLU_ACC_LOG_MEM_RD(buf, len, format, ...) \
	TLU_LOG_MEM(TLU_LOG_ACC, buf, len, "TLU R", format, ##__VA_ARGS__)
#define TLU_ACC_CRIT(format, ...) \
	TLU_LOG(TLU_LOG_CRIT, "ACC", format, ##__VA_ARGS__)
#define TLU_ACC_WARN(format, ...) \
	TLU_LOG(TLU_LOG_WARN, "ACC", format, ##__VA_ARGS__)

/*====================== Public Macros and APIs =============================*/
#define TLU_MAX_NUM 4
#define TLU_BASE_ADDR(tlu) ((unsigned long)(tlu))
#define TLU_CMD_PTR(tlu) ((volatile uint32_t *)(TLU_BASE_ADDR(tlu) + TLU_CMDOP))
#define TLU_KDATA_PTR(tlu) \
	((volatile uint32_t *)(TLU_BASE_ADDR(tlu) + TLU_KDATA))
#define TLU_KDATA_PTR_BY_CMD(tlu_cmd) \
	((volatile uint32_t *)(((unsigned long)(tlu_cmd)) \
	      + TLU_KDATA - TLU_CMDOP))
#define TLU_REG(tlu, reg) *(volatile uint32_t *)(TLU_BASE_ADDR(tlu) + reg)
#define TLU_MAX_RW_LEN 32

#define __tlu_write tlu_write64
int _tlu_bank_config(tlu_handle_t tlu, int bank, int par, int tgt,
		unsigned long base_addr);
void _tlu_table_config(tlu_handle_t tlu, int table, int type, int key_bits,
		int entry_size, int mask, int bank, uint32_t base_addr);
void *_tlu_read_data(tlu_handle_t tlu, int table, int index, int offset,
		int data_size, void *data);
void *_tlu_read(tlu_handle_t tlu, int table, int index, int data_size,
		void *data);
int _tlu_write_data(tlu_handle_t tlu, int table, int index, int offset,
		int data_size, void *data);
int _tlu_write(tlu_handle_t tlu, int table, int index, int data_size,
		void *data);
int _tlu_write_byte(tlu_handle_t tlu, int table, uint32_t addr, int len,
		void *data);
int _tlu_add(tlu_handle_t tlu, int table, int addr, uint16_t data);
int _tlu_acchash(tlu_handle_t tlu, int table, void *key, int key_len);
void _tlu_get_stats(tlu_handle_t tlu, union tlu_stats *stats);
void _tlu_reset_stats(tlu_handle_t tlu);
#ifdef TLU_DEBUG
void tlu_acc_log(tlu_handle_t tlu, int table, int cmd, int len);
#else
#define tlu_acc_log(tlu, table, cmd, len)
#endif

/* --------------------- Internal Functions ----------------------------------*/
/* TLU hardware requires 32-bit aligned access for all registers. The following
 * three functions are introduced to meet this requirement
 */
static inline void tlu_memcpy32(void *dest, void *src, int len)
{
	int i;

	for (i = 0; i < len/4; i++)
		((uint32_t *)dest)[i] = ((uint32_t *)src)[i];
}

static inline int tlu_memcmp32(void *buf1, void *buf2, int len)
{
	int i;

	for (i = 0; i < len/4; i++) {
		if (((uint32_t *)buf1)[i] != ((uint32_t *)buf2)[i])
			return 1;
	}
	return 0;
}

static inline void tlu_memset32(void *dest, uint32_t value, int len)
{
	int i;

	for (i = 0; i < len/4; i++)
		((uint32_t *)dest)[i] = value;
}

/* This file is platform independent. Instead of using get_order from linux
 * (asm/page.h) it is re-written here.
 */
static inline uint32_t tlu_get_order(uint32_t size)
{
	uint32_t order;

	order = 0;
	while ((1 << order) < size)
		order++;
	return order;
}

static inline int tlu_get_bit(void *buf, int bit_pos)
{
	return (((uint32_t *)buf)[bit_pos/(sizeof(uint32_t)*8)]
			>> (31 - bit_pos % (sizeof(uint32_t)*8))) & 1;
}

static inline int tlu_bit_diff(void *buf1, void *buf2, int len)
{
	int count;

	count = 0;
	while ((count < (len<<3))
			&& (tlu_get_bit(buf1, count)
				== tlu_get_bit(buf2, count)))
		count++;
	return count;
}

/*====================== TLU Raw Access Public APIs ==========================*/

/*******************************************************************************
 * Description:
 *   This function is to setup a TLU for access. The return value, a tlu hadle,
 *   will be used by most of tlu access APIs. Thus this function has to be
 *   called before calling any other functions with require a TLU handle.
 * Parameters:
 *   addr -  The base address of the TLU. It should be in a processor
 *           accessible memory space.
 * Return:
 *   The TLU handle is returned.
 ******************************************************************************/
static inline tlu_handle_t tlu_setup(uint32_t addr)
{
	return (tlu_handle_t)addr;
}

/*******************************************************************************
 * Description:
 *   This function writes a value into a TLU register.
 * Parameters:
 *   tlu   - TLU handle returned by tlu_setup.
 *   reg   - the register address
 *   value - The value to be written into the register
 * Return:
 *   None
 ******************************************************************************/
static inline void tlu_write_reg(tlu_handle_t tlu, uint32_t reg, uint32_t value)
{
	TLU_REG(tlu, reg) = value;
	mb();
}

/*******************************************************************************
 * Description:
 *   This function read a TLU register.
 * Parameters:
 *   tlu   - TLU handle returned by tlu_setup.
 *   reg   - the register address
 * Return:
 *   The value of the register
 ******************************************************************************/
static inline uint32_t tlu_read_reg(tlu_handle_t tlu, uint32_t reg)
{
	return TLU_REG(tlu, reg);
}

/*******************************************************************************
 * Description:
 *   This function gets tlu status.
 * Parameters:
 *   tlu   - TLU handle
 * Return:
 *   The current TLU status
 ******************************************************************************/
static inline uint32_t tlu_cstat(tlu_handle_t tlu)
{
	return tlu_read_reg(tlu, TLU_CSTAT);
}

/*******************************************************************************
 * Description:
 *   This function fills a TLU command register. It only fills the 1st word.
 *   offset and len are coded values as TLU hardware required instead of in
 *   bytes.
 * Parameters:
 *   tlu    - TLU handle
 *   cmd    - The command code
 *   table  - The table index of the command
 *   offset - "offset" field the command if existing. Otherwise should be 0.
 *   len    - "len" field of the command if existing. Otherwise should be 0.
 *   data   - "data" field of the command if existing. Otherwise should be 0.
 * Return:
 *   None
 ******************************************************************************/
static inline void tlu_fill_cmd0(tlu_handle_t tlu, int cmd, int table,
		int offset, int len, uint16_t data)
{
	volatile uint32_t *cmd_ptr;
	cmd_ptr = TLU_CMD_PTR(tlu);
	cmd_ptr[0] = (cmd << TLU_CMD_CMD_SHIFT) | (table << TLU_CMD_TBL_SHIFT)
			| (offset << TLU_CMD_OFFSET_SHIFT)
			| (len << TLU_CMD_LEN_SHIFT)
			| (data << TLU_CMD_DATA_SHIFT);
	tlu_acc_log(tlu, table, cmd, len);
}

/*******************************************************************************
 * Description:
 *   This function fills a TLU command register. offset and len are coded
 *   values as TLU hardware required instead of in bytes.
 * Parameters:
 *   index   - "index" field of the command if existing. Otherwise should be 0.
 *   tag     - "tag" field of the command if existing. Otherwise should be 0.
 *   Others  - Please refer to the description of tlu_fill_cmd0.
 * Return:
 *   None
 ******************************************************************************/
static inline void tlu_fill_cmd(tlu_handle_t tlu, int cmd, int table, int index,
		int offset, int len, int tag, uint16_t data)
{
	volatile uint32_t *cmd_ptr;
	cmd_ptr = TLU_CMD_PTR(tlu);
	cmd_ptr[1] = (index << TLU_CMD_INDEX_SHIFT)
			| (tag << TLU_CMD_TAG_SHIFT);
	cmd_ptr[0] = (cmd << TLU_CMD_CMD_SHIFT) | (table << TLU_CMD_TBL_SHIFT)
			| (offset << TLU_CMD_OFFSET_SHIFT)
			| (len << TLU_CMD_LEN_SHIFT)
			| (data << TLU_CMD_DATA_SHIFT);
	tlu_acc_log(tlu, table, cmd, len);
}

/*******************************************************************************
 * Description:
 *   This function is to fill a TABLE_WRITE command into TLU's command
 *   registers. Data should filled at TLU data register before it is called.
 * Parameters:
 *   tlu	- TLU handle which is the table belonging to.
 *   table  - The index of the table to be written.
 *   index  - The index of the memory to be written. It is the byte address
 *            divided by data entry size configure in the table.
 *   offset - The byte offset of the written location. It must be multiple of 8.
 *   len    - The total number of bytes to be written. It must be multiple of 8
 *            and less or equal than 32.
 * Return:
 *   None
 ******************************************************************************/
static inline void _tlu_write64(tlu_handle_t tlu, int table, int index,
		int offset, int len)
{
	tlu_fill_cmd(tlu, TLU_WRITE, table, index, offset / TLU_UNIT_SIZE,
			TLU_LEN_CODE(len), 0xFF, 0);
}

/*******************************************************************************
 * Description:
 *   This function is to fill a TABLE_WRITE command into TLU's command
 *   registers.
 * Parameters:
 *   tlu    - TLU handle which is the table belonging to.
 *   table  - The index of the table to be written.
 *   index  - The index of the memory to be written. It is the byte address
 *            divided by data entry size configure in the table.
 *   offset - The byte offset of the written location.
 *   len    - The total number of bytes to be written. It must be multiple of
 *            8.
 *   data   - The pointer of the data buffer.
 * Return:
 *   None
 ******************************************************************************/
static inline void tlu_write64(tlu_handle_t tlu, int table, int index,
		int offset, int len, void *data)
{
	tlu_memcpy32((void *)TLU_KDATA_PTR(tlu), data, len);
	_tlu_write64(tlu, table, index, offset, len);
}

/*******************************************************************************
 * Description:
 *   This function get index scale configuration of a table which is the entry
 *   size field of the table. Based on PowerQUICC III Reference Manuals, below
 *   is how the size field maps to data enty size.
 *	   data entry size = 1 << (size field + 3)
 * Parameters:
 *   tlu   - TLU handle
 *   table - the table index
 * Return:
 *   The index scale value
 ******************************************************************************/
static inline int tlu_get_table_index_scale(tlu_handle_t tlu, int table)
{
#ifdef TLU_FIXED_INDEX_SCALE
	return 0;
#else
	uint32_t cfg;
	cfg = tlu_read_reg(tlu, TLU_PTBL0 + table * 4);
	return ((struct tlu_table_config *)&cfg)->size;
#endif
}

/*******************************************************************************
 * Description:
 *   This function is to fill a TABLE_WRITE command into TLU's command
 *   registers.
 * Parameters:
 *   tlu   - TLU handle which is the table belonging to.
 *   table - The index of the table to be written.
 *   addr  - The byte address of the memory to be written
 *   len -   The total number of bytes to be written. It must be less than 8.
 *   data -  The pointer of the data buffer.
 * Return:
 *   None
 ******************************************************************************/
static inline void tlu_send_cmd_write_byte(tlu_handle_t tlu, int table,
		uint32_t addr, int len, void *data)
{
	uint8_t tmp[8];
	int index, scale, offset;

	scale = tlu_get_table_index_scale(tlu, table);
	offset = (addr >> 3) & ((1 << scale) - 1);
	index = addr >> (scale + 3);

	memcpy(tmp + (addr & 7), data, len);
	tlu_memcpy32((void *)TLU_KDATA_PTR(tlu), tmp, (len + 7) & (~7));
	tlu_fill_cmd(tlu, TLU_WRITE, table, index, offset, TLU_LEN_CODE(len),
			((1 << len) - 1) << (8 - (addr & 7) - len), 0);
}

/*******************************************************************************
 * Description:
 *   This function is to fill a TABLE_READ command into TLU's command registers.
 * Parameters:
 *   tlu    - TLU handle.
 *   table  - The index of the table to be read.
 *   index  - The index of the memory to be read. It is the byte address divided
 *            by data entry size configure in the table.
 *  offset  - The byte offset of the reading location.
 *  len	    - The total number of bytes to be read. It must be multiple of 8.
 * Return:
 *   None
 ******************************************************************************/
static inline void tlu_send_cmd_read(tlu_handle_t tlu, int table, int index,
		int offset, int len)
{
	tlu_fill_cmd(tlu, TLU_READ, table, index, offset / TLU_UNIT_SIZE,
			TLU_LEN_CODE(len), 0, 0);
}

/*******************************************************************************
 * Description:
 *   This function is to fill a TABLE_FIND command into TLU's command registers.
 * Parameters:
 *   tlu   - TLU to be searched
 *   table - The index of the table to be searched
 *   key   - A pointer to the key to be searched
 *   key_len - The total number of bytes of the key
 * Return:
 *   None
 ******************************************************************************/
static inline void tlu_send_cmd_find(tlu_handle_t tlu, int table, void *key,
		int key_len)
{
	tlu_memcpy32((void *)TLU_KDATA_PTR(tlu), key, key_len);
	tlu_fill_cmd0(tlu, TLU_FIND, table, 0, 0, 0);
}

/*******************************************************************************
 * Description:
 *   This function is to fill a TABLE_FINDR command into TLU's command
 *   registers.
 * Parameters:
 *   tlu     - TLU to be searched
 *   table   - The index of the table to be searched
 *   key     - A pointer to the key to be searched
 *   key_len - The total number of bytes of the key
 *   offset  - A byte offset within found data entry. It must be 8 byte aligned.
 *   len     - Total number of bytes to be read. It must be a multiple of 8.
 * Return:
 *   None
 ******************************************************************************/
static inline void tlu_send_cmd_findr(tlu_handle_t tlu, int table, void *key,
		int key_len, int offset, int len)
{
	tlu_memcpy32((void *)TLU_KDATA_PTR(tlu), key, key_len);
	tlu_fill_cmd0(tlu, TLU_FINDR, table, offset / TLU_UNIT_SIZE,
			TLU_LEN_CODE(len), 0);
}

/*******************************************************************************
 * Description:
 *   This function is to fill a TABLE_FINDW command into TLU's command
 *   registers.
 * Parameters:
 *   tlu     - TLU to be searched
 *   table   - The index of the table to be searched
 *   key     - A pointer to the key to be searched
 *   key_len - The total number of bytes of the key
 *   offset  - A byte offset within found data entry
 *   data    - A 64-bit data too be written into the found entry at offset
 *             <offset>.
 * Return:
 *   None
 ******************************************************************************/
static inline void tlu_send_cmd_findw(tlu_handle_t tlu, int table, void *key,
		int key_len, int offset, uint64_t data)
{
	volatile uint64_t *data_ptr;

	data_ptr = (uint64_t *)TLU_KDATA_PTR(tlu);
	tlu_memcpy32((void *)data_ptr, key, key_len);
	data_ptr[3] = data;
	tlu_fill_cmd0(tlu, TLU_FINDW, table, offset / TLU_UNIT_SIZE, 0, 0);
}

/*******************************************************************************
 * Description:
 *   This function is to fill a ADD command into TLU's command registers.
 * Parameters:
 *   tlu    - TLU handle
 *   table  - The index of the table
 *   index  - The index of the memory to be read. It is the byte address
 *            divided by data entry size configure in the table.
 *   offset - A byte index of the location of data to be added on.
 *   tag    - Indicating which 4 bytes are going to be added data on.
 *   data   - A 16-bit data too be added.
 * Return:
 *   None
 ******************************************************************************/
static inline void tlu_send_cmd_add(tlu_handle_t tlu, int table, int index,
		int offset, int tag, uint16_t data)
{
	tlu_fill_cmd(tlu, TLU_ADD, table, index, offset / TLU_UNIT_SIZE, 0,
			tag, data);
}

/*******************************************************************************
 * Description:
 *   This function is to fill a ACCHASH command into TLU's command registers.
 * Parameters:
 *   tlu     - TLU handle
 *   table   - The index of the table
 *   key     - A pointer to the key
 *   key_len - The total number of bytes of the key
 * Return:
 *   None
 ******************************************************************************/
static inline void tlu_send_cmd_acchash(tlu_handle_t tlu, int table, void *key,
		int key_len)
{
	tlu_memcpy32((void *)TLU_KDATA_PTR(tlu), key, key_len);
	tlu_fill_cmd0(tlu, TLU_ACCHASH, table, 0, TLU_LEN_CODE(key_len), 0);
}

/**************************** TLU Blocking Access *****************************/
static inline uint32_t __tlu_ready(tlu_handle_t tlu)
{
	uint32_t cstat;

	/* iobarrier_rw is sufficient because it is caching-inhibited */
	iobarrier_rw();
	do {
		cstat = TLU_REG(tlu, TLU_CSTAT);
	} while (!(cstat & TLU_CSTAT_RDY));
	TLU_ACC_LOG("CSTAT: %08x\n", cstat);
	return cstat;
}

/*******************************************************************************
 * Description:
 *   This function search a key in the specified TLU table.
 * Parameters:
 *   tlu   - TLU handle
 *   table - Table index
 * Return:
 *   >= 0  The index of found entry
 *   < 0   Not found or error.
 *         TLU_NOT_FOUND  The key is not found.
 *         TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
static inline int _tlu_find_ready(tlu_handle_t tlu, int table)
{
	uint32_t cstat;

	cstat = __tlu_ready(tlu);
	if (cstat & TLU_CSTAT_FAIL) {
		if ((cstat & TLU_CSTAT_ERR) == 0) {
			return TLU_NOT_FOUND;
		} else {
			TLU_ACC_CRIT("Find fail. cstat=%08x tlu=%x table=%d\n",
					cstat, tlu, table);
			return TLU_MEM_ERROR;
		}
	}
	return cstat & TLU_CSTAT_INDEX;
}

/*******************************************************************************
 * Description:
 *   This function search a key in the specified TLU table.
 * Parameters:
 *   tlu       - The TLU to be searched
 *   table     - The table to be searched
 *   key       - A pointer to the key to be searched
 *   key_bytes - Total number of bytes of the key.
 * Return:
 *   >= 0  The index of found key
 *   < 0   Not found or error.
 *         TLU_NOT_FOUND  The key is not found.
 *         TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
static inline int _tlu_find(tlu_handle_t tlu, int table, void *key,
		int key_bytes)
{
	tlu_send_cmd_find(tlu, table, key, key_bytes);
	return _tlu_find_ready(tlu, table);
}

/*******************************************************************************
 * Description:
 *   This function search a key in the specified TLU table and read the found
 *   data.
 * Parameters:
 *   tlu       - The TLU to be searched
 *   table     - The table to be searched
 *   key       - A pointer to the key to be searched
 *   key_bytes - Total number of bytes of the key.
 *   offset    - A byte offset within found data entry
 *   len       - Total number of bytes to be read. It must be a multiple of 8
 *               and must not be 0.
 *   data      - A buffer to return read data if it is not NULL. By giving a
 *               NULL, a caller can get data from TLU's data register at
 *               address TLU_KDATA_PTR(tlu).
 * Return:
 *   >= 0  The index of found key
 *   < 0   Not found or error.
 *         TLU_NOT_FOUND  The key is not found.
 *         TLU_MEM_ERROR  TLU memory error detected
*******************************************************************************/
static inline int _tlu_findr(tlu_handle_t tlu, int table, void *key,
		int key_bytes, int offset, int len, void *data)
{
	int index;

	tlu_send_cmd_findr(tlu, table, key, key_bytes, offset, len);
	index = _tlu_find_ready(tlu, table);
	if (index >= 0 && data != NULL)
		tlu_memcpy32(data, (void *)TLU_KDATA_PTR(tlu), len);
	return index;
}

/*******************************************************************************
 * Description:
 *   This function search a key in the specified TLU table and modify 64 bits
 *   of data if found.
 * Parameters:
 *   tlu       - The TLU to be searched
 *   table     - The table to be searched
 *   key       - A pointer to the key to be searched
 *   key_bytes - Total number of bytes of the key.
 *   offset    - A byte offset within found data entry
 *   data      - Data to be written if found
 * Return:
 *   >= 0  The index of found key
 *   < 0   Not found or error.
 *         TLU_NOT_FOUND  The key is not found
 *         TLU_MEM_ERROR  TLU memory error detected
 ******************************************************************************/
static inline int _tlu_findw(tlu_handle_t tlu, int table, void *key,
		int key_bytes, int offset, uint64_t data)
{
	tlu_send_cmd_findw(tlu, table, key, key_bytes, offset, data);
	return _tlu_find_ready(tlu, table);
}

#endif

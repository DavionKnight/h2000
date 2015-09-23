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
/* Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains an implementation of the TLU access layer including raw
 * access and blocking access. The raw access APIs allow applications to
 * communicate with TLU hardware without dealing with every detail of a hardware
 * interface.  For example, reading or writing tlu memory, sending a command to
 * TLU etc.  Those APIs basically fill command data structures and send it to a
 * TLU, then return to callers without waiting for operations to be finished.
 *
 * The blocking access APIs allow applications to have a convenient way to
 * execute hardware functions. They send a command to a TLU then wait it to be
 * finished before returning to callers.
 *
 * For TLU memory access, two type of read/write functions are provided. One
 * requires data entry index and offset within the entry. The index is scaled
 * by the size configured in the 'size' field of a TLU table. It is the nature
 * of the TLU hardware functionality. Another requires double word (64 bit)
 * index.  It is intended for accessing of table memory except data entry, e.g,
 * hash table, Trie entry and IPTD table etc.
 */
#include <asm/tlu_access.h>

/* This should only apply to commands whose execution time is fixed. E.g. read
 * and write etc.  It should not apply to command such as find
 */
#define TLU_MAX_WAIT_COUNT 1000

#ifdef TLU_DEBUG
/*******************************************************************************
 * Description:
 *   This function is to log a read/write command. It is a debug function.
 * Parameters:
 *   tlu   - TLU handle
 *   table - Table Id
 *   cmd   - Command code
 * Return:
 *   None
 ******************************************************************************/
void tlu_acc_rw_log(tlu_handle_t tlu, int table, int cmd)
{
	int scale, index, offset;
	uint32_t cmd_word0, cmd_word1;
	volatile uint32_t *cmd_ptr;

	cmd_ptr = TLU_CMD_PTR(tlu);
	scale = tlu_get_table_index_scale(tlu, table);
	cmd_word0 = cmd_ptr[0];
	cmd_word1 = cmd_ptr[1];
	index = (cmd_word1 >> TLU_CMD_INDEX_SHIFT) & TLU_CMD_INDEX_MASK;
	offset = (cmd_word0 >> TLU_CMD_OFFSET_SHIFT) & TLU_CMD_OFFSET_MASK;
	TLU_ACC_LOG("W %p: %08x %08x [%s: %08x:%d]\n",
		cmd_ptr, cmd_word0, cmd_word1, tlu_cmd_str[cmd],
		(index << (scale + 3)) + offset * TLU_UNIT_SIZE,
		(((cmd_word0 >> TLU_CMD_LEN_SHIFT) & TLU_CMD_LEN_MASK) + 1)
		* TLU_UNIT_SIZE);
}

/*******************************************************************************
 * Description:
 *   This function is to config TLU's bank memory.
 * Parameters:
 *   tlu   - TLU handle
 *   table - Table Id
 *   cmd   - Command code
 *   len   - Data length code
 * Return:
 *   None
 ******************************************************************************/
void tlu_acc_log(tlu_handle_t tlu, int table, int cmd, int len)
{
	volatile uint32_t *cmd_ptr;

	cmd_ptr = TLU_CMD_PTR(tlu);
	switch (cmd) {
	case TLU_WRITE:
		TLU_ACC_LOG_MEM((void *)TLU_KDATA_PTR(tlu), (len + 1) * 8,
				NULL);
		tlu_acc_rw_log(tlu, table, cmd);
		break;
	case TLU_READ:
		tlu_acc_rw_log(tlu, table, cmd);
		break;
	case TLU_FIND:
	case TLU_FINDR:
	case TLU_FINDW:
	case TLU_ACCHASH:
		TLU_ACC_LOG_MEM((void *)TLU_KDATA_PTR(tlu), TLU_MAX_KEY_BYTES,
				NULL);
		TLU_ACC_LOG("W %p: %08x %08x [%s]\n",
			cmd_ptr, cmd_ptr[0], cmd_ptr[1], tlu_cmd_str[cmd]) ;
		break;
	default:
		TLU_ACC_LOG("W %p: %08x %08x [%s]\n",
			cmd_ptr, cmd_ptr[0], cmd_ptr[1], tlu_cmd_str[cmd]) ;
		break;
	}
}
#endif

/*******************************************************************************
 * Description:
 *   This function is to config TLU's bank memory.
 * Parameters:
 *   tlu   - TLU handle
 *   bank  - The index of a bank to be configured
 *   par   - Parity enable or disable for this bank memory. It is either
 *	     TLU_MEM_PARITY_ENABLE or TLU_MEM_PARITY_DISABLE
 *   tgt   - Memory target. It is either TLU_MEM_LOCAL_BUS or TLU_MEM_SYSTEM_DDR
 *   base_addr  - The base address of the memory bank with 256M byte aligned.
 *   size  - Total number of bytes of the bank memory. This is for software bank
 *	     memory allocation.
 * Return:
 *   = 0  Success
 *   < 0  Error code
 *        TLU_INVALID_PARAM  Invalid parameter
 ******************************************************************************/
int _tlu_bank_config(tlu_handle_t tlu, int bank, int par, int tgt,
		unsigned long base_addr)
{
	struct tlu_bank cfg;

	if (base_addr & (TLU_BANK_BOUNDARY - 1)) {
		TLU_ACC_LOG("The base address (%08x) is not 256M aligned.\n",
			base_addr);
		return TLU_INVALID_PARAM;
	}
	*(uint32_t *)&cfg = 0;
	cfg.par = par;
	cfg.tgt = tgt;
	cfg.base = base_addr >> TLU_BANK_BIT_BOUNDARY;
	TLU_ACC_LOG("BANK[%d]: %08x\n", bank, *(uint32_t *)&cfg);
	tlu_write_reg(tlu, TLU_MBANK0 + bank * 4 , *(uint32_t *)&cfg);
	return 0;
}

/*******************************************************************************
 * Description:
 *    This function is to configure a specified table.
 * Parameters:
 *   tlu -   TLU handle which is counting from 0 and up.
 *   table - The index of the table to be configured. It is counting from 0.
 *   type -  The type of the table. One of the following table type is
 *           supported:
 *  	     TLU_TABLE_TYPE_FLAT - The table is a flat indexed one.
 *	     TLU_TABLE_TYPE_CRT  - The initial sub-table is an IPTD table.
 *	  			   It can initiates a CRT table or a chained
 *				   hash table.
 *	     TLU_TABLE_TYPE_HASH - It is a Hash-Trie-Key (HTK) table.
 *   key_bits - The key length in bit if table type is TLU_TABLE_TYPE_FLAT or
 *  	        TLU_TABLE_TYPE_CRT. It is ignored if the table type is
 *	        TLU_TABLE_TYPE_FLAT because key length of this type of table
 *                  is always 32 bits. It can only be 32, 64, 96 or 128.
 *                  The least significant 5 bits of any other valuse will be
 *                  ignored.
 *   entry_size - The table entry size in byte. It can only be 8, 16, 32
 *                or 64. Any other values will be adjusted downward to a
 *                closest value. e.g. 31 will be adjusted to a value 16.
 *   mask      -  MASK field of the PTBLx register.
 *   bank      -  The index of memory bank which is used for this table.
 *   base_addr - The based address of the table memory. The most significant
 *               4 bits are ignored and the least significant 12 bits are
 *               ignored if they are not 0s.
 * Return:
 *   None
 ******************************************************************************/
void _tlu_table_config(tlu_handle_t tlu, int table, int type, int key_bits,
		int entry_size, int mask, int bank, uint32_t base_addr)
{
	struct tlu_table_config cfg;

	cfg.type = type;
	cfg.klen = TLU_KEY_LEN(key_bits);
#ifdef TLU_FIXED_INDEX_SCALE
	cfg.size = TLU_KDATA_SIZE_8;
#else
	cfg.size = tlu_get_order(entry_size >> 3);
#endif
	cfg.mask = mask;
	cfg.bank = bank;
	cfg.baddr = (base_addr >> 12) & 0xFFFF;

	TLU_ACC_LOG("PTBL[%d]: %08x\n", table, *(uint32_t *)&cfg);
	tlu_write_reg(tlu, TLU_PTBL0 + table * 4, *(uint32_t *)&cfg);
}

uint32_t _tlu_ready(tlu_handle_t tlu)
{
	uint32_t cstat;
	int count;

	count = TLU_MAX_WAIT_COUNT;
	mb();
	while (--count > 0) {
		cstat = tlu_cstat(tlu);
		if ((cstat & TLU_CSTAT_RDY) != 0) {
			TLU_ACC_LOG("CSTAT: %08x\n", cstat);
			return cstat;
		}
	}
	TLU_ACC_CRIT("TLU polling timeout: %08x\n", cstat);
	return cstat | TLU_CSTAT_FAIL;
}

/*******************************************************************************
 * Description:
 *   This function writes data up to 32 bytes into TLU table memroy using data
 *   entry index.
 * Parameters:
 *   tlu       - The TLU whose memory will be read
 *   table     - The table to which the memory belongs
 *   index     - The data entry index of the memory relative to the table base.
 *   offset    - The byte offset within the data entry where data to be
 *	         written
 *   data_size - Total number of bytes to be written. It must be a multiple of
 *	         8 bytes. The maximum data size is 32 bytes.
 *   data      - A pointer to the data to be written if it is not NULL.
 *               Otherwise it is assumed that data has been put in the TLU
 *               data register.
 * Return:
 *   = 0   Writing is success
 *   < 0   Error code
 ******************************************************************************/
int __tlu_write_data(tlu_handle_t tlu, int table, int index, int offset,
		int data_size, void *data)
{
	uint32_t cstat;

	if (!data)
		_tlu_write64(tlu, table, index, offset, data_size);
	else
		tlu_write64(tlu, table, index, offset, data_size, data);

	cstat = _tlu_ready(tlu);
	if (cstat & TLU_CSTAT_FAIL) {
		TLU_ACC_CRIT("Write failed. cstat = %08x tlu = %x table = %d "
				"index = %x size = %x\n",
				cstat, tlu, table, index, data_size);
		return TLU_MEM_ERROR;
	}
	return 0;
}

/*******************************************************************************
 * Description:
 *   This function writes data up to 64 bytes into TLU table memroy using data
 *   entry index.
 * Parameters:
 *   tlu       - The TLU whose memory will be read
 *   table     - The table to which the memory belongs
 *   index     - The data entry index of the memory relative to the table base.
 *   offset    - The byte offset within the data entry where data to be
 *	         written
 *   data_size - Total number of bytes to be written. It must be a multiple of
 *	         8 bytes. The maximum data size is 64 bytes.
 *   data      - A pointer to the data to be written if it is not NULL.
 *               Otherwise it is assumed that data has been put in the TLU
 *               data register.
 * Return:
 *   = 0  Writing is success
 *   < 0   Error code
 ******************************************************************************/
int _tlu_write_data(tlu_handle_t tlu, int table, int index, int offset,
		int data_size, void *data)
{
	int rc;

	if (data_size <= TLU_MAX_RW_LEN) {
		return __tlu_write_data(tlu, table, index, offset, data_size,
				data);
	} else {
		rc = __tlu_write_data(tlu, table, index, offset,
						TLU_MAX_RW_LEN, data);
		if (rc < 0)
			return rc;

		return __tlu_write_data(tlu, table, index, TLU_MAX_RW_LEN,
				data_size - TLU_MAX_RW_LEN,
				((uint8_t *)data) + TLU_MAX_RW_LEN);
	}
}

/*******************************************************************************
 * Description:
 *   This function writes data into TLU table memroy using double word index.
 * Parameters:
 *   tlu       - The TLU whose memory will be read
 *   table     - The table to which the memory belongs
 *   index     - The double word index of the memory relative to the table base.
 *   data_size - Total number of bytes to be written. It must be a multiple of
 *	         8 bytes.
 *   data      - A pointer to the data to be written if it is not NULL.
 *               Otherwise it is assumed that data has been put in the TLU
 *               data register.
 * Return:
 *   = 0   Writing is success
 *   < 0   Error code
 ******************************************************************************/
int _tlu_write(tlu_handle_t tlu, int table, int index, int data_size,
		void *data)
{
	int scale, offset;

	scale = tlu_get_table_index_scale(tlu, table);
	offset = (index & ((1 << scale) - 1)) * TLU_UNIT_SIZE;
	index = index >> scale;

	return _tlu_write_data(tlu, table, index, offset, data_size, data);
}

/*******************************************************************************
 * Description:
 *   This function writes data into TLU table memroy at any byte address.
 * Parameters:
 *   tlu       - The TLU whose memory will be read
 *   table     - The table to which the memory belongs
 *   addr      - The byte address of the memory to be written. It relatives to
 *               the table base address.
 *   data_size - Total number of bytes to be written
 *   data      - A pointer to the data to be written
 * Return:
 *   = 0  Writing is success
 *   < 0   Error code
 ******************************************************************************/
int _tlu_write_byte(tlu_handle_t tlu, int table, uint32_t addr, int data_size,
		void *data)
{
	uint32_t cstat;

	tlu_send_cmd_write_byte(tlu, table, addr, data_size, data);

	cstat = _tlu_ready(tlu);
	if (cstat & TLU_CSTAT_FAIL) {
		TLU_ACC_CRIT("Write failed. cstat = %08x tlu = %x table = %d "
				"addr = %x size = %x\n",
				cstat, tlu, table, addr, data_size);
		return TLU_MEM_ERROR;
	}
	return 0;
}

/*******************************************************************************
 * Description:
 *   This function reads data up to 32 bytes from TLU table memory using data
 *   entry index.
 * Parameters:
 *   tlu      - The TLU whose memory will be read
 *   table    - The table to which the memory belongs
 *   index    - The index of data entry relative to the table base where data
 *              to be read
 *   offset   - The byte offset within the data entry where data to be read
 *   data_size - Total number of bytes requested. It must be multiple of 8 and
 *               less than or equal to 32.
 *   data     - A pointer of memory to store read data if not NULL
 * Return:
 *   The parameter <data> is return if it is not NULL and reading is success.
 *   The TLU data memory register address is returned is <data> is NULL and
 *   reading is success. NULL is return if memory reading error.
 ******************************************************************************/
void *__tlu_read_data(tlu_handle_t tlu, int table, int index, int offset,
		int data_size, void *data)
{
	uint32_t cstat;

	data_size = (data_size + TLU_UNIT_SIZE - 1) & (~(TLU_UNIT_SIZE - 1));
	tlu_send_cmd_read(tlu, table, index, offset, data_size);
	cstat = _tlu_ready(tlu);
	if (cstat & TLU_CSTAT_FAIL) {
		TLU_ACC_CRIT("Read failed. cstat = %08x tlu = %x table = %d "
				"index = %x size = %x Cmd = %08x %08x "
				"IEVENT = %08x\n",
				cstat, tlu, table, index, data_size,
				TLU_CMD_PTR(tlu)[0], TLU_CMD_PTR(tlu)[1],
				tlu_read_reg(tlu, TLU_IEVENT));
		return NULL;
	}

	TLU_ACC_LOG_MEM_RD((void *)TLU_KDATA_PTR(tlu), data_size, NULL);
	if (data) {
		tlu_memcpy32(data, (void *)TLU_KDATA_PTR(tlu), data_size);
		return data;
	} else {
		return (void *)TLU_KDATA_PTR(tlu);
	}
}

/*******************************************************************************
 * Description:
 *   This function reads data up to 64 bytes from TLU table memory using data
 *   entry index.
 * Parameters:
 *   tlu       - The TLU whose memory will be read
 *   table     - The table to which the memory belongs
 *   index     - The index of data entry relative to the table base where data
 *               to be read
 *   offset    - The byte offset within the data entry where data to be read
 *   data_size - Total number of bytes requested. It must be multiple of 8 and
 *               not exceed 64.
 *   data      - A pointer of memory to store read data. It must not be NULL.
 * Return:
 *   The parameter <data> is return if it is not NULL and reading is success.
 *   The TLU data memory register address is returned is <data> is NULL and
 *   reading is success.  NULL is return if memory reading error.
 ******************************************************************************/
void *_tlu_read_data(tlu_handle_t tlu, int table, int index, int offset,
		int data_size, void *data)
{
	if (data_size <= TLU_MAX_RW_LEN) {
		return __tlu_read_data(tlu, table, index, offset, data_size,
				data);
	} else {
		if (data == NULL)
			return NULL;

		if ((__tlu_read_data(tlu, table, index, offset,
					TLU_MAX_RW_LEN, data)) == NULL) {
			return NULL;
		}
		return __tlu_read_data(tlu, table, index, TLU_MAX_RW_LEN,
			data_size - TLU_MAX_RW_LEN,
			((uint8_t *)data) + TLU_MAX_RW_LEN);
	}
}

/*******************************************************************************
 * Description:
 *   This function reads data from TLU table memory using double word index.
 * Parameters:
 *   tlu       - The TLU whose memory will be read
 *   table     - The table to which the memory belongs
 *   index     - The double word index of the memory relative to the table base.
 *   data_size - Total number of bytes requested.
 *   data      - A pointer of memory to store read data if not NULL.
 * Return:
 *   The parameter <data> is return if it is not NULL and reading is success.
 *   The TLU data memory register address is returned is <data> is NULL and
 *   reading is success.  NULL is return if memory reading error.
 ******************************************************************************/
void *_tlu_read(tlu_handle_t tlu, int table, int index, int data_size,
		void *data)
{
	int scale, offset;

	scale = tlu_get_table_index_scale(tlu, table);
	offset = (index & ((1 << scale) - 1)) * TLU_UNIT_SIZE;
	index = index >> scale;
	return _tlu_read_data(tlu, table, index, offset, data_size, data);
}

/*******************************************************************************
 * Description:
 *   This function adds 16 bit value on a 32-bit data in a table memory.
 * Parameters:
 *   tlu   - The TLU handle
 *   table - The table index
 *   addr  - The byte addr where the value will be added on. It must be 32 bit
 *           aligned.
 *   data  - 16 bit value to be added
 * Return:
 *   = 0   Success
 *   < 0   Failure
 *         TLU_MEM_ERROR  A memory error detected
 ******************************************************************************/
int _tlu_add(tlu_handle_t tlu, int table, int addr, uint16_t data)
{
	uint32_t cstat;
	int scale, offset, index;

	index = addr >> 2;
	scale = tlu_get_table_index_scale(tlu, table);
	offset = ((index >> 1) & ((1 << scale) - 1)) * TLU_UNIT_SIZE;

	tlu_send_cmd_add(tlu, table, index >> (scale + 1), offset,
			0xF0 >> ((index & 1) * 4), data);
	cstat = _tlu_ready(tlu);
	if (cstat & TLU_CSTAT_FAIL) {
		TLU_ACC_CRIT("Add failed. cstat = %08x tlu = %x table = %d "
				"index = %x\n",
				cstat, tlu, table, index);
		return TLU_MEM_ERROR;
	}
	return 0;
}

/*******************************************************************************
 * Description:
 *   This function performs accumulated hash and wait for done.
 * Parameters:
 *   tlu     - The TLU handle
 *   table   - The table index
 *   key     - A pointer to the key
 *   key_len - The total number of bytes of the key
 * Return:
 *   = 0  Success
 *   < 0  Failure
 *        TLU_MEM_ERROR  A memory error detected
 ******************************************************************************/
int _tlu_acchash(tlu_handle_t tlu, int table, void *key, int key_len)
{
	uint32_t cstat;

	tlu_send_cmd_acchash(tlu, table, key, key_len);
	cstat = _tlu_ready(tlu);
	if (cstat & TLU_CSTAT_FAIL) {
		TLU_ACC_CRIT("Acchash failed. cstat=%08x tlu=%x table=%d\n",
				cstat, tlu, table);
		return TLU_MEM_ERROR;
	}
	return 0;
}

/*******************************************************************************
 * Description:
 *   This function reads TLU statistics registers.
 * Parameters:
 *   tlu    - The TLU handle
 *   stats  - A pointer to the statistics data structure
 * Return:
 *  None
 ******************************************************************************/
void _tlu_get_stats(tlu_handle_t tlu, union tlu_stats *stats)
{
	stats->cramr = tlu_read_reg(tlu, TLU_CRAMR);
	stats->cramw = tlu_read_reg(tlu, TLU_CRAMW);
	stats->cfind = tlu_read_reg(tlu, TLU_CFIND);
	stats->cthtk = tlu_read_reg(tlu, TLU_CTHTK);
	stats->ctcrt = tlu_read_reg(tlu, TLU_CTCRT);
	stats->ctchs = tlu_read_reg(tlu, TLU_CTCRT);
	stats->ctdat = tlu_read_reg(tlu, TLU_CTDAT);
	stats->chits = tlu_read_reg(tlu, TLU_CHITS);
	stats->cmiss = tlu_read_reg(tlu, TLU_CMISS);
	stats->chcol = tlu_read_reg(tlu, TLU_CHCOL);
	stats->ccrtl = tlu_read_reg(tlu, TLU_CCRTL);
	stats->caro = tlu_read_reg(tlu, TLU_CARO);
	stats->carm = tlu_read_reg(tlu, TLU_CARM);
}

/*******************************************************************************
 * Description:
 *   This function resets (clears) TLU statistics registers.
 * Parameters:
 *   tlu    - The TLU handle
 * Return:
 *   None
 ******************************************************************************/
void _tlu_reset_stats(tlu_handle_t tlu)
{
	tlu_write_reg(tlu, TLU_CRAMR, 0);
	tlu_write_reg(tlu, TLU_CRAMW, 0);
	tlu_write_reg(tlu, TLU_CFIND, 0);
	tlu_write_reg(tlu, TLU_CTHTK, 0);
	tlu_write_reg(tlu, TLU_CTCRT, 0);
	tlu_write_reg(tlu, TLU_CTCRT, 0);
	tlu_write_reg(tlu, TLU_CTDAT, 0);
	tlu_write_reg(tlu, TLU_CHITS, 0);
	tlu_write_reg(tlu, TLU_CMISS, 0);
	tlu_write_reg(tlu, TLU_CHCOL, 0);
	tlu_write_reg(tlu, TLU_CCRTL, 0);
	tlu_write_reg(tlu, TLU_CARO, 0);
	tlu_write_reg(tlu, TLU_CARM, 0);
}

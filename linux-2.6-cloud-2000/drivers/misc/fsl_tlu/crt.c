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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE US:E OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains an implementation of Hash-Trie_Key (HTK) table management
 * functions.
 */
#include <asm/crt.h>

#define MODIFY_ALL 	-1
#define MODIFY_SINGLE 	-2

/* IPTD tables may be split thus smaller than min_iptd_bits may be
 * generated if it is defined
 */
/*#define CRT_SPLIT_ENABLE*/

struct crt_trace {
	int32_t base;
	int16_t index;
	int16_t bits;
	uint32_t entropy;
};

static inline int32_t crt_alloc_data_entry(struct crt *crt)
{
	int index;

	crt->free_data_count--;
	index = tlu_heap_alloc_data(crt->tlu, crt->table, &crt->data_heap);
	CRT_LOG("%s[%d]: %x\n", __func__, crt->free_data_count, index);
	return index;
}

static inline int32_t crt_free_data_entry(struct crt *crt, int32_t entry)
{
	crt->free_data_count++;
	CRT_LOG("%s[%d]: %x\n", __func__, crt->free_data_count, entry);
	return tlu_heap_free_data(crt->tlu, crt->table, &crt->data_heap, entry);
}

/* read/write functions for various objectsi. The unit of index is one double
 *   word.
 */
static inline void *crt_read(struct crt *crt, int index, int len, void *entry)
{
	return _tlu_read(crt->tlu, crt->table, index, len, entry);
}

static inline int crt_write(struct crt *crt, int index, void *data, int len)
{
	return _tlu_write(crt->tlu, crt->table, index, len, data);
}

/* read/write functions for data entries. The unit of index is one data entry.*/
static inline void *crt_read_data(struct crt *crt, int index, int len,
		void *entry)
{
	return _tlu_read_data(crt->tlu, crt->table, index, 0, len, entry);
}

static inline int crt_write_data(struct crt *crt, int index, void *data,
		int len)
{
	return _tlu_write_data(crt->tlu, crt->table, index, 0, len, data);
}

/* read/write an IPTD entry */
static inline iptd_entry_t *crt_read_iptd_entry(struct crt *crt, int index)
{
	return (iptd_entry_t *)crt_read(crt, index, sizeof(iptd_entry_t), NULL);
}

static inline int crt_write_iptd_entry(struct crt *crt, int index,
	iptd_entry_t *entry)
{
	return crt_write(crt, index, entry, sizeof(iptd_entry_t));
}

static inline iptd_entry_t *crt_get_iptd_entry(struct crt *crt, int index,
	iptd_entry_t *entry)
{
	return (iptd_entry_t *)
		crt_read(crt, index, sizeof(iptd_entry_t), entry);
}

/* Fill a data IPTD entry. The key size is stored in the entropy field which is
 * not used by hw
 */
static inline void crt_fill_iptd_data_entry(iptd_entry_t *entry,
	int data_index, int key_bits)
{
	iptd_fill_entry(entry, key_bits, data_index, 0, IPTD_ETYPE_DATA);
}

/* Key size of a data entry is stored in the entropy field */
static inline int crt_get_key_size(iptd_entry_t *entry)
{
	return iptd_get_entropy(entry);
}

/*******************************************************************************
 * Description:
 *   Check whether the IPTD table should be compressed
 * Parameters:
 *   compress   - Compression required
 *   bits       - The table size in bits
 * Return:
 *   A none zero value indicates the table should be compressed. Otherwise a 0
 *   value indicates the table should not be compressed.
 ******************************************************************************/
static inline int crt_iptd_compress(int compress, int bits)
{
	return compress && bits >= RUNDELTA_MIN_BITS;
}

/*******************************************************************************
 * Description:
 *   Get part of a key.
 * Parameters:
 *    key	 - 32 bit key value
 *    bit_offset - The offset relative to the most significant bit of the key.
 *    bits	 - Total number of bits to get
 * Return:
 *   The value get from the key. It is aligned to the least significant bit.
 ******************************************************************************/
static inline int crt_get_key_prefix32(uint32_t key, int bit_offset, int bits)
{
	return (key >> (32 - (bit_offset + bits))) & ((1 << bits) - 1);
}

/*******************************************************************************
 * Description:
 *   Get part of a key which is any size.
 * Parameters:
 *   key        - The pointer to the key
 *   bit_offset - The offset relative to the most significant bit of the key.
 *   bits       - Total number of bits to get
 * Return:
 *   The value get from the key. It is aligned to the least significant bit.
 ******************************************************************************/
static inline int crt_get_key_prefix(void *key, int bit_offset, int bits)
{
	uint32_t key32;
	int byte_offset;

	byte_offset = bit_offset / 8;
	bit_offset %= 8;
	memcpy(&key32, key + byte_offset, (bit_offset + bits + 7) / 8);
	return crt_get_key_prefix32(key32, bit_offset, bits);
}

/*******************************************************************************
 * Description:
 *   Get the initial table size of a CRT table
 * Parameters:
 *   crt   - A pointer to the CRT table
 * Return:
 * The initital table size in bits
 ******************************************************************************/
static inline int crt_get_init_table_bits(struct crt *crt)
{
	return ((tlu_read_reg(crt->tlu, TLU_PTBL0 + crt->table * 4)
			>> TLU_TABLE_CFG_KEYSHL_SHIFT)
			& TLU_TABLE_CFG_KEYSHL_MASK) + 1;
}

/*******************************************************************************
 * Description:
 *   Set the initial table size of a CRT table
 * Parameters:
 *   crt   - A pointer to the CRT table
 *   bits  - The initital table size in bits
 * Return:
 *   None
 ******************************************************************************/
static inline void crt_set_init_table_bits(struct crt *crt, int bits)
{
	uint32_t cfg;

	cfg = tlu_read_reg(crt->tlu, TLU_PTBL0 + crt->table * 4);
	cfg = (cfg & (~(TLU_TABLE_CFG_KEYSHL_MASK
					<< TLU_TABLE_CFG_KEYSHL_SHIFT)))
			| ((bits - 1) << TLU_TABLE_CFG_KEYSHL_SHIFT);
	tlu_write_reg(crt->tlu, TLU_PTBL0 + crt->table * 4, cfg);
}

static inline int crt_is_empty(struct crt *crt)
{
	return crt->empty;
}

/* Convert data index to an index in the data information table */
static inline int crt_data_info_index(struct crt *crt, int data_index)
{
#ifdef TLU_FIXED_INDEX_SCALE
	return data_index/(crt->kdata_size/TLU_UNIT_SIZE);
#else
	return data_index - crt->data_start;
#endif
}

/* Get the data information entry of a data entry */
static inline struct crt_data_info *crt_get_data_info(struct crt *crt,
		int data_index)
{
	return crt->data_info + crt_data_info_index(crt, data_index);
}

/* Get data reference count of a data entry */
static inline int _crt_get_data_ref_count(struct crt *crt, int data_index)
{
	return crt_get_data_info(crt, data_index)->ref_count;
}

/* Get data reference count of a data entry with debugging check and log */
static inline int crt_get_data_ref_count(struct crt *crt, int data_index)
{
	int count;

	count = _crt_get_data_ref_count(crt, data_index);
	if (count < 0) {
		CRT_CRIT("Internal error: negative data reference counter"
			" (data_index = %x count = %d)\n", data_index, count);
	} else {
		CRT_LOG("get: data_index = %x count = %d\n", data_index, count);
	}
	return count;
}

/* Increase the reference count if a data entry */
static inline void crt_inc_data_ref_count(struct crt *crt, int data_index,
		int count)
{
	CRT_LOG("inc: data_index = %x inc = %d  count = %d\n",
			data_index, count,
			_crt_get_data_ref_count(crt, data_index));
	crt_get_data_info(crt, data_index)->ref_count += count;
}

/* Decrease the reference count if a data entry */
static inline void crt_dec_data_ref_count(struct crt *crt, int data_index,
		int count)
{
	int ref;

	ref = _crt_get_data_ref_count(crt, data_index);
	CRT_LOG("dec: data_index = %x dec = %d count = %d\n",
			data_index, count, ref);
	if (ref < count) {
		CRT_LOG("WARN: Broken data reference counter: %x: %d - %d\n",
				data_index, ref, count);
	}

	crt_get_data_info(crt, data_index)->ref_count -= count;
}

/* Store data information */
static void crt_set_data_info(struct crt *crt, int index, int count,
		uint8_t *key, int key_bits)
{
	struct crt_data_info *info;

	info = crt_get_data_info(crt, index);
	CRT_LOG("set: data_index = %x count = %d\n", index, count);
	info->ref_count = count;
#ifdef CRT_STORE_KEY
	info->key_size = key_bits;
	memset(info->key, 0, TLU_MAX_KEY_BYTES);
	memcpy(info->key, key, (key_bits + 7) / 8);
#endif
}

/* Initialize the data information table */
static void crt_data_info_init(struct crt *crt)
{
#ifdef CRT_STORE_KEY
	int i;

	for (i = 0; i < crt->data_entry_num; i++)
		crt->data_info[i].key_size = -1;

#endif
}

/*******************************************************************************
 * Description:
 *   Initialize the CRT memory which is used for IPTD tables. The memory will be
 *   split to a fixed block size.
 * Parameters:
 *   crt        - A pointer to the CRT table
 *   mem_start  - The start address of usable memory.It is relative to the table
 *	          base.
 *   size       - The size of the memory in bits
 * Return:
 *   >= 0  The heap handle
 *   < 0  Error code which is the following:
 *        TLU_HEAP_FAIL  TLU memory error detected
 ******************************************************************************/
static int crt_mem_init(struct crt *crt, uint32_t mem_start, int size)
{
	crt->mem_heap = tlu_heap_create(crt->tlu, crt->table,
			TLU_ADDR_TO_INDEX(mem_start),
			size/CRT_MEM_BLOCK_SIZE,
			CRT_MEM_BLOCK_SIZE / TLU_UNIT_SIZE);
	crt->free_mem_size = size;
	return crt->mem_heap;
}

/*******************************************************************************
 * Description:
 *   Allocate a memory block
 * Parameters:
 *   crt   - A pointer to the CRT table
 *   size  - The size to be allocated. It must be less than CRT_MEM_BLOCK_SIZE
 * Return:
 *   >= 0  The index of the allocated memory if success.It is the memory address
 * 	   divided by 8.
 *   <  0  Error
 ******************************************************************************/
static int crt_mem_alloc(struct crt *crt, int size)
{
	int index;

	if (size > CRT_MEM_BLOCK_SIZE) {
		CRT_CRIT("Required size %x is too large\n", size);
		return -1;
	}

	crt->free_mem_size -= CRT_MEM_BLOCK_SIZE;
	index = tlu_heap_alloc(crt->tlu, crt->table, &crt->mem_heap);
	CRT_LOG("MALLOC %x %x\n", index, size);
	return index;
}

/*******************************************************************************
 * Description:
 *   Free a memory block
 * Parameters:
 *   crt   - A pointer to the CRT table
 *   index - The index of the memory block
 *   size  - The size of the block. It is not used and reserved for future fancy
 *	     allocation algorithm.
 * Return:
 *   >= 0  Success
 *   <0   Error code listed below:
 *	  TLU_HEAP_FAIL Memory error detected
 ******************************************************************************/
static int crt_mem_free(struct crt *crt, int index, int size)
{
	crt->free_mem_size += CRT_MEM_BLOCK_SIZE;
	CRT_LOG("MFREE %x %x\n", index, size);
	return tlu_heap_free(crt->tlu, crt->table, &crt->mem_heap, index);
}

/*******************************************************************************
 * Description:
 *    Free an IPTD table
 * Parameters:
 *    crt    - A pointer to the CRT table
 *    base   - The base index of the IPTD table
 *    size   - The size of the IPTD table
 * Return:
 *    See crt_mem_free.
 ******************************************************************************/
static int crt_free_iptd(struct crt *crt, int base, int size)
{
	if (base == 0)
		crt->empty = 1;

	return crt_mem_free(crt, base, size);
}

/* Free an IPTD table by giving an IPTD entry which is pointing to the table */
static int crt_free_iptd_by_entry(struct crt *crt, iptd_entry_t *entry)
{
	return crt_free_iptd(crt, iptd_get_base(entry),
			rundelta_get_size(iptd_get_entropy(entry),
				iptd_get_bits(entry)));
}

/*******************************************************************************
 * Description:
 *   Allocate and fill an IPTD table
 * Parameters:
 *   crt     - A pointer to the CRT table
 *   bit     - The table size in bits
 *   entry   - The entry to be filled in the whole allocated table if it is not
 *	       NULL. Otherwise FAIL entries will be filled in the table.
 * compress  - Indicates whether the table should be compressed if possible.
 * Return:
 *   See crt_mem_alloc.
 ******************************************************************************/
static int crt_alloc_iptd(struct crt *crt, int bits, iptd_entry_t *entry,
		int compress)
{
	int start_index, size, entry_num;
	int rc;
	int i;
	iptd_entry_t tmp_entry;

	CRT_LOG("Alloc IPTD: bits = %d compress = %d\n", bits, compress);
	if (crt_iptd_compress(compress, bits)) {
		entry_num = 1;
		size = CRT_MIN_COMPRESSED_IPTD_SIZE * sizeof(iptd_entry_t);
	} else {
		entry_num = 1 << bits;
		size = entry_num * sizeof(iptd_entry_t);
	}
	start_index = crt_mem_alloc(crt, size);
	if (start_index < 0) {
		CRT_LOG("Allocate IPTD failed: out of memory\n");
		return TLU_OUT_OF_MEM;
	}
	if (entry == NULL) {
		entry = &tmp_entry;
		iptd_fill_entry(entry, 0, 0, 0, IPTD_ETYPE_FAIL);
	}
	for (i = 0; i < entry_num; i++) {
		rc = crt_write_iptd_entry(crt, start_index + i, entry);
		if (rc < 0)
			return rc;

	}

	if (entry && iptd_get_etype(entry) == IPTD_ETYPE_DATA)
		crt_inc_data_ref_count(crt, iptd_get_base(entry), entry_num);

	return start_index;
}

/*******************************************************************************
 * Description:
 *   Create an IPTD table in TLU from a given table in processor memory.
 *   Data reference counters are increased accordingly.
 * Parameters:
 *   crt    - A pointer to the CRT table
 *   iptd   - The table content
 *   size   - Total entry number of the table
 * Return:
 *   >= 0  The based index of the IPTD table
 *   <0   Error
 ******************************************************************************/
static int crt_create_iptd(struct crt *crt, iptd_entry_t *iptd, int size)
{
	int base, i, rc;

	CRT_LOG("Create IPTD: %d\n", size);
	base = crt_mem_alloc(crt, size * sizeof(iptd_entry_t));
	if (base < 0) {
		CRT_LOG("Create IPTD failed: out of memory\n");
		return TLU_OUT_OF_MEM;
	}

	for (i = 0; i < size; i++) {
		if (iptd[i].etype == IPTD_ETYPE_DATA)
			crt_inc_data_ref_count(crt, iptd[i].base, 1);

		rc = crt_write_iptd_entry(crt, base + i, iptd + i);
		if (rc < 0)
			return rc;

	}
	return base;
}

/*******************************************************************************
 * Description:
 *   Destroy a data entry. The data will not be deleted if its reference counter
 *   is not 0.
 * Parameters:
 *   crt        - A pointer to the CRT table
 *   data_index - Index of the data entry
 * Return:
 *   >= 0  Success
 *   <0   Error
 ******************************************************************************/
static int crt_destroy_data(struct crt *crt, int data_index)
{
	CRT_LOG("%s: index = %x  count = %d\n", __func__, data_index,
			_crt_get_data_ref_count(crt, data_index));

	if (_crt_get_data_ref_count(crt, data_index) == 0) {
#ifdef CRT_STORE_KEY
		crt_get_data_info(crt, data_index)->key_size = -1;
#endif
		return crt_free_data_entry(crt, data_index);
	}
	return 0;
}

/*******************************************************************************
 * Description:
 *   Create a data entry.
 * Parameters:
 *   crt 	   - A pointer to the CRT table
 *   data	   - The content of the data
 *   key	   - Key of the data entry
 *   key_bits - Key size in bits
 * Return:
 *   >= 0  Data index
 *   <0   Error
 ******************************************************************************/
static int crt_create_data(struct crt *crt, void *data, void *key, int key_bits)
{
	int data_index, err;

	CRT_LOG("%s\n", __func__);
	/* Alloc data entry */
	data_index = crt_alloc_data_entry(crt);
	if (data_index < 0) {
		CRT_LOG("Table full\n");
		return TLU_TABLE_FULL;
	}
	/* Write data */
	err = crt_write_data(crt, data_index, data, crt->kdata_size);
	if (err < 0) {
		crt_destroy_data(crt, data_index);
		return err;
	}
	/* Set data info */
	crt_set_data_info(crt, data_index, 0, key, key_bits);
	return data_index;
}

/*******************************************************************************
 * Description:
 *   Check whether an IPTD table is redundant.
 *   An IPTD table is redundant if it meets one of the following conditions:
 *	1. All entries have type IPTD_ETYPE_FAIL
 *	2. All entries have type IPTD_ETYPE_DATA and key bits is less than
 *	   bit_offset, and key bits and base of all entries are same
 * Parameters:
 *   crt        - A pointer to the CRT table
 *   iptd       - The IPTD table is in memory if it is not NULL.
 *   base       - The base index of the IPTD table if it is not in memory
 *   size       - Total entry number of the table.
 *   bit_offset - The bit offset of the table's partial key in the full key.
 *   entry_1st  - To return the 1st entry of the table if it is not NULL.
 * Return:
 *   None zero indicates it is a redundant table. Otherwise 0 indicate it is
 *   not.
 ******************************************************************************/
static int crt_iptd_redundant(struct crt *crt, iptd_entry_t *iptd, int base,
		int size, int bit_offset, iptd_entry_t *entry_1st)
{
	int i;
	int type, key_bits;
	uint32_t data_base;
	iptd_entry_t *entry;

	CRT_LOG("%s base = %x size = %d bit_offset = %d\n",
			__func__, base, size, bit_offset);

	/* Get the 1st entry */
	if (iptd)
		entry = iptd;
	else {
		entry = crt_read_iptd_entry(crt, base);
		if (entry == NULL)
			return TLU_MEM_ERROR;

	}
	type = iptd_get_etype(entry);
	if (entry_1st)
		tlu_memcpy32(entry_1st, entry, sizeof(iptd_entry_t));

	CRT_LOG("type = %d  key_size = %d  entry = %08x %08x\n",
			type, crt_get_key_size(entry), entry->words[0],
			entry->words[1]);

	/* Avoid compiler warning */
	key_bits = 0;
	data_base = 0;
	/* Check the 1st entry against the conditions */
	if (type == IPTD_ETYPE_DATA) {
		data_base = iptd_get_base(entry);
		/* '>' really never happens because it is the terminal entry */
		key_bits = crt_get_key_size(entry);
		if (key_bits >= bit_offset)
			return 0;

	} else if (type != IPTD_ETYPE_FAIL) {
		return 0;
	}
	/* Check all other entries against the 1st entry */
	for (i = 1; i < size; i++) {
		if (iptd)
			entry = iptd + i;
		else {
			entry = crt_read_iptd_entry(crt, base + i);
			if (entry == NULL)
				return TLU_MEM_ERROR;

		}
		if (type != iptd_get_etype(entry)
			|| (type != IPTD_ETYPE_FAIL
				&& !(type == IPTD_ETYPE_DATA
					&& iptd_get_base(entry) == data_base
					&& crt_get_key_size(entry)
					== key_bits))) {
			return 0;
		}
	}
	return 1;
}

/* Convert a key (uncompressed index) of an entry into an index in the
 * rundelta table
 */
static int crt_get_rundelta_index(uint32_t entropy, int bits, int key)
{
	int block_num;

	if (entropy) {
		block_num = key >> (bits-4);
		return _rundelta_get_size(entropy, block_num, bits)
			+ rundelta_get_index(entropy, block_num, bits, key);
	} else {
		return key;
	}
}

/* Full an IPTD table with the same entry */
static void crt_fill_iptd_table(iptd_entry_t *iptd, int size,
		iptd_entry_t *entry)
{
	int i;

	for (i = 0; i < size; i++)
		tlu_memcpy32(iptd + i, entry, sizeof(iptd_entry_t));

}

/*******************************************************************************
 * Description:
 *   Read an IPTD table from TLU into memory. Reference counters will be changed
 *   accordingly.
 * Parameters:
 *   cr	     - A pointer to the CRT table
 *   iptd    - Table space in the memory.
 *   index   - The start index of entries to be read
 *   size    - Total entry number to read.
 *   reverse - Indicates just reversing (increasing) reference counters
 *	       instead of loading the table if it is not 0. Otherwise table
 *	       will be loaded and reference counters of entries will be
 *	       decreased.
 * Return:
 *   0 if success. Otherwise a negative value indicates an error.
 ******************************************************************************/
static int crt_read_iptd_table(struct crt *crt, iptd_entry_t *iptd, int index,
		int size, int reverse)
{
	int i;
	iptd_entry_t *entry;

	for (i = 0; i < size; i++) {
		entry = crt_read_iptd_entry(crt, index++);
		if (entry == NULL)
			return TLU_MEM_ERROR;

		if (!reverse) {
			tlu_memcpy32(iptd + i, entry, sizeof(iptd_entry_t));
			if (iptd[i].etype == IPTD_ETYPE_DATA)
				crt_dec_data_ref_count(crt, iptd[i].base, 1);

		} else {
			/* Reverse operation */
			if (iptd_get_etype(entry) == IPTD_ETYPE_DATA) {
				crt_inc_data_ref_count(crt,
						iptd_get_base(entry), 1);
			}
		}
	}
	return 0;
}

/*******************************************************************************
 * Description:
 *   Decompress an IPTD table from TLU into memory. Reference counters will be
 *   changed accordingly
 * Parameters:
 *   crt      - A pointer to the CRT table
 *   base     - The base index of the IPTD table
 *   entropy  - The entropy of the table
 *   bit      - The size of the table in bits
 *   iptd     - Table space in the memory
 *   reverse  - Indicates reversing (increasing) reference counters if it is
 *              not 0. Otherwise reference counters if entries will be
 *              decreased.
 * Return:
 *   0 is returned if success. Otherwise a negative value indicates an error.
 ******************************************************************************/
static int crt_decompress_iptd(struct crt *crt, int base, uint32_t entropy,
		int bits, iptd_entry_t *iptd, int reverse)
{
	int i, index, block_size, rc;

	CRT_LOG("%s base = %x entropy = %08x bits = %d\n",
			__func__, base, entropy, bits);
	index = 0;
	block_size = 1 << (bits - 4);
	for (i = 0; i < RUNDELTA_BLOCK_NUM; i++) {
		switch (entropy_get(entropy, 30 - i * 2)) {
		case ENTROPY_NONE:
			crt_fill_iptd_table(iptd, block_size, iptd - 1);
			break;
		case ENTROPY_SINGLE:
			rc = crt_read_iptd_table(crt, iptd, base + index,
							1, reverse);
			if (rc < 0)
				return rc;

			/* Copy the rest of entries */
			crt_fill_iptd_table(iptd + 1, block_size - 1, iptd);
			index++;
			break;
		case ENTROPY_HALF:
			crt_fill_iptd_table(iptd, block_size / 2, iptd - 1);
			rc = crt_read_iptd_table(crt,
							iptd + block_size / 2,
							base + index,
							block_size / 2,
							reverse);
			if (rc < 0)
				return rc;

			index += block_size / 2;
			break;
		case ENTROPY_FULL:
			rc = crt_read_iptd_table(crt, iptd, base + index,
							block_size,
							reverse);
			if (rc < 0)
				return rc;

			index += block_size;
			break;
		}
		iptd += block_size;
	}
	return 0;
}

/* Check whether two IPTD entries are match */
static inline int iptd_entry_match(iptd_entry_t *iptd1, iptd_entry_t *iptd2)
{
	return iptd1->words[0] == iptd2->words[0]
			&& iptd1->words[1] == iptd2->words[1];
}

/* Count contiguous exact same entries */
static int crt_iptd_count_entry(iptd_entry_t *iptd, int max)
{
	int i, type;

	type = iptd[0].etype;
	i = 1;
	while (i < max && ((type == IPTD_ETYPE_FAIL && iptd[i].etype == type)
			|| (type == IPTD_ETYPE_DATA
				&& iptd_entry_match(iptd, iptd + i)))) {
		i++;
	}
	return i;
}

/*******************************************************************************
 * Description:
 *   Compress an IPTD table.
 * Parameters:
 *   iptd1	- The original IPTD table
 *   iptd2	- To store the compressed table
 *   new_size   - Total entry number of the compressed table
 * Return:
 *  0 is returned if success. Otherwise a negative value indicates an error.
 ******************************************************************************/
static int crt_compress_iptd(iptd_entry_t *iptd1, int bits,
		iptd_entry_t *iptd2, int *new_size)
{
	int size, count, i, block_size;
	int code;
	uint32_t entropy;

	CRT_LOG("%s bits = %d\n", __func__, bits);
	block_size = 1 << (bits - 4);

	size = 0;
	entropy = 0;
	for (i = 0; i < RUNDELTA_BLOCK_NUM; i++) {
		count = crt_iptd_count_entry(iptd1, block_size);
		if (count == block_size) {
			if (i != 0 && iptd_entry_match(iptd1, iptd1 - 1))
				code = ENTROPY_NONE;
			else {
				code = ENTROPY_SINGLE;
				tlu_memcpy32(iptd2 + size, iptd1,
						sizeof(iptd_entry_t));
				size++;
			}
		} else if (count >= block_size / 2 && i != 0
				&& iptd_entry_match(iptd1, iptd1 - 1)) {
			code = ENTROPY_HALF;
			tlu_memcpy32(iptd2 + size, iptd1 + block_size / 2,
					sizeof(iptd_entry_t) * block_size / 2);
			size += block_size / 2;
		} else {
			code = ENTROPY_FULL;
			tlu_memcpy32(iptd2 + size, iptd1,
					sizeof(iptd_entry_t) * block_size);
			size += block_size;
		}
		entropy = (entropy << 2) | code;
		iptd1 += block_size;
	}
	*new_size =  size;
	return entropy;
}

/*******************************************************************************
 * Description:
 *  Load an IPTD or rundelta table into memory. Reference counters will be
 *  changed accordingly.
 * Parameters:
 *   crt     - A pointer to the CRT table
 *   iptd    - Table space in the memory
 *   base    - The base index of the IPTD table
 *   entropy - The entropy of the table
 *   bit     - The size of the table in bits
 *   reverse - Indicates reversing (increasing) reference counters if it is not
 *	       0. Otherwise reference counters if entries will be decreased.
 * Return:
 *   0 is return if success. Otherwise a negative value indicates an error.
 ******************************************************************************/
static int crt_load_iptd_table(struct crt *crt, iptd_entry_t *iptd, int base,
		int bits, uint32_t entropy, int reverse)
{
	if (entropy) {
		return crt_decompress_iptd(crt, base, entropy, bits,
				iptd, reverse);
	} else {
		return crt_read_iptd_table(crt, iptd, base, 1 << bits,
				reverse);
	}
}

/*******************************************************************************
 * Description:
 *   This function is to replace an IPTD entry by a given one. Then free the
 *   previously pointed IPTD table.
 * Parameters:
 *   crt    - Pointing to the CRT table
 *   index  - The index of an IPTD entry, if exists, which points to this IPTD
 *	      table specified by <base>. It is a global index within the CRT
 * 	      table. The switched
 *   base   - The base index of the current IPTD table.
 *   size   - The total entry number of the current IPTD table.
 *   entry  - The new IPTD entry.
 * Return:
 * The new based index is returned if success. Otherwise a negative value, an
 * error code, is returned.
*******************************************************************************/
static int _crt_switch_iptd(struct crt *crt, int index, int base, int size,
		iptd_entry_t *entry)
{
	int rc;

	CRT_LOG("index = %x base = %x size = %d\n", index, base, size);
	/* Updata the link */
	if (base != 0) {
		CRT_LOG("[%x] %08x %08x\n", index, entry->words[0],
				entry->words[1]);
		rc = crt_write_iptd_entry(crt, index, entry);
		if (rc < 0)
			return rc;

	}
	rc = crt_free_iptd(crt, base, size);
	if (rc < 0)
		return rc;

	return iptd_get_base(entry);
}

/*******************************************************************************
 * Description:
  This function is to switch an IPTD table to a new one given by <new_base>.
  The current IPTD table which is referenced by <base> will be deleted. If the
  current IPTD table is the top level table, that is <base> is 0, the
  parameters for the new table are ignored and not table switch is performed.
 * Parameters:
  crt	   - Pointing to the CRT table
  index	   - The index of an IPTD entry, if exists, which points to this IPTD
	     table specified by <base>. It is a global index within the CRT
	     table. The switched
  base	   - The base index of the current IPTD table.
  size	   - The total entry number of the current IPTD table.
  new_base - The base index of the new IPTD table.
	     new_base, new_bits, new_entropy and new_type will be ignored if
	     base equals to 0.
  new_bits - The bit size of the new IPTD table. It is forced to 0 if new_type
	     is IPTD_ETYPE_FAIL.
  new_entropy  - The entropy of the new IPTD table.
  new_type - The type of the new IPTD table.
 * Return:
  The new based index is returned if success. Otherwise a negative value, an
  error code, is returned.
  Reference counters of data in the both table are not touched in this function.
*******************************************************************************/
int crt_switch_iptd(struct crt *crt, int index, int base, int size,
		int new_base, int new_bits, uint32_t new_entropy, int new_type)
{
	iptd_entry_t entry;

	CRT_LOG("%s: index=%x base=%x size=%d new_base=%x new_bits=%d"
			"new_entropy=%08x new_type=%d\n",
			__func__, index, base, size, new_base, new_bits,
			new_entropy, new_type);
	iptd_fill_entry(&entry, new_entropy, new_base,
			(new_type == IPTD_ETYPE_FAIL ? 0 : new_bits - 1),
			new_type);
	return _crt_switch_iptd(crt, index, base, size, &entry);
}

/*******************************************************************************
 * Description:
 *   This function evaluates whether an entry is qualified for modification.
 * Parameters:
 *   entry     - Pointing to the entry to be evaluated.
 *   cmp_entry - The All qualified entries will be replaced by this new_entry.
 *  	         It must be a data entry of <key> == -1 and <old_entry> == NULL
 *	         because key size is taken from it in this case.
 *   new_entry - All qualified entries will be replaced by this new_entry.
 *               It must be a data entry of <key> == -1 and <old_entry> == NULL
 *               because key size is taken from it in this case.
 * Return:
 *   The new base index of the modified IPTD table previously pointed by <base>.
 *   The original <base> is returned if the table is not re-allocated.
 ******************************************************************************/
static int crt_modify_qualified(iptd_entry_t *entry, iptd_entry_t *cmp_entry,
		iptd_entry_t *new_entry)
{
	int type;

	if (cmp_entry == NULL) {
		type = iptd_get_etype(entry);
		return (type == IPTD_ETYPE_FAIL)
			|| (type == IPTD_ETYPE_DATA && crt_get_key_size(entry)
					< crt_get_key_size(new_entry));
	} else {
		return iptd_entry_match(cmp_entry, entry);
	}
}

/*******************************************************************************
 * Description:
 *   This function is to modify or replace one or more entries in the specifyed
 *   IPTD table.  The IPTD table is given by base index, entropy and bit size.
 *   This IPTD table may be compressed. It might be re-allocated after
 *   modification. As a result, the upper level IPTD, if exists, may be modified
 *   as well.
 *   The parameter <direct> is used to force modification on the original table
 *   even though it is comperssed. Thus the table will not be re-located, and
 *   TLU_OUT_OF_MEM error will not occur. However, it is the caller's
 *   responsibilty to make sure the current entropy is still correct after
 *   modification.
 * Parameters:
 *   crt      - Pointing to the CRT table
 *   base     - The base index of the IPTD table
 *   entropy  - The entropy of the IPTD table.
 *   bits     - The bit size of the IPTD table
 *   key      - >= 0: The key of the modifying entry within the IPTD table.
 *	          It equals to the index of the entry in an uncompressed table.
 *		  The next parameter <old_entry> is ignored.
 *	        = -1: Entries to be replaced are determined by the next
 *                parameter <old_entry>.
 *   key_end  - >= 0 The end of key range
 *	        <0  MODIFY_ALL indicates modify in the whole table
 *	 	    MODIFY_SINGLE indicates modify a single entry pointed by
 *                  'key'
 *   old_entry - Pointing to the content of the entry to be replaced if it is
 *               not NULL. Otherwise all entries with type IPTD_ETYPE_FAIL and
 *               data entries whose key bits is less then that of the new_entry
 *               will be replaced.
 *   new_entry - All qualified entries will be replaced by this new_entry.
 *               It must be a data entry of <key> == -1 and <old_entry> == NULL
 *               because key size is taken from it in this case.
 *   prev_link - The index of an IPTD entry which points to this IPTD table
 * 	         specified by <base>. It is a global index within the CRT table.
 *   compress    - Any modified IPTD tables should be compressed if possible.
 *   new_entropy - A memory to store the new entropy of the modified IPTD table
 * 		   pointed by <base>. Upon error, the content of this memory
 *                 is not defined.
 *   direct      - Indicating the modification should directly performed on the
 *	           original table eventhou it is compressed.
 * Return:
 *   >= 0  The new base index of the modified IPTD table previously pointed by
 *        <base>. The original <base> is returned if the table is not
 *        re-allocated.
 *   < 0  Error code
 *        If it ios non-critical error such as TLU_TABLE_FULL, the table to be
 *        modified is not touched. However there is no guarantee if a critical
 *        error occurs,
 ******************************************************************************/
static int crt_modify_iptd(struct crt *crt, int base, uint32_t entropy,
		int bits, int key, int key_end, iptd_entry_t *old_entry,
		iptd_entry_t *new_entry, int prev_link, int compress,
		uint32_t *new_entropy, int direct)
{
	int rc, size, new_base, i;
	iptd_entry_t *iptd;
	iptd_entry_t *crt_iptd1, *crt_iptd2;
	int etype;
	uint32_t modified_entropy;

	/* Total 2 * (1 << CRT_MAX_IPTD_BITS) entries in the buf */
	crt_iptd1 = crt->tmp_iptd_buf;
	crt_iptd2 = crt->tmp_iptd_buf + (1 << CRT_MAX_IPTD_BITS);

	CRT_LOG("%s base = %x  bits = %d  entropy = %08x prev_link = %x\n",
			__func__, base, bits, entropy, prev_link);
	/* Debug check: the initial table must not be compressed. */
	if (base == 0 && entropy != 0) {
		CRT_CRIT("Initial IPTD error\n");
		return TLU_TABLE_ERROR;
	}

	if (key >= 0) {
		CRT_LOG("[%x]", key);
	} else if (old_entry) {
		CRT_LOG("<%08x %08x>", old_entry->words[0],
				old_entry->words[1]);
	} else {
		CRT_LOG("<FAIL>");
	}
	CRT_LOG(" = ><%08x %08x>\n", new_entry->words[0], new_entry->words[1]);

	/* base == 0 indicates the initial IPTD table which is never
	 * compressed
	 */
	if ((!entropy && (!crt_iptd_compress(compress, bits) || base == 0))
			|| direct) {
		iptd_entry_t *tmp_entry;
		/* No compression or requires directly modifing the table */
		/* Modify a single entry indexed by key */
		if (key_end == MODIFY_SINGLE) {
			tmp_entry = crt_read_iptd_entry(crt, base + key);
			if (tmp_entry == NULL)
				return TLU_MEM_ERROR;

			if (iptd_get_etype(tmp_entry) == IPTD_ETYPE_DATA) {
				crt_dec_data_ref_count(crt,
						iptd_get_base(tmp_entry), 1);
			}
			if (iptd_get_etype(new_entry) == IPTD_ETYPE_DATA) {
				crt_inc_data_ref_count(crt,
						iptd_get_base(new_entry), 1);
			}
			rc = crt_write_iptd_entry(crt, base + key,
							new_entry);
			if (rc < 0)
				return rc;

		} else {
			/* Modify all entries identical to old_entry */
			int count, start, end;
			count = 0;
			if (key_end == MODIFY_ALL) {
				start = 0;
				end = rundelta_get_size(entropy, bits) - 1;
			} else {
				start = crt_get_rundelta_index(entropy, bits,
						key);
				end = crt_get_rundelta_index(entropy, bits,
						key_end);
			}
			for (i = start; i <= end; i++) {
				tmp_entry = crt_read_iptd_entry(crt,
								base + i);
				if (tmp_entry == NULL)
					return TLU_MEM_ERROR;

				if (crt_modify_qualified(tmp_entry, old_entry,
							new_entry)) {
					if (iptd_get_etype(tmp_entry)
							== IPTD_ETYPE_DATA) {
						crt_dec_data_ref_count(crt,
						  iptd_get_base(tmp_entry),
							1);
					}
					rc = crt_write_iptd_entry(crt,
							base + i, new_entry);
					if (rc < 0)
						return rc;

					count++;
				}
			}
			if (iptd_get_etype(new_entry) == IPTD_ETYPE_DATA) {
				crt_inc_data_ref_count(crt,
						iptd_get_base(new_entry),
						count);
			}
		}
		/* No change on entropy*/
		*new_entropy = entropy;
		return base;
	}
	/* Load the table */
	rc = crt_load_iptd_table(crt, crt_iptd1, base, bits, entropy, 0);
	if (rc < 0) {
		/* This error is not recoverable */
		return rc;
	}

	/* Modify the table */
	/* Modify a single entry indexed by key */
	if (key_end == MODIFY_SINGLE)
		tlu_memcpy32(crt_iptd1 + key, new_entry, sizeof(iptd_entry_t));
	else {
		/* Modify all entries identical to old_entry */
		if (key_end == MODIFY_ALL)
			key_end = (1 << bits) - 1;

		for (i = key; i <= key_end; i++) {
			if (crt_modify_qualified(crt_iptd1 + i,
						old_entry, new_entry)) {
				tlu_memcpy32(crt_iptd1 + i, new_entry,
						sizeof(iptd_entry_t));
			}
		}

	}
	/* If base is 0, entropy must be 0. Thus it never comes here if base is
	 * 0. Compress the table
	 */
	if (crt_iptd_compress(compress, bits)) {
		modified_entropy = crt_compress_iptd(crt_iptd1, bits, crt_iptd2,
				&size);
		iptd = crt_iptd2;
		etype = IPTD_ETYPE_RUNDELTA;
	} else {
		modified_entropy = 0;
		size = 1 << bits;
		iptd = crt_iptd1;
		etype = IPTD_ETYPE_SIMPLE;
	}
	new_base = crt_create_iptd(crt, iptd, size);
	if (new_base < 0) {
		/* Reverse the load operation: recover data reference
		 * counters
		 */
		rc = crt_load_iptd_table(crt, crt_iptd1, base, bits,
						entropy, 1);
		if (rc < 0)
			return rc;

		return new_base;
	}

	*new_entropy = modified_entropy;
	/* When crt_switch_iptd failed, it must be memory error. Thus, no clean
	 * up is necessary
	 */
	return crt_switch_iptd(crt, prev_link, base,
			rundelta_get_size(entropy, bits),
			new_base, bits, *new_entropy, etype);
}

/*******************************************************************************
 * Description:
 *   This function inserts an IPTD table
 * Parameters:
 *   crt	  - The CRT table
 *   base	  - The base index of the parent IPTD table
 *   entropy      - The entropy of the parent IPTD table
 *   bits	  - The bit size of the parent IPTD table
 *   key	  - The partial key or the parent entry.
 *   insert_bits  - The size of the inserting table in bits
 *   entry        - The entry to be put in the new table.
 *   prev_base    - The base of the previous IPTD by which the parent IPTD is
 *                  linked. 0 is such IPTD does not exist.
 *   prev_index   - The index in the previous IPTD by which the parent IPTD is
 *  	            linked.  0 is such IPTD does not exist.
 *   compress     - Indicating whether the new table should be compressed if
 *                  possible if it is 1.  Otherwise no compression.
 *   insert_entropy - To return the entropy of the new table
 *   insert_base    - To return the base index of the new table
 *   new_entropy    - The new entropy of the parent table
 * Return:
 *   >= 0  The new base of the parent IPTD table
 *   < 0   Error code
 ******************************************************************************/
static int crt_insert_iptd(struct crt *crt, int base, uint32_t entropy,
		int bits, int key, int insert_bits, iptd_entry_t *entry,
		int prev_base, int prev_index, int compress,
		uint32_t *insert_entropy, int *insert_base, int *new_entropy)
{
	iptd_entry_t tmp_entry;
	int new_base;
	int size;

	CRT_LOG("%s base = %x bits = %d insert = %d compress = %d\n",
			__func__, base, bits, insert_bits, compress);
	*insert_base = crt_alloc_iptd(crt, insert_bits, entry, compress);
	if (*insert_base < 0)
		return *insert_base;

	if (entry)
		CRT_LOG("entry = %08x %08x\n", entry->words[0],
				entry->words[1]);

	/* Write IPTD entry */
	if (crt_iptd_compress(compress, insert_bits)) {
		iptd_fill_entry(&tmp_entry, 0x40000000, *insert_base,
				insert_bits - 1, IPTD_ETYPE_RUNDELTA);
		*insert_entropy = 0x40000000;
		size = 1;
	} else {
		iptd_fill_entry(&tmp_entry, 0, *insert_base, insert_bits - 1,
				IPTD_ETYPE_SIMPLE);
		*insert_entropy = 0;
		size = 1 << insert_bits;
	}
	new_base = crt_modify_iptd(crt, base, entropy, bits, key, MODIFY_SINGLE,
				NULL, &tmp_entry, prev_base + prev_index,
				compress, new_entropy, 0);
	if (new_base < 0) {
		/* Reverse the data counter which is increased in crt_alloc_iptd
		 * */
		if (entry && iptd_get_etype(entry) == IPTD_ETYPE_DATA)
			crt_dec_data_ref_count(crt, iptd_get_base(entry), size);

		crt_free_iptd_by_entry(crt, &tmp_entry);
		return new_base;
	}
	CRT_LOG("base = %x prev_base = %x\n", *insert_base, new_base);
	return new_base;
}

/*******************************************************************************
 * Description:
 *   This function to fill a rundelta table with a gived entry
 * Parameters:
 *   crt     - The CRT table
 *   base    - The base index of the IPTD table to be filled
 *   entropy - The entropy of the IPTD table
 *   bits    - The bit size of the IPTD table
 *   entry   - The entry used to fill the table
 *   prev_base  - The base of the previous IPTD by which this IPTD is linked.
 *	          0 is such IPTD does not exist.
 *   prev_index - The index in the previous IPTD by which this IPTD is linked.
 *	          0 is such IPTD does not exist.
 *   compress   - Indicating any modified/created IPTD table should be
 *                compressed if possible. if it is 1. Otherwise no compression.
 *   replaced_entry - Used to store the content of the entry which ihas been
 *		  replaced. The content should always be same even though more
 *		  than one entries might have been replaced.
 *   new_entropy - The entropy of the table
 *   recursive_count - A pointer to a recursive counter which counts how many
 *                times this function has been recursively called. The current
 *		  counter value is passed in and a new value will be passed
 *		  back. If the current couter + 1 > crt->max_iptd_levels, an
 *		  error code, TLU_CHAIN_TOO_LONG, will be returned.
 * Return:
 *   >= 0  The base index of the new table.
 *   < 0   Error code
 * Note:
 *   Only the current base (it may be a new one) is returned though there might
 *   be more than one tables are modified.
 ******************************************************************************/
static int crt_fill_rundelta(struct crt *crt, int base, uint32_t entropy,
		int bits, iptd_entry_t *entry, int prev_base, int prev_index,
		int compress, iptd_entry_t *replaced_entry,
		uint32_t *new_entropy, int *recursive_count)
{
	int i, err, count, size;
	int data_index;
	int index;
	int new_base;
	int fail_count, data_count;

	CRT_LOG("%s[%d] base = %x bits = %d entropy = %x\n",
			__func__, *recursive_count, base, bits, entropy);
	if (++(*recursive_count) > crt->max_iptd_levels) {
		CRT_LOG("%s chain too long: %d\n", __func__, *recursive_count);
		return TLU_CHAIN_TOO_LONG;
	}

	/* Debug check */
	if (bits > CRT_MAX_IPTD_BITS) {
		CRT_CRIT("IPTD size (%d bits) exceeds the maximum limit (%d)\n",
				bits, CRT_MAX_IPTD_BITS);
		return TLU_TABLE_ERROR;
	}

	fail_count = 0;
	data_count = 0;
	data_index = iptd_get_base(entry);
	size = rundelta_get_size(entropy, bits);

	index = 0;
	for (i = 0; i < size; i++) {
		iptd_entry_t *tmp_entry;
		tmp_entry = crt_read_iptd_entry(crt, base + i);
		if (tmp_entry == NULL)
			return TLU_MEM_ERROR;

		count = _rundelta_get_entry_count(entropy, bits, index);
		index += count;
		switch (iptd_get_etype(tmp_entry)) {
		case IPTD_ETYPE_FAIL:
				fail_count++;
				/* Keep one which will be replaced. This
				 * should always be same.  So does not
				 *  matter overwrite it
				 */
				tlu_memcpy32(replaced_entry, tmp_entry,
						sizeof(iptd_entry_t));
				break;
		case IPTD_ETYPE_DATA:
				/* Should never equal */
				if (crt_get_key_size(tmp_entry)
						< crt_get_key_size(entry)) {
					data_count++;
					/* Keep one which will be replaced.
					 * This should always be same.
					 * So does not matter overwrite it
					 */
					tlu_memcpy32(replaced_entry, tmp_entry,
							sizeof(iptd_entry_t));
				}
				break;
		case IPTD_ETYPE_SIMPLE:
		case IPTD_ETYPE_RUNDELTA:
				err = crt_fill_rundelta(crt,
						iptd_get_base(tmp_entry),
						iptd_get_entropy(tmp_entry),
						iptd_get_bits(tmp_entry),
						entry, base, i, compress,
						replaced_entry, new_entropy,
						recursive_count);
				if (err < 0)
					return err;

		break;
		}

	}
	/* Modify all FAIL or data entries */
	new_base = base;
	*new_entropy = entropy;
	/* DEBUG: Modifying fail entries and data entries simultaniously
	 * should never happen
	 */
	if (fail_count != 0 && data_count != 0) {
		CRT_CRIT("Internal Error: IPTD table corrupted:"
				"fail_count = %d data_count = %d\n",
				fail_count, data_count);
		return TLU_TABLE_ERROR;
	}

	if (data_count != 0 || fail_count != 0) {
#ifndef CRT_REDUNDANT
		/* data_count != 0 indicates that iptd_get_base(replaced_entry)
		 * is a valid data index
		 */
		if (data_count != 0 && data_count
				>= crt_get_data_ref_count(crt,
					iptd_get_base(replaced_entry))) {
			CRT_LOG("%s: Attempt to insert a duplicate entry\n",
					__func__);
			return TLU_REDUNDANT_ENTRY;
		}
#endif
		new_base = crt_modify_iptd(crt, base, entropy, bits, 0,
					MODIFY_ALL, NULL, entry,
					prev_base + prev_index, compress,
					new_entropy, 0);
		if (new_base < 0)
			return new_base;

	}
	(*recursive_count)--;
	CRT_LOG("%s[%d] new_base = %x new_entropy = %x\n",
			__func__, *recursive_count, new_base, *new_entropy);
	return new_base;
}

/*******************************************************************************
 * Description:
 *   This function to reverse split operation on an IPTD table including
 *	 1. Reverse tables linked by iptd's first 'num' entries
 *	 2. Reverse table pointed by base.
 * Parameters:
 *   crt     - The CRT table
 *   iptd    - The parent IPTD table generated during splitting.
 *   num     - Total number of entries in the parent table.
 *   base    - The base index of the IPTD table being split.
 *   bits    - The bit size of the IPTD table being split
 *   entropy - The entropy of the IPTD table being split
 *   tmp     - Temporary table memory required for loading. The loaded content
 * 	       will be discarded.
 * Return:
 *   0 indicates success and a negative indicates an error
 ******************************************************************************/
#ifdef CRT_SPLIT_ENABLE
static int crt_split_cleanup(struct crt *crt, iptd_entry_t *iptd, int num,
		int base, int bits, uint32_t entropy, iptd_entry_t *tmp)
{
	int i, type, rc;

	CRT_LOG("%s: num = %d\n", __func__, num);
	for (i = 0; i < num; i++) {
		type = iptd_get_etype(iptd + i);
		if (type == IPTD_ETYPE_SIMPLE || type == IPTD_ETYPE_RUNDELTA) {
			/* Reverse the write oprtation: Decreament increased
			 * counters by load it again
			 */
			rc = crt_load_iptd_table(crt, tmp, iptd[i].base,
							iptd[i].keyshl + 1,
							iptd[i].entropy, 0);
			if (rc < 0)
				return rc;

			rc = crt_free_iptd_by_entry(crt, iptd + i);
			if (rc < 0)
				return rc;

		}
	}
	/* Reverse the load operation: recover data reference counters */
	rc = crt_load_iptd_table(crt, tmp, base, bits, entropy, 1);
	if (rc < 0)
		return rc;

	return 0;
}

/*******************************************************************************
 * Description:
 *   This function to split a rundelta table witt size "bits" into several i
 *    tables with size "new_bits".
 * Parameters:
 *   crt	 - The CRT table
 *   base	 - The base index of the IPTD table to be split.
 *   entropy     - The entropy of the IPTD table to be split
 *   bits	 - The bit size of the IPTD table to be split
 *   new_bits    - The size of new tables in bits.
 *   index       - The index of the parent entry
 *   bit_offset  - the bit offset of the partial key associated with this table
 *                 in the full key.
 *   compress    - Indicates whether the splited table should be compressed if
 *		   possible.
 *   new_entropy - The new entropy of the table.
 * Return:
 *   >= 0 the base index of the new table
 *   < 0   Error
 ******************************************************************************/
static int crt_split_rundelta(struct crt *crt, int base, uint32_t entropy,
		int bits, int new_bits, int index, int bit_offset,
		int compress, uint32_t *new_entropy)
{
	int new_base, new_size, sub_base, sub_bits, sub_size;
	int compressed_size, i, rc;
	iptd_entry_t *iptd1, *new_iptd, *sub_iptd;
	uint32_t tmp_entropy, type;
	iptd_entry_t *tmp_iptd;
	iptd_entry_t *crt_iptd1, *crt_iptd2;

	/* Total 2 * (1 << CRT_MAX_IPTD_BITS) entries in the buf */
	crt_iptd1 = crt->tmp_iptd_buf;
	crt_iptd2 = crt->tmp_iptd_buf + (1 << CRT_MAX_IPTD_BITS);

	CRT_LOG("%s bits = %d new_bits = %d\n", __func__, bits, new_bits);

	sub_bits = bits - new_bits;
	new_size = 1<<new_bits;
	sub_size = 1 << sub_bits;

	/* Load the table*/
	rc = crt_load_iptd_table(crt, crt_iptd1, base, bits, entropy, 0);
	if (rc < 0)
		return rc;

	/* Allocate temporary tables */
	new_iptd = crt_iptd2;
	sub_iptd = new_iptd + new_size;

	/* Compress the sub-tables */
	iptd1 = crt_iptd1;
	for (i = 0; i < new_size; i++) {
		int type;

		/* Don't need a new IPTD table */
		if (crt_iptd_redundant(crt, iptd1, 0, sub_size,
					bit_offset + new_bits, NULL)) {
			tlu_memcpy32(new_iptd + i, iptd1, sizeof(iptd_entry_t));
		} else {
			if (crt_iptd_compress(compress, sub_bits)) {
				/* Compress the table */
				tmp_entropy = crt_compress_iptd(iptd1, sub_bits,
						sub_iptd, &compressed_size);
				tmp_iptd = sub_iptd;
				type = IPTD_ETYPE_RUNDELTA;
			} else {
				/* No compression, Directly create a table */
				tmp_entropy = 0;
				type = IPTD_ETYPE_SIMPLE;
				compressed_size = sub_size;
				tmp_iptd = iptd1;
			}
			sub_base = crt_create_iptd(crt, tmp_iptd,
							compressed_size);
			if (sub_base < 0) {
				CRT_LOG("%s: error = %d\n", __func__, sub_base);
				rc = crt_split_cleanup(crt, new_iptd, i,
								base, bits,
								entropy,
								crt_iptd1);
				if (rc < 0) {
					/* Return this error because it is
					 * always fatal
					 */
					return rc;
				}
				return sub_base;
			}
			iptd_fill_entry(new_iptd + i, tmp_entropy, sub_base,
					sub_bits - 1, type);
		}
		iptd1 += sub_size;
	}

	/* Create the new table */
	CRT_LOG("Write new table\n");
	if (crt_iptd_compress(compress, new_bits)) {
		tmp_entropy = crt_compress_iptd(new_iptd, new_bits, crt_iptd1,
				&compressed_size);
		tmp_iptd = crt_iptd1;
		type = IPTD_ETYPE_RUNDELTA;
	} else {
		tmp_entropy = 0;
		compressed_size = new_size;
		tmp_iptd = new_iptd;
		type = IPTD_ETYPE_SIMPLE;
	}

	new_base = crt_create_iptd(crt, tmp_iptd, compressed_size);

	if (new_base < 0) {
		if (new_base == TLU_OUT_OF_MEM) {
			/* Items in tmp_iptd has not been counted yet so
			 * no need to reverse it.
			 */
			rc = crt_split_cleanup(crt, new_iptd, new_size,
							base, bits, entropy,
							crt_iptd1);
			if (rc < 0) {
				/* Return this error because it is always
				 * fatal
				 */
				return rc;
			}
		}
		return new_base;
	}
	*new_entropy = tmp_entropy;
	/* When crt_switch_iptd failed, it must be memory error.
	 * Thus, no clean up is necessary
	 */
	return crt_switch_iptd(crt, index, base,
			rundelta_get_size(entropy, bits),
			new_base, new_bits, tmp_entropy, type);
}
#endif
/*******************************************************************************
 * Description:
 *   This function deletes an IPTD table if it is redundant.
 *   The uplevel tables will not be rebuilt even though compression may change.
 *    Thus trace stack does not need to be updated.
 * Parameters:
 *   crt	- The CRT table
 *   base	- The base index of the IPTD table to be deleted
 *   bits	- The bit size of the IPTD table to be deleted
 *   entropy    - The entropy of the IPTD table to be deleted
 *   bit_offset - the bit offset of the partial key associated with this table
 *                in the full key.
 *   prev_base  - The base index of the parent table
 *   prev_index - The index of the parent entry
 * Return:
 *   >= 0  The original base base index of the table
 *   < 0   Error
 ******************************************************************************/
static int crt_delete_redundant_iptd(struct crt *crt, int base, int bits,
		uint32_t entropy, int bit_offset, int prev_base, int prev_index)
{
	int rc, size, del;
	iptd_entry_t entry;

	CRT_LOG("%s: base = %x bits = %d entropy = %08x prev_base = %x "
			"prev_index = %x\n",
			__func__, base, bits, entropy, prev_base, prev_index);
	size = rundelta_get_size(entropy, bits);
	del = crt_iptd_redundant(crt, NULL, base, size, bit_offset + bits,
					&entry);
	if (del < 0)
		return del;

	/* Remove the empty IPTD */
	if (del) {
		rc = _crt_switch_iptd(crt, prev_base + prev_index, base,
						size, &entry);
		if (rc < 0)
			return rc;

		/* Modify the reference counter if it is a data entry */
		if (iptd_get_etype(&entry) == IPTD_ETYPE_DATA) {
			/* The final reference counter will not be 0,
			 * so no destroy is necessary
			 */
			crt_inc_data_ref_count(crt, iptd_get_base(&entry),
					1 - size);
		}
		return 0;
	}
	return base;
}

/*******************************************************************************
 * Description:
 *    This function deletes any iredundant IPTD tables from those recorded in
 *    the trace stack.
 * Parameters:
 *   crt	- The CRT table
 *   base	- The base index of the IPTD table to be deleted
 *   bits	- The bit size of the IPTD table to be deleted
 *   entropy    - The entropy of the IPTD table to be deleted
 *   bit_offset - the bit offset of the partial key associated with this table
 *                in the full key.
 *   prev_base  - The base index of the parent table
 *   prev_index - The index of the parent entry
 *   key_bits   - Key size in bits
 *   trace_stack - Trace stack
 *   trace_num   - Total entry number in the trace stack
 * Return:
 *   >= 0  The original base base index of the table
 *   < 0   Error
 ******************************************************************************/
static int crt_clean_super_redundant_iptd(struct crt *crt, int key_bits,
		struct crt_trace *trace_stack, int trace_num)
{
	int base, prev_base, prev_index;

	CRT_LOG("%s: key_bits = %d trace_num = %d\n",
			__func__, key_bits, trace_num);

	while (--trace_num >= 0) {
		if (trace_num > 0) {
			prev_base = trace_stack[trace_num - 1].base;
			prev_index = trace_stack[trace_num - 1].index;
		} else {
			prev_base = 0;
			prev_index = 0;
		}
		base = crt_delete_redundant_iptd(crt,
				trace_stack[trace_num].base,
				trace_stack[trace_num].bits,
				trace_stack[trace_num].entropy,
				key_bits, prev_base, prev_index);
		if (base < 0)
			return base;

		if (base)
			break;

		key_bits -= trace_stack[trace_num].bits;
	}
	return 0;
}

/* Push search path information into the trace stack */
static int crt_trace_push(struct crt *crt, struct crt_trace *trace_stack,
		int trace_num, int iptd_base, int iptd_index, int iptd_bits,
		uint32_t entropy)
{
	if (trace_num >= crt->max_iptd_levels) {
		CRT_CRIT("%s: Chain too long:\n", __func__, trace_num);
		return TLU_CHAIN_TOO_LONG;
	}
	trace_stack[trace_num].base = iptd_base;
	trace_stack[trace_num].index = iptd_index;
	trace_stack[trace_num].bits = iptd_bits;
	trace_stack[trace_num].entropy = entropy;
	return trace_num + 1;
}

#ifdef CRT_STORE_KEY
static int crt_key_match(void *key1, void *key2, int key_size)
{
	int len, bits;

	len = key_size / 8;
	if (memcmp(key1, key2, len))
		return 0;

	bits = key_size % 8;
	if ((((uint8_t *)key1)[len] ^ ((uint8_t *)key2)[len])
			& (((1 << bits) - 1) << (8 - bits))) {
		return 0;
	}
	return 1;
}

int crt_data_info_delete(struct crt *crt, void *key, int key_bits)
{
	int i;
	struct crt_data_info *info;

	info = crt->data_info;
	for (i = 0; i < crt->data_entry_num; i++) {
		if (info->key_size == key_bits
				&& crt_key_match(info->key, key, key_bits)) {
			info->key_size = -1;
			return i + crt->data_start;
		}
		info++;
	}
	return 0;
}

/* Find a replacement entry from the data information list */
static int crt_find_replacement(struct crt *crt, void *key, int key_bits,
		iptd_entry_t *entry)
{
	int i, index;
	struct crt_data_info *info;

	index = -1;
	info = crt->data_info;
	for (i = 0; i < crt->data_entry_num; i++) {
		if (info->key_size >= 0 && info->key_size < key_bits
				&& crt_key_match(info->key, key,
					info->key_size)) {
			/* Choose one with longer key */
			if (index == -1 || crt->data_info[index].key_size
					< info->key_size) {
				index = i;
			}
			if (info->key_size == key_bits - 1) {
				/* find the candidate */
				break;
			}
		}
		info++;
	}
	if (index == -1) {
		CRT_LOG("Replacement not found\n");
		iptd_fill_entry(entry, 0, 0, 0, IPTD_ETYPE_FAIL);
		return 0;
	} else {
		int size;

		size = crt->data_info[index].key_size;
		crt_fill_iptd_data_entry(entry, index + crt->data_start,
				crt->data_info[index].key_size);
		CRT_LOG("Replacement: key_bits = %d %08x %08x\n",
				size, entry->words[0], entry->words[1]);
		return size;
	}
}

/* Search the key in the data information list */
static int crt_key_search(struct crt *crt, void *key, int key_bits, int index)
{
	int i;
	struct crt_data_info *info;

	index -= crt->data_start;
	info = crt->data_info;
	for (i = 0; i < crt->data_entry_num; i++) {
		if (info->key_size == key_bits
				&& crt_key_match(info->key, key, info->key_size)
				&& index != i) {
			return 1;
		}
		info++;
	}
	return 0;
}
#else
int crt_compute_max_wild_key_size(int bit_offset, int max_bits, uint32_t key,
		uint32_t index, int index_bits)
{
	int n;

	while ((n = max_bits - bit_offset) > 0 &&
			(key >> (32 - n)) != (index >> (index_bits - n))) {
		max_bits--;
	}
	return max_bits;
}

/*******************************************************************************
 * Description:
 *   This function is to find a longest wild entry which meets the following
 *   criteria in the specified IPTD table and those below it.
 *	1. It is a data entry and it is located in the current IPTD table or
 *         any IPTD tables below the current one.
 *	2. Its key bit number is less than or equal to a given number
 *	   <max_bits>.
 *	3. The key bit number should be greater than that of the entry
 *         specified by <wild_entry>.
 *	4. It has the largest key size in all candidates.
 * Parameters:
 *   crt        - The CRT table
 *   base       - The base index of the IPTD table
 *   bits       - The bit size of the IPTD table
 *   entropy    - The entropy of the IPTD table.
 *   max_bits   - The maximum (inclusive) key bit number which a qulified
 *                entry should be.
 *   key_offset - The bit offset of 'key' within the full key.
 *   key	- The partial key value which is left aligned.
 *   wild_entry - The current qualified entry. It is also used to return the
 *	          new qualified entry.
 *   recursive_count - The recursive counter of calling of this function.
 *                     This is to prevent infinitly recursive calling.
 * Return:
 *   > 0  The key size in bits in <wild_entry>.
 *   = 0  Not found
 *   < 0  Error code
 ******************************************************************************/
static int _crt_find_wild_entry(struct crt *crt, int base, int bits,
		uint32_t entropy, int max_bits, int key_offset, uint32_t key,
		iptd_entry_t *wild_entry, int *recursive_count)
{
	int i, type;
	iptd_entry_t *entry;
	int key_bits_found, size;
	int prev_max;   /* For debug */
	int key_found, count;
	int table_count;
	int index;

	if (++(*recursive_count) > crt->max_iptd_levels) {
		CRT_LOG("%s chain too long: %d\n", __func__, *recursive_count);
		return TLU_CHAIN_TOO_LONG;
	}
	prev_max = crt_get_key_size(wild_entry);
	CRT_LOG("%s[%d]: base = %x bits = %d entropy = %08x max_bits = %d"
			" key_offset = %d key = %x\n",
			__func__, *recursive_count, base, bits, entropy,
			max_bits, key_offset, key);
	/* Get the physical table entry number */
	size = rundelta_get_size(entropy, bits);
	key_found = 0;
	key_bits_found = 0;
	table_count = 0;
	for (i = 0; i < size; i++) {
		count = _rundelta_get_entry_count(entropy, bits, key_found);
		entry = crt_read_iptd_entry(crt, base + i);
		if (entry == NULL)
			return TLU_MEM_ERROR;

		type = iptd_get_etype(entry);
		if (type == IPTD_ETYPE_DATA) {
			/*
			 * ................ xxxxxxxxx 0000
			 * |   	         |         |    |
			 * |                + -------------- + -->bits,key_found
			 * |	         |	   |
			 * + -------------------------- + ------->key_bits_found
			 * |	         |
			 * + ---------------- + ----------------->key_offset
			 */
			int data_key_size;

			data_key_size = crt_get_key_size(entry);
			/* 1. The key size must be less than max_bits
			 * 2. The key must be longer than current found one.
			 */
			if (data_key_size <= max_bits
				&& data_key_size > crt_get_key_size(wild_entry)
				&& (key >> (32 - (data_key_size - key_offset)))
					== (key_found >>
					(key_offset + bits - data_key_size))) {
				/* Overwrite the entry */
				tlu_memcpy32(wild_entry, entry,
						sizeof(iptd_entry_t));
				key_bits_found = data_key_size;
				if (key_bits_found == max_bits) {
					CRT_LOG("Found a wild entry: "
							"%08x %08x\n",
							wild_entry->words[0],
							wild_entry->words[1]);
					return key_bits_found;
				}
			}
		} else if (type == IPTD_ETYPE_SIMPLE
				|| type == IPTD_ETYPE_RUNDELTA) {
			table_count++;
		}
		key_found += count;
	}
	/* Any key found in the current table should be the right one */
	if (!key_bits_found && table_count) {
		index = 0;
		for (i = 0; i < size; i++) {
			count = _rundelta_get_entry_count(entropy, bits, index);
			entry = crt_read_iptd_entry(crt, base + i);
			if (entry == NULL)
				return TLU_MEM_ERROR;

			type = iptd_get_etype(entry);
			/* It is a table: do the recursive search */
			if (type == IPTD_ETYPE_SIMPLE
					|| type == IPTD_ETYPE_RUNDELTA) {
				key_bits_found = _crt_find_wild_entry(crt,
				iptd_get_base(entry),
				iptd_get_bits(entry),
				iptd_get_entropy(entry),
				crt_compute_max_wild_key_size(key_offset,
								max_bits,
								key, index,
								bits),
						key_offset, key, wild_entry,
						recursive_count);
				if (key_bits_found < 0)
					return key_bits_found;

			}
			index += count;
		}
	}
	(*recursive_count)--;
	key_bits_found = crt_get_key_size(wild_entry);
	/* Debug */
	if (key_bits_found > prev_max) {
		CRT_LOG("[%d]Found: %08x %08x\n", *recursive_count,
				wild_entry->words[0], wild_entry->words[1]);
	}
	return key_bits_found;
}

/*******************************************************************************
 * Description:
 *   This function is to find a replacement entry in the given IPTD table and
 *   those in the trace stack. The criteria of a replacement entry is the same
 *   as that of a wild entry defined in _crt_find_wild_entry.
 * Parameters:
 *   crt	 - The CRT table
 *   base	 - The base index of the IPTD table
 *   bits	 - The bit size of the IPTD table
 *   entropy     - The entropy of the IPTD table.
 *   key_bits    - Full key size in bits.
 *   key_offset  - The bit offset of 'key' within the full key.
 *   key	 - The partial key value which is right aligned. Its size is
 *                 key_bits - key_offset
 *   trace_stack - The trace stack which contains information of the upper level
 *		   IPTD tables.
 *   trace_num   - Total number of stack entries in <trace_stack>
 *   entry       - To return a found entry.
 * Return:
 *   > 0  The key size in bits in <entry>.
 *   = 0  Not found
 *   < 0  Error code
 ******************************************************************************/
static int crt_find_replacement(struct crt *crt, int base, int bits,
		uint32_t entropy, int key_bits, int key_offset, int key,
		struct crt_trace *trace_stack, int trace_num,
		iptd_entry_t *entry)
{
	int key_bits_found, cur_bits;
	int recursive_count = 0;
	/* The maximum (inclusive) bit number of a replacement key. */
	int max_bits;
	uint32_t key32;

	CRT_LOG("%s key_bits = %d key_offset = %d trace_num = %d\n", __func__,
			key_bits, key_offset, trace_num);
	/* Make it left aligned */
	key32 = key << (32 - (key_bits - key_offset));
	/* Search the current and below tables */
	iptd_fill_entry(entry, 0, 0, 0, IPTD_ETYPE_FAIL);
	key_bits_found = _crt_find_wild_entry(crt, base, bits, entropy,
					key_bits - 1, key_offset, key32,
					entry, &recursive_count);
	if (key_bits_found < 0)
		return key_bits_found;

	max_bits = key_bits - bits;

	/* Search the upper levels */
	while (--trace_num > 0) {
		cur_bits = trace_stack[trace_num].bits;
		key_bits_found = _crt_find_wild_entry(crt,
					trace_stack[trace_num].base,
					cur_bits,
					trace_stack[trace_num].entropy,
					max_bits, key_offset, key32,
					entry, &recursive_count);
		if (key_bits_found < 0)
			return key_bits_found;

		max_bits -= cur_bits;
	}
	CRT_LOG("Replacement: key_bits = %d %08x %08x\n",
			key_bits_found, entry->words[0], entry->words[1]);
	return key_bits_found;
}

#endif
/*******************************************************************************
 * Description:
 *   This function is to delete one IPTD entry specified by <key> or any entries
 *   whose key size equal to key_bits in the specified IPTD table. The data
 *   entry given by data_index will be deleted. The IPTD table will be deleted
 *   as well if it becomes redundant.
 * Parameters:
 *   base     - The base index of the IPTD table
 *   bits     - The bit size of the IPTD table
 *   entropy  - The entropy of the IPTD table.
 *   key      - >= 0: The key of the deleting entry corresponding to the IPTD
 *		      table. It is also the index of the entry in an
 *                    uncompressed table.
 *	        = -1: Any entries whose key size equal to key_bits will be
 *		      deleted.
 *   key_bits    - The key size in bits of entries to be deleted if key == -1. t
 * 		   is ignored if key >= 0.
 *   prev_base   - The base index of the parent table
 *   prev_index  - The index in the parent table
 *   trace_stack - the trace stack
 *   trace_num   - Total entry number in the trace stack
 *   entry	 - The content of the entry to be deleted.
 *   new_entropy - Entropy of the table after deletion
 *   data_index  - The index of the data entry to be deleted.
 * Return:
 *   >= 0  The base index of the table after deletion
 *   < 0   Error code. TLU_OUT_OF_MEM error may occur.
 ******************************************************************************/
static int crt_delete_data_entry(struct crt *crt, int base, int bits,
		uint32_t entropy, uint32_t key, int key_end, int key_bits,
		int prev_base, int prev_index, struct crt_trace *trace_stack,
		int trace_num, iptd_entry_t *entry, iptd_entry_t *new_entry,
		int data_index, uint32_t *new_entropy)
{
	int new_base;

	CRT_LOG("%s base=%x bits=%d entropy=%08x key=%08x key_bits=%d\n",
			__func__, base, bits, entropy, key, key_bits);
	*new_entropy = entropy;
	new_base = crt_modify_iptd(crt, base, entropy, bits, key, key_end,
					entry, new_entry,
					prev_base + prev_index,
					crt->compress, new_entropy, 0);
	if (new_base < 0) {
		/* If running out of memory, use direct modification.
		 * Suppose deleting a data entry can keep the current
		 * entropy unchanged. The side effect of this is the
		 * table may not be best compressed.
		 */
		if (new_base == TLU_OUT_OF_MEM) {
			CRT_LOG("Run out of memory. "
				"Use direct table modification\n");
			new_base = crt_modify_iptd(crt, base, entropy,
							bits, key, key_end,
							entry, new_entry,
							prev_base + prev_index,
							crt->compress,
							new_entropy, 1);
			if (new_base < 0)
				return new_base;

		}
		return new_base;
	}

	return new_base;
}

/*******************************************************************************
 * Description:
 *   This function is to delete entries in IPTD tables which meet the following
 *   criteria:
 *   1. Entry key size equal to key_bits
 *   2. Entries are in the current IPTD table or those below the current one
 *   Any IPTD table which becomes redundant will be deleted as well.
 * Parameters:
 *   base      - The current base index of the current IPTD table
 *   bits      - The bit size of the current IPTD table
 *   entropy   - The entropy of the current IPTD table
 *   key_bits  - The key size of entries to be deleted
 *   entropy   - The entropy of the current IPTD table.
 *   key_start - The start position of the deletion in the IPTD table.
 *   key_end   - >= 0 The end position of the deletion
 *	         <0  MODIFY_ALL indicates delete the whole table
 *		     MODIFY_SINGLE indicates delete a single entry pointed
 *                   by 'key_start'.
 *   replacement - Deleted entries will be replaced by this entry.
 *   prev_base   - The base index of the parent table
 *   prev_index  - The index in the parent table
 *   trace_stack - the trace stack
 *   trace_num   - Total entry number in the trace stack
 *   new_base    - The base index of the table after deletion
 *   new_entropy - Entropy of the table after deletion
 *   recursive_count - The recursive counter of calling of this function.
 *                     This is to prevent infinitly recursive calling.
 * Return:
 *	< 0  An error encountered
 *	= 0  The data entry does not exist
 *	>= 0 The data entry index. The data is not freed.
 ******************************************************************************/
static int crt_delete_iptd_entries(struct crt *crt, int base, int bits,
		uint32_t entropy, int key_bits, int key_start, int key_end,
		iptd_entry_t *replacement, int prev_base, int prev_index,
		struct crt_trace *trace_stack, int trace_num,
		uint32_t *new_base, uint32_t *new_entropy,
		int *recursive_count)
{
	int i, type;
	iptd_entry_t *entry;
	int data_index;
	int empty;
	int sub_base, size;
	int max_bits;
	int data_count;
	int clean, data_index1, start_index, end_index;

	CRT_LOG("%s[%d]: base = %x key_bits = %d bits = %d entropy = %08x"
			" prev_base = %x prev_index = %x key[%x:%x]\n",
			__func__, *recursive_count, base, key_bits,
			bits, entropy, prev_base, prev_index, key_start,
			key_end);
	if (++(*recursive_count) > crt->max_iptd_levels) {
		CRT_LOG("%s chain too long: %d\n", __func__, *recursive_count);
		return TLU_CHAIN_TOO_LONG;
	}

	*new_entropy = entropy;
	max_bits = -1;
	empty = 1;
	size = rundelta_get_size(entropy, bits);
	data_count = 0;
	data_index = 0;
	data_index1 = 0;
	clean = 1;
	if (key_end == MODIFY_ALL) {
		start_index = 0;
		end_index = rundelta_get_size(entropy, bits) - 1;
	} else {
		start_index = crt_get_rundelta_index(entropy, bits, key_start);
		end_index = crt_get_rundelta_index(entropy, bits, key_end);
	}
	for (i = start_index; i <= end_index; i++) {
		entry = crt_read_iptd_entry(crt, base + i);
		if (entry == NULL)
			return TLU_MEM_ERROR;

		type = iptd_get_etype(entry);
		CRT_LOG("delete entry[%x] <%08x %08x>\n",
				i, entry->words[0], entry->words[1]);
		if (type == IPTD_ETYPE_SIMPLE || type == IPTD_ETYPE_RUNDELTA) {
			uint32_t tmp_entropy;
			int rc;

			sub_base = iptd_get_base(entry);
			rc = crt_delete_iptd_entries(crt, sub_base,
					iptd_get_bits(entry),
					iptd_get_entropy(entry),
					key_bits, 0, MODIFY_ALL,
					replacement, base, i, trace_stack,
					trace_num, &sub_base, &tmp_entropy,
					recursive_count);
			if (rc < 0)
				return rc;

			/* Update the deleted data_index if rc is not 0. */
			if (rc != 0 && data_index1 == 0)
				data_index1 = rc;

			/* The sub-table is still exist. Don't need
			 * clean this table
			 */
			if (sub_base != 0)
				clean = 0;

		} else if (type == IPTD_ETYPE_DATA) {
			if (crt_get_key_size(entry) == key_bits) {
				data_count++;
				data_index = iptd_get_base(entry);
			}
		}
	}
	if (data_count) {
		iptd_entry_t tmp_entry;

		crt_fill_iptd_data_entry(&tmp_entry, data_index, key_bits);
		base = crt_delete_data_entry(crt, base, bits, entropy,
				key_start, key_end, key_bits, prev_base,
				prev_index, trace_stack, trace_num,
				&tmp_entry, replacement, data_index,
				new_entropy);
		if (base < 0)
			return base;

	}
	if (clean) {
		base = crt_delete_redundant_iptd(crt, base, bits,
						*new_entropy, key_bits,
						prev_base, prev_index);
		if (base < 0)
			return base;

	}

	(*recursive_count)--;
	CRT_LOG("%s[%d]: base = %x data_index = %x %x\n",
			__func__, *recursive_count, base,
			data_index, data_index1);
	*new_base = base;
	return data_index > 0 ? data_index : data_index1;
}

/*******************************************************************************
 * Description:
 *   This function inserts a data entry into the specified IPTD table and those
 *   below it. It will replace all empty entries and data entries with less key
 *   bits.
 * Parameters:
 *   crt        - The CRT table
 *   base       - The base index of the IPTD table to be inserted
 *   entropy    - The entropy of the IPTD table
 *   bits       - The bit size of the IPTD table
 *   iptd_entry - The data entry to be inserted
 *   prev_base  - The base of the previous IPTD by which this IPTD is linked.
 *	          0 is such IPTD does not exist.
 *   prev_index - The index in the previous IPTD by which this IPTD is linked.
 *	          0 is such IPTD does not exist.
 *   compress   - Indicating any modified/created IPTD table should be
 *                compressed if possible. if it is 1. Otherwise no compression.
 *   new_entropy - The entropy of the table after insrtion
 * Return:
 *   >= 0  The base index of the table after insertion
 *   < 0   Error code
 * Notes:
 *   1. Callers can check crt_get_data_ref_count of data_index to figure out
 *      whether the entry has been inserted.
 *   2. Inserted entries are clean upon error. But the date entry is not freed.
 ******************************************************************************/
static int crt_insert_rundelta_data_wild(struct crt *crt, int base,
		uint32_t entropy, int bits, iptd_entry_t *iptd_entry,
		int prev_base, int prev_index, int compress,
		uint32_t *new_entropy)
{
	int new_base;
	iptd_entry_t replaced_entry;
	int recursive_count;
	int data_index, index;

	CRT_LOG("%s bits = %d\n", __func__, bits);

	recursive_count = 0;
	data_index = iptd_get_base(iptd_entry);
	new_base = crt_fill_rundelta(crt, base, entropy, bits, iptd_entry,
					prev_base, prev_index, compress,
					&replaced_entry, new_entropy,
					&recursive_count);
	if (new_base < 0) {
		/* Cleanup if any data has been really inserted */
		if (crt_get_data_ref_count(crt, data_index) > 0) {
			int tmp;
			/* new_base should be equal to base if the operation
			 * was failed. And this deletion (cleapup) will
			 * not change the base either
			 */
			recursive_count = 0;
			index = crt_delete_iptd_entries(crt, base, bits,
					entropy, crt_get_key_size(iptd_entry),
					0, MODIFY_ALL, &replaced_entry,
					prev_base, prev_index, NULL, 0, &tmp,
					new_entropy, &recursive_count);
			if (index < 0)
				return index;

		}
		return new_base;
	}
	return new_base;
}

uint32_t crt_required_mem_size(int data_entry_num)
{
	/* Two pieces of memories are required:
	 * One for data_entry reference counters and another for temporary IPTD
	 * tables. The latter is really per TLU based instead of per CRT.
	 * However, since it is relatively small when CRT_MAX_IPTD_BITS is 8,
	 * and to make APIs simple, it is assigned per table base. A large
	 * CRT_MAX_IPTD_BITS will make this a problem.
	 */
	return data_entry_num *sizeof(struct crt_data_info) +
		(1 << CRT_MAX_IPTD_BITS) * sizeof(iptd_entry_t) * 2;
}

/* See description in the header file */
int crt_create(struct crt *crt, tlu_handle_t tlu, int table, int size,
		int key_bits, int kdata_size, int kdata_entry_num,
		struct tlu_bank_mem *bank, int compress, void *buf)
{
	int min_iptd_bits;

	/* Perform parameter checks */
	if (table >= TLU_MAX_TABLE_NUM) {
		CRT_WARN("Invalid table ID: %d. A valid value must be "
				" between 0 and 31.\n", table);
		return TLU_INVALID_PARAM;
	}
	if (key_bits != 32 && key_bits != 64 && key_bits != 96
			&& key_bits != 128) {
		CRT_WARN("Invalid key size: %d. A valid value must be"
				" 32, 64, 96 or 128.\n", key_bits);
		return TLU_INVALID_PARAM;
	}
	if (kdata_size != 8 && kdata_size != 16 && kdata_size != 32
			&& kdata_size != 64) {
		CRT_WARN("Invalid key-data size: %d. A valid value must"
				" be 8, 16, 32, 64\n", kdata_size);
		return TLU_INVALID_PARAM;
	}
	if (kdata_entry_num < 1) {
		CRT_WARN("key-data entry number (%d) must be greater than 0\n",
				kdata_entry_num);
		return TLU_INVALID_PARAM;
	}

	crt->table_start = _tlu_bank_mem_alloc(bank, size);

	if (crt->table_start < 0) {
		CRT_WARN("Out of bank memory\n");
		return TLU_OUT_OF_MEM;
	}

	/* Configue the table so TLU reading/writing can be performed. */
	_tlu_table_config(tlu, table, TLU_TABLE_TYPE_IPTD, key_bits,
			kdata_size, 0, bank->index, crt->table_start);
	crt->empty = 1;

	crt->data_info = (struct crt_data_info *)buf;
	crt->data_entry_num = kdata_entry_num;
	crt->tmp_iptd_buf = (iptd_entry_t *)(((uint8_t *)buf)
			+ kdata_entry_num * sizeof(struct crt_data_info));
	/* Make it kdata_size aligned. kdata_size is always 2^n */
	crt->data_start = (size - kdata_size * kdata_entry_num)
		& (~(kdata_size - 1));
	crt->free_size = crt->data_start;
	if (crt->free_size < CRT_MEM_BLOCK_SIZE) {
		CRT_WARN("Total table size (%d) is too small to contain"
			" Key/Data table (%d) and at leaset one IPTD table\n",
				size, kdata_size * kdata_entry_num);
		_tlu_bank_mem_free(bank, crt->table_start);
		return TLU_INVALID_PARAM;
	}

	crt->tlu = tlu;
	crt->table = table;
	crt->key_bits = key_bits;
	crt->kdata_size = kdata_size;
	crt->bank = bank;
	crt->free_start = 0;
	crt->compress = compress;
	crt->max_iptd_bits = CRT_MAX_IPTD_BITS;
	crt->min_iptd_bits = CRT_MIN_IPTD_BITS;
	crt->max_iptd_levels = CRT_MAX_IPTD_LEVELS;

	/* Automatically set minimum IPTD bits based on key_bits
	 * and max_iptd_levels. */
	min_iptd_bits = (key_bits + crt->max_iptd_levels - 1)
		/ crt->max_iptd_levels;
	if (min_iptd_bits > crt->max_iptd_bits) {
		CRT_WARN("Key size (%d bits) is too big to fit for "
				"maximum %d IPTD levels",
				key_bits, crt->max_iptd_levels);
		_tlu_bank_mem_free(bank, crt->table_start);
		return TLU_INVALID_PARAM;
	}
	if (min_iptd_bits > crt->min_iptd_bits) {
		CRT_LOG("Minium IPTD size: %d bits\n", min_iptd_bits);
		crt->min_iptd_bits = min_iptd_bits;
	}

	CRT_LOG("[Memory Map] Total: %x  IPTD: %x %x  DATA: %x %x\n",
			size, crt->free_start, crt->free_size,
			crt->data_start, kdata_size * kdata_entry_num);
	crt->data_start /= kdata_size;
	crt->data_heap = tlu_heap_create_data(tlu, table, crt->data_start,
			kdata_entry_num);
	crt->free_data_count = kdata_entry_num;
	crt_data_info_init(crt);
	if (crt_mem_init(crt, crt->free_start, crt->free_size) < 0) {
		CRT_CRIT("Memory Initialization failed: start=%08x size=%d\n",
				crt->free_start, crt->free_size);
		_tlu_bank_mem_free(bank, crt->table_start);
		return TLU_MEM_ERROR;
	}

	return 0;
}

/*******************************************************************************
 * Description:
 *   Insert an entry into the location at cur_key of an IPTD table.
 * Parameters:
 *   crt         - The CRT table
 *   cur_entry   - The content of the entry which will be replaced by a new one.
 *   cur_base    - The base index of the IPTD table to be inserted
 *   cur_entropy - The entropy of the IPTD table
 *   cur_bits    - The bit size of the IPTD table
 *   cur_key     - The position of the entry to be replaced.
 *   bit_offset  - The bit offset of cur_key in the full key.
 *   cur_index   - The index of the entry in the physical table.
 *   prev_base   - The base of the parent table
 *   prev_index  - The base of the index of the parent entry.
 *   data_iptd_entry - The data entry to be inserted.
 *   new_entropy - The entropy of the table after insertion.
 * Return:
 *   >= 0 The base index of the table after insertion.
 *   < 0  Error code
 ******************************************************************************/
static int crt_insert_entry(struct crt *crt, iptd_entry_t *cur_entry,
		int cur_base, uint32_t cur_entropy, int cur_bits, int cur_key,
		int bit_offset, int cur_index, int prev_base, int prev_index,
		iptd_entry_t *data_iptd_entry, uint32_t *new_entropy)
{
	int type;
	int new_base;
	int sub_base;
	uint32_t sub_entropy;

	CRT_LOG("%s: [%x] <%08x %08x>\n", __func__, cur_key,
			cur_entry->words[0], cur_entry->words[1]);
	/* Case 2.1: No entry: Insert a new entry */
	type = iptd_get_etype(cur_entry);
	*new_entropy = cur_entropy;
	new_base = cur_base;
	if (type == IPTD_ETYPE_FAIL) {
		new_base = crt_modify_iptd(crt, cur_base, cur_entropy,
						cur_bits, cur_key,
						MODIFY_SINGLE, NULL,
						data_iptd_entry,
						prev_base + prev_index,
						crt->compress, new_entropy, 0);
		if (new_base < 0)
			return new_base;

	} else if (type == IPTD_ETYPE_DATA) {
		/* Case 2.2: A data entry: overwrite it if ok
		 * The entry is replaced only if it has less key bits than that
		 * of being inserted. If their key bits are same, they suppose
		 * to be same and no action on it. Thus the caller should check
		 * data reference counter to detect this case.
		 */
		if (crt_get_key_size(cur_entry)
				< crt_get_key_size(data_iptd_entry)) {
			/* It is the last entry of a key. Thus it can not be
			 * overwritten. So the enntry will not be inserted.
			 * An example of this case:
			 * Assume, the CRT table contains 10.10/16 and
			 * 10.10.0/24 ~ 10.10.254/24. The entry 10.10.255/24
			 * will not be inserted because it will cause 10.10/16
			 * being unresolvable.
			 */
#ifndef CRT_REDUNDANT
			if (crt_get_data_ref_count(crt,
					iptd_get_base(cur_entry)) > 1 ||
					_rundelta_get_entry_count(cur_entropy,
						cur_bits, cur_key) > 1) {
#endif
				new_base = crt_modify_iptd(crt, cur_base,
						cur_entropy, cur_bits, cur_key,
						MODIFY_SINGLE, NULL,
						data_iptd_entry,
						prev_base + prev_index,
						crt->compress, new_entropy, 0);
				if (new_base < 0)
					return new_base;

#ifndef CRT_REDUNDANT
			} else {
				CRT_LOG("%s: Attempt to insert a duplicate "
						"entry\n", __func__);
				return TLU_REDUNDANT_ENTRY;
			}
#endif
		}
	} else if (type == IPTD_ETYPE_SIMPLE || type == IPTD_ETYPE_RUNDELTA) {
		/* Case 2.3: A table: Walkthrough the tree and fill all FAIL
		 * entries with a new data entry
		 */
		sub_base = crt_insert_rundelta_data_wild(crt,
				iptd_get_base(cur_entry),
				iptd_get_entropy(cur_entry),
				iptd_get_bits(cur_entry), data_iptd_entry,
				cur_base, cur_index, crt->compress,
				&sub_entropy);
		if (sub_base < 0)
			return sub_base;

	} else {
		CRT_CRIT("Unsupported etype: %d\n", type);
		return TLU_TABLE_ERROR;
	}
	return new_base;
}

/* See description in the header file */
int crt_free(struct crt *crt)
{
	return _tlu_bank_mem_free(crt->bank, crt->table_start);
}

/*******************************************************************************
 * Description:
 *   Delete a range of entries from the table
 * Parameters:
 *   crt	 - The CRT table
 *   cur_base    - The base index of the IPTD table to be inserted
 *   cur_bits    - The bit size of the IPTD table
 *   cur_entropy - The entropy of the IPTD table
 *   bit_offset  - The bit offset of cur_key in the full key.
 *   bits	 - The bit size of the partial key. It may smaller than
 *                 cur_bits.
 *   cur_key     - The position of the entry to be replaced.
 *   key	 - The full key
 *   key_bits    - The size of the full key in bits
 *   prev_base   - The base of the parent table
 *   prev_index  - The base of the index of the parent entry.
 *   trace_stack - The trace stack of the searching path
 *   trace_num   - The entry number in the trace stack
 * Return:
 *   >= 0 The base index of the table after insertion.
 *   < 0  Error code
 ******************************************************************************/
int crt_delete_range(struct crt *crt, int cur_base, int cur_bits,
		uint32_t cur_entropy, int bit_offset, int bits, int cur_key,
		void *key, int key_bits, int prev_base, int prev_index,
		struct crt_trace *trace_stack, int trace_num)
{
	iptd_entry_t replacement;
	int rc;
	int recursive_count;
	int data_index;
	int entry_num;

	CRT_LOG("%s: base = %x entropy = %08x bits = %d cur_key_bits = %d "
			"key_bits = %d"
			" key = %x\n", __func__, cur_base, cur_entropy,
			cur_bits, bits, key_bits, cur_key);
#ifdef CRT_STORE_KEY
	rc = crt_find_replacement(crt, key, key_bits, &replacement);
	if (rc < 0)
		return rc;

#else
	rc = crt_find_replacement(crt, cur_base, cur_bits, cur_entropy,
					key_bits, bit_offset, cur_key,
					trace_stack, trace_num, &replacement);
	if (rc < 0)
		return rc;

#endif
	cur_key <<= cur_bits - bits;
	entry_num = 1 << (cur_bits - bits);
	recursive_count = 0;
	data_index = crt_delete_iptd_entries(crt, cur_base, cur_bits,
					cur_entropy, key_bits, cur_key,
					cur_key + entry_num - 1,
					&replacement, prev_base, prev_index,
					trace_stack, trace_num, &cur_base,
					&cur_entropy, &recursive_count);
	if (data_index < 0)
		return data_index;

#ifdef CRT_REDUNDANT
	if (data_index == 0) {
		/* The entry is not in the hardware table. It might be
		 * in the software table.
		 */
		data_index = crt_data_info_delete(crt, key, key_bits);
	}
#endif
	if (data_index == 0)
		return TLU_ENTRY_NOT_EXIST;

	/* Destroy the data */
	rc = crt_destroy_data(crt, data_index);
	if (rc < 0)
		return rc;

	/* Debug check */
	if (crt_get_data_ref_count(crt, data_index) != 0) {
		CRT_CRIT("Data [%x] is not cleaned: %d\n", data_index,
				crt_get_data_ref_count(crt, data_index));
		return TLU_TABLE_ERROR;
	}
	if (cur_base == 0) {
		return crt_clean_super_redundant_iptd(crt, key_bits,
				trace_stack, trace_num);
	}
	return 0;
}

/*******************************************************************************
 * Description:
 *   Delete a range of entries from the table
 * Parameters:
 *   crt          - The CRT table
 *   cur_base     - The base index of the IPTD table to be inserted
 *   cur_entropy  - The entropy of the IPTD table
 *   cur_bits  	  - The bit size of the IPTD table
 *   cur_key	  - The start position of the entry to be replaced. It has been
 *                  left shifted to align with cur_bits.
 *   entry_num    - Total number of entries to be replaced/inserted.
 *   bit_offset   - The bit offset of cur_key in the full key.
 *   prev_base    - The base of the parent table
 *   prev_index   - The base of the index of the parent entry.
 *   data	  - The content of data to be inserted
 *   key	  - The full key
 *   key_bits	  - The size of the full key in bits
 * Return:
 *   >= 0 The index of the inserted data entry
 *   < 0  Error code
 ******************************************************************************/
static int crt_insert_range(struct crt *crt, int cur_base, uint32_t cur_entropy,
		int cur_bits, int cur_key, int entry_num, int bit_offset,
		int prev_base, int prev_index, void *data, void *key,
		int key_bits)
{
	iptd_entry_t data_entry;
	iptd_entry_t cur_entry;
	int data_index, cur_index;
	int i, rc, new_base;
	uint32_t new_entropy;

	CRT_LOG("%s: base = %x entropy = %x bits = %x key = %x num = %d "
			"bit_offset = %d "
		   "prev_base = %x prev_index = %x\n",
		__func__, cur_base, cur_entropy, cur_bits, cur_key, entry_num,
		bit_offset, prev_base, prev_index);
	data_index = crt_create_data(crt, data, key, key_bits);
	if (data_index < 0)
		return data_index;

	crt_fill_iptd_data_entry(&data_entry, data_index, key_bits);

	for (i = 0; i < entry_num; i++) {
		cur_index = crt_get_rundelta_index(cur_entropy, cur_bits,
				cur_key + i);
		if (crt_get_iptd_entry(crt, cur_base + cur_index, &cur_entry)
				== NULL) {
			return TLU_MEM_ERROR;
		}

		new_base = crt_insert_entry(crt, &cur_entry, cur_base,
						cur_entropy, cur_bits,
						cur_key + i, bit_offset,
						cur_index, prev_base,
						prev_index, &data_entry,
						&new_entropy);

		if (new_base < 0) {
			/* Fail: Clean up if some data is already inserted */
			if (crt_get_data_ref_count(crt, data_index) > 0) {
				/* Data will be destroied */
				rc = crt_delete_range(crt, cur_base,
						cur_bits, cur_entropy,
						bit_offset,
						key_bits - bit_offset,
						cur_key >>
						(cur_bits -
						 (key_bits - bit_offset)),
						key, key_bits, prev_base,
						prev_index, NULL, 0);
				if (rc < 0)
					return rc;

			} else {
				crt_destroy_data(crt, data_index);
			}
			return new_base;
		}
		cur_entropy = new_entropy;
		cur_base = new_base;
	}
	/* Nothing inserted it must be exist */
	if (crt_get_data_ref_count(crt, data_index) == 0) {
		/* Allow none-duplicate keys in CRT_REDUNDANT mode */
#ifdef CRT_REDUNDANT
		if (crt_key_search(crt, key, key_bits, data_index) == 0)
			return data_index;

#endif
		crt_destroy_data(crt, data_index);
		return TLU_ENTRY_EXIST;
	}
	return data_index;
}

static int crt_determine_iptd_size(struct crt *crt, int bits, int bit_offset)
{
	int new_bits;

	new_bits = bits <= crt->max_iptd_bits ?
			bits : crt->max_iptd_bits;
	new_bits = new_bits >= crt->min_iptd_bits ?
		new_bits : crt->min_iptd_bits;
	/* Make sure it does not exceed
	   total key length */
	if (new_bits > crt->key_bits - bit_offset)
		new_bits = crt->key_bits - bit_offset;

	return new_bits;
}

/* See description in the header file */
/*******************************************************************************
 *               prev_bits
 *   prev_base--> + ---------- +
 *               |          |
 *               |          |                 cur_bits
 *               + ---------- +                cur_entropy
 *  prev_index-->|          |-->cur_base---> + ---------- +
 *               + ---------- +               |          |
 *               |          |               |          |
 *               |          |               + ---------- +
 *               + ---------- +  cur_index--->|cur_entry |
 *                                          + ---------- +
 *                                          |          |
 *                                          |          |
 *                                          + ---------- +
 *****************************************************************************/
int crt_insert(struct crt *crt, void *key, int key_bits, void *data)
{
	int cur_base;
	int prev_index, cur_entropy, cur_index, cur_key;
	iptd_entry_t *cur_entry, tmp_entry;
	int type, bit_offset;
	int bits;	   /* The remaining bit number of the key */
	int cur_bits;   /*The current table bit number */
	int prev_base;
	int index;

	CRT_LOG_MEM(key, (key_bits + 7) / 8, "INSERT [%d]: ", key_bits);
	CRT_LOG_MEM(data, crt->kdata_size, NULL);

	/* Create the initial IPTD table if the CRT is empty */
	bits = key_bits;
	if (crt_is_empty(crt)) {
		CRT_LOG("Create Initial IPTD table\n");
		crt->empty = 0;
		cur_bits = crt_determine_iptd_size(crt, bits, 0);
		_tlu_table_config(crt->tlu, crt->table,
				TLU_TABLE_TYPE_IPTD, crt->key_bits,
				crt->kdata_size, cur_bits - 1,
				crt->bank->index, crt->table_start);
		cur_base = crt_alloc_iptd(crt, cur_bits, NULL, 0);
		if (cur_base < 0)
			return cur_base;

	} else {
		cur_base = 0;
		cur_bits = crt_get_init_table_bits(crt);
	}
	/* The initial IPTD table is never compressed. */
	cur_entropy = 0;
	cur_entry = NULL;
	prev_index = 0;
	prev_base = 0;
	bit_offset = 0;

	while (1) {
		CRT_LOG("bit_offset = %d  bits = %d  cur_bits = %d base = %x "
				"entropy = %08x\n", bit_offset, bits,
				cur_bits, cur_base, cur_entropy);
		if (cur_bits > crt->max_iptd_bits) {
			CRT_CRIT("Table error: IPTD size %d is larger "
					"than the maximum %d\n",
					cur_bits, crt->max_iptd_bits);
			return TLU_TABLE_ERROR;
		}
		/* Case 1: current bits is less than the table bits.
		 * A new IPTD sub table is needed.
		 */
		if (bits <= cur_bits) {
			cur_key = crt_get_key_prefix(key, bit_offset, bits)
				<< (cur_bits - bits);
			CRT_LOG("bit_offset=%d bits=%d cur_bits=%d base=%x "
				"entropy=%08x key=%x\n",
				bit_offset, bits, cur_bits, cur_base,
				cur_entropy, cur_key);
			/* Case 2: The current bits fit in the current IPTD
			 * table. Action based on entry's ETYPE.
			 */

#ifdef CRT_SPLIT_ENABLE
			if (bits == cur_bits || cur_entry == NULL) {
#endif
				index = crt_insert_range(crt, cur_base,
						cur_entropy, cur_bits, cur_key,
						1 << (cur_bits - bits),
						bit_offset, prev_base,
						prev_index, data, key,
						key_bits);
				if (index < 0)
					return index;

				return index - crt->data_start;
#ifdef CRT_SPLIT_ENABLE
			}
			/* Split the current IPTD table into multiple
			 * sub-tables.
			 */
			cur_base = crt_split_rundelta(crt, cur_base,
					cur_entropy, cur_bits, bits,
					prev_base + prev_index, bit_offset,
					crt->compress, &cur_entropy);
			if (cur_base < 0)
				return cur_base;

			cur_bits = bits;
			/* It can be move forward as usual now. */
#endif
		} else { /* bits > cur_bits */
			/* Case 3.1: Insert a new iptd table with ETYPE FAIL. */
			/* Case 3.2 Insert a new iptd table pointing to the
			 * data entry
			 */
			cur_key = crt_get_key_prefix(key, bit_offset, cur_bits);
			cur_index = crt_get_rundelta_index(cur_entropy,
					cur_bits, cur_key);
			cur_entry = crt_get_iptd_entry(crt,
					cur_base + cur_index, &tmp_entry);
			if (cur_entry == NULL)
				return TLU_MEM_ERROR;

			CRT_LOG("key = %x index = %x Entry = %08x %08x\n",
					cur_key, cur_index, cur_entry->words[0],
					cur_entry->words[1]);
			type = iptd_get_etype(cur_entry);
			if (type == IPTD_ETYPE_FAIL ||
					type == IPTD_ETYPE_DATA) {
				int new_bits;
				uint32_t new_entropy;
				iptd_entry_t *fill_entry;

				fill_entry = NULL;
				if (type == IPTD_ETYPE_DATA)
					fill_entry = cur_entry;

				bits -= cur_bits;
				bit_offset += cur_bits;
				new_bits = crt_determine_iptd_size(crt, bits,
								bit_offset);
				prev_base = crt_insert_iptd(crt,
						cur_base, cur_entropy,
						cur_bits, cur_key,
						new_bits, fill_entry,
						prev_base, prev_index,
						crt->compress,
						&cur_entropy, &cur_base,
						&new_entropy);
				if (prev_base < 0)
					return prev_base;

				/* Recalculate the index because entropy may
				 * has been changed
				 */
				prev_index = crt_get_rundelta_index(new_entropy,
						cur_bits, cur_key);
				CRT_LOG("prev_index = %x new_entropy = %08x "
						"bits = %d key = %x\n",
						prev_index, new_entropy,
						cur_bits, cur_key);
				cur_bits = new_bits;
			} else if (type == IPTD_ETYPE_SIMPLE
					|| type == IPTD_ETYPE_RUNDELTA) {
				/* Case 3.3: An IPTD table: Forward to the next
				 * round check
				 */
				prev_base = cur_base;
				bits -= cur_bits;
				bit_offset += cur_bits;
				cur_bits = iptd_get_bits(cur_entry);
				cur_base = iptd_get_base(cur_entry);
				cur_entropy = iptd_get_entropy(cur_entry);
				prev_index = cur_index;
			} else {
				CRT_CRIT("Unsupported etype: %x\n", type);
				return TLU_TABLE_ERROR;
			}
		}
	}
	return 0;
}

/* See description in the header file */
int crt_delete(struct crt *crt, void *key, int key_bits)
{
	int cur_base, cur_bits, cur_index, cur_key;
	int prev_base, prev_index;
	iptd_entry_t *cur_entry, tmp_entry;
	uint32_t cur_entropy;
	int type, bit_offset;
	int bits;
	struct crt_trace trace_stack[CRT_MAX_IPTD_LEVELS];
	int trace_num;

	CRT_LOG_MEM(key, (key_bits + 7) / 8, "DELETE [%d]: ", key_bits);
	if (crt->empty)
		return TLU_ENTRY_NOT_EXIST;

	trace_num = 0;
	bits = key_bits;
	bit_offset = 0;
	cur_bits = crt_get_init_table_bits(crt);
	/* The initial IPTD table always starts at 0 */
	cur_base = 0;
	cur_entropy = 0;
	prev_base = 0;
	prev_index = 0;
	while (1) {
		CRT_LOG("base = %x bits = %d  cur_bits = %d entropy = %08x\n",
				cur_base, bits, cur_bits, cur_entropy);
		/* Case 1 & 2 */
		if (bits <= cur_bits) {
			int index;
			cur_key = crt_get_key_prefix(key, bit_offset, bits);
			index = crt_delete_range(crt, cur_base, cur_bits,
					cur_entropy, bit_offset, bits, cur_key,
					key, key_bits, prev_base, prev_index,
					trace_stack, trace_num);
			if (index < 0)
				return index;

			return 0;
		} else {
			/* Case 3 */
			/* bits > cur_bits */
			cur_key = crt_get_key_prefix(key, bit_offset, cur_bits);
			cur_index = crt_get_rundelta_index(cur_entropy,
					cur_bits, cur_key);
			cur_entry = crt_get_iptd_entry(crt,
					cur_base + cur_index, &tmp_entry);
			if (cur_entry == NULL)
				return TLU_MEM_ERROR;

			type = iptd_get_etype(cur_entry);
			/* Case 3.1 */
			if (type == IPTD_ETYPE_FAIL ||
					type == IPTD_ETYPE_DATA) {
				CRT_LOG("Entry does not exist: bits = %d "
						"cur_bits = %d type = %d\n",
						bits, cur_bits, type);
				return TLU_ENTRY_NOT_EXIST;
			} else if (type == IPTD_ETYPE_SIMPLE
					|| type == IPTD_ETYPE_RUNDELTA) {
				/* Case 3.2 */
				trace_num = crt_trace_push(crt, trace_stack,
						trace_num, cur_base, cur_index,
						cur_bits, cur_entropy);
				if (trace_num < 0)
					return trace_num;

				prev_base = cur_base;
				prev_index = cur_index;
				bits -= cur_bits;
				bit_offset += cur_bits;
				cur_bits = iptd_get_bits(cur_entry);
				cur_base = iptd_get_base(cur_entry);
				cur_entropy = iptd_get_entropy(cur_entry);
				continue;
			} else {
				CRT_CRIT("Unsupported etype: %d\n", type);
				return TLU_TABLE_ERROR;
			}
		}
	}
}

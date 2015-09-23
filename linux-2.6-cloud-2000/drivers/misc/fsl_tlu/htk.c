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
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Wed Apr 11 2007
 *
 * Description:
 * This file contains an implementation of Hash-Trie_Key (HTK) table management
 * functions.
 */
#include <asm/htk.h>

/* Management of data heap and trie heap */
static inline int32_t htk_alloc_data_entry(struct htk *htk)
{
	return tlu_heap_alloc_data(htk->tlu, htk->table, &htk->data_heap);
}

static inline int32_t htk_free_data_entry(struct htk *htk, int32_t entry)
{
	return tlu_heap_free_data(htk->tlu, htk->table, &htk->data_heap, entry);
}

static inline int htk_data_heap_empty(struct htk *htk)
{
	return (tlu_heap_peek(&htk->data_heap) == TLU_HEAP_END);
}

static inline int32_t htk_alloc_trie_entry(struct htk *htk)
{
	return tlu_heap_alloc(htk->tlu, htk->table, &htk->trie_heap);
}

static inline int32_t htk_free_trie_entry(struct htk *htk, int32_t entry)
{
	return tlu_heap_free(htk->tlu, htk->table, &htk->trie_heap, entry);
}

static inline int htk_trie_heap_empty(struct htk *htk)
{
	return (tlu_heap_peek(&htk->trie_heap) == TLU_HEAP_END);
}

/* read/write functions for various objects */
static inline void *htk_read(struct htk *htk, int index, int len, void *entry)
{
	return _tlu_read(htk->tlu, htk->table, index, len, entry);
}

static inline int htk_write(struct htk *htk, int index, void *data, int len)
{
	return _tlu_write(htk->tlu, htk->table, index, len, data);
}

/* Entry can not be NULL if len > 32 */
static inline void *htk_read_data(struct htk *htk, int index, int len,
		void *entry)
{
	return _tlu_read_data(htk->tlu, htk->table, index, 0, len, entry);
}

static inline int htk_write_data(struct htk *htk, int index, void *data,
		int len)
{
	return _tlu_write_data(htk->tlu, htk->table, index, 0, len, data);
}

static inline void *htk_read_trie_entry(struct htk *htk, int index, void *entry)
{
	return htk_read(htk, index, sizeof(struct tlu_trie_entry), entry);
}

static inline int htk_write_trie_entry(struct htk *htk, int index,
	struct tlu_trie_entry *entry)
{
	return htk_write(htk, index , entry, sizeof(struct tlu_trie_entry));
}

static inline int htk_write_node(struct htk *htk, int index,
		struct tlu_node *node)
{
	return _tlu_write_byte(htk->tlu, htk->table,
			TLU_NODE_INDEX_TO_ADDR(index), TLU_NODE_SIZE, node);
}

static inline int htk_hash(struct htk *htk, void *key)
{
	return tlu_hash(key, htk->key_bits >> 5, htk->hash_bits);
}

static inline void htk_build_data_node(struct htk *htk, struct tlu_node *node,
	int data_index)
{
	tlu_build_node(node, TLU_NODE_FLAG_DATA, 0, data_index);
}

/*******************************************************************************
 * Description:
 *   This function initializes a hash table which is used by an HTK table.
 * Parameters:
 *   tlu       - The TLU handle
 *   table     - The table ID
 *   hash_bits - The size of the hash table in bits
 * Return:
 *   0 is returned indicating success
 ******************************************************************************/
static int tlu_init_hash_table(tlu_handle_t tlu, int table, int hash_bits)
{
	int i, rc;
	struct tlu_hash_entry entry;

	tlu_memset32(&entry, 0, sizeof(struct tlu_hash_entry));
	for (i = 0; i < (1 << (hash_bits - 1)); i++) {
		rc = _tlu_write(tlu, table, i,
				sizeof(struct tlu_hash_entry), &entry);
		if (rc < 0)
			return rc;

	}

	return 0;
}

/*******************************************************************************
 * Description:
 *   This function loads a key from a trie node. It walks down to any data entry
 *   from the node then load the key from it.
 * Parameters:
 *   htk    - The htk table the trie node belongs to.
 *   trie   - The trie node.
 * Return:
 *   Address pointing to the key data if it is success. Otherwise NULL is
 *   returned if fail.
 ******************************************************************************/
static void *htk_load_key(struct htk *htk, struct tlu_trie_entry *trie)
{
	while (1) {
		/* Both nodes should never be empty */
		if (!tlu_node_get_link(&trie->nodes[0])
				|| !tlu_node_get_link(&trie->nodes[1])) {
			HTK_CRIT("Invalid trie: %08x %08x\n",
					*(uint32_t *)&trie->nodes[0],
					*(uint32_t *)&trie->nodes[1]);
			return NULL;
		}

		if (tlu_node_link_is_data(trie->nodes)) {
			return htk_read_data(htk,
				tlu_node_get_link(&trie->nodes[0]),
				htk->key_bits >> 3, NULL);
		} else if (tlu_node_link_is_data(trie->nodes + 1)) {
			return htk_read_data(htk,
				tlu_node_get_link(&trie->nodes[1]),
				htk->key_bits >> 3, NULL);
		}
		/* Get the trie node */
		trie = htk_read(htk, tlu_node_get_link(&trie->nodes[0]),
				sizeof(struct tlu_trie_entry), NULL);
		if (trie == NULL)
			return NULL;

	}
}

/*******************************************************************************
 * Description:
 *   This function inserts a trie between a super node and two other nodes (two
 *   data entries or one data entry and one trie node).
 * Parameters:
 *   htk	- The htk table to be inserted.
 *   super_node_index - The node index of the super node.
 *   super_node	- The super node
 *   data   	- The data content of the the data entry
 *   count  	- The location of the diff bit
 * Return:
 *   The index of the inserted data entry if success. Otherwise an error is
 *   return.
*******************************************************************************/
static int htk_insert_trie(struct htk *htk, int super_node_index,
		struct tlu_node *super_node, void *data, int count)
{
	int index;
	int bit;
	struct tlu_trie_entry trie;
	struct tlu_node node;
	int err;
	int data_index;

	data_index = htk_alloc_data_entry(htk);

	if (data_index < 0) {
		HTK_LOG("Table full\n");
		return TLU_TABLE_FULL;
	}
	/* Write data */
	err = htk_write_data(htk, data_index, data, htk->kdata_size);
	if (err < 0) {
		htk_free_data_entry(htk, data_index);
		return err;
	}

	/* Allocate a trie entry */
	index = htk_alloc_trie_entry(htk);
	if (index < 0) {
		htk_free_data_entry(htk, data_index);
		HTK_LOG("Out of trie entries\n");
		return TLU_OUT_OF_MEM;
	}
	/* Fill and write the trie entry */
	bit = tlu_get_bit(data, count);
	htk_build_data_node(htk, trie.nodes + bit, data_index);
	tlu_memcpy32(&trie.nodes[bit ^ 1], super_node, sizeof(struct tlu_node));
	err = htk_write_trie_entry(htk, index, &trie);
	if (err < 0) {
		htk_free_data_entry(htk, data_index);
		htk_free_trie_entry(htk, index);
		return err;
	}

	/* Update the super node */
	tlu_build_node(&node, TLU_NODE_FLAG_TRIE, count, index);
	err = htk_write_node(htk, super_node_index, &node);
	if (err < 0) {
		htk_free_data_entry(htk, data_index);
		htk_free_trie_entry(htk, index);
		return err;
	}
	return data_index;
}

/*******************************************************************************
 * Description:
 *   This function inserts a data entry when it collides with the one already
 *   exists.
 * Parameters:
 *   htk	- The htk table to be inserted.
 *   node_index - The index of the node which is collides with the inserting
 * 		  one.
 *   node   	- The node which is collides with the insertinf one.
 *   entry  	- The data content of the the data entry
 * Return:
 *   The index of the inserted data entry if success. Otherwise an error is
 *   return.
 ******************************************************************************/
static int htk_insert_collision(struct htk *htk, int node_index,
		struct tlu_node *node, void *entry)
{
	void *key;
	struct tlu_trie_entry trie;
	int count, bit, index;
	struct tlu_node tmp_node;

	while (1) {
		/* Keep it in local memory */
		tlu_memcpy32(&tmp_node, node, sizeof(struct tlu_node));

		/* Case 1: Collision with a data entry */
		if (tlu_node_link_is_data(&tmp_node)) {
			HTK_LOG("Collision with data\n");
			key = htk_read_data(htk,
					tlu_node_get_link(&tmp_node),
					htk->key_bits >> 3, NULL);
			if (key == NULL)
				return TLU_MEM_ERROR;

			count = tlu_bit_diff(entry, key, htk->key_bits >> 3);
			if ((count  == htk->key_bits)) {
				HTK_LOG("Entry exist\n");
				return TLU_ENTRY_EXIST;
			}
			return htk_insert_trie(htk, node_index, &tmp_node,
					entry, count);
		}
		/* Case 2, 3: Collision with a trie entry */
		/* Get the next node */
		index = tlu_node_get_link(&tmp_node);
		if (htk_read(htk, index, sizeof(struct tlu_trie_entry), &trie)
				== NULL) {
			return TLU_MEM_ERROR;
		}
		/* Get the key content */
		key = htk_load_key(htk, &trie);
		if (key == NULL)
			return TLU_MEM_ERROR;

		HTK_LOG_MEM(key, htk->key_bits >> 3, NULL);

		/* Case 2: A new trie node should be inserted here if the diff
		 * bit location is earlier
		 */
		count = tlu_bit_diff(entry, key, htk->key_bits >> 3);
		if ((count  < tlu_node_get_count(&tmp_node))) {
			return htk_insert_trie(htk, node_index, &tmp_node,
					entry, count);
		}
		/* Case 3: Keep search the next entry */
		bit = tlu_get_bit(entry, tlu_node_get_count(&tmp_node));
		node = &trie.nodes[bit];
		node_index = (index << 1) | bit;
	}
}

/* Descriptions in the header file */
int htk_create(struct htk *htk, tlu_handle_t tlu, int table, int size,
		int key_bits, int kdata_size, int kdata_entry_num,
		int hash_bits, struct tlu_bank_mem *bank)
{
	int data_start, trie_start;
	uint32_t data_table_size, hash_table_size;
	int rc;

	HTK_LOG("%s: table=%d size=%d key_bits=%d kdata_size=%d entries=%d "
			"hash_bits = %d\n", __func__, table, size, key_bits,
			kdata_size, kdata_entry_num, hash_bits);
	if (size == 0) {
		/* Estimate the total required memory size based on
		 * assumption that maximum required trie node number is
		 * equal to key-data entry number.
		 */
		size = ((((1 << hash_bits) + kdata_entry_num)*(TLU_UNIT_SIZE/2)
				+ kdata_size - 1) & (~(kdata_size - 1)))
			+ kdata_size * kdata_entry_num;
		HTK_LOG("Total table size = %d\n", size);

	}

	/* Perform parameter checks */
	if (table >= TLU_MAX_TABLE_NUM) {
		HTK_WARN("Invalid table ID: %d. A valid value must be between "
				"0 and 31.\n", table);
		return TLU_INVALID_PARAM;
	}
	if (key_bits != 32 && key_bits != 64 && key_bits != 96
			&& key_bits != 128) {
		HTK_WARN("Invalid key size: %d. A valid value must be 32, 64, "
				"96 or 128.\n", key_bits);
		return TLU_INVALID_PARAM;
	}
	if (kdata_size != 8 && kdata_size != 16 && kdata_size != 32
			&& kdata_size != 64) {
		HTK_WARN("Invalid key-data size: %d. A valid value must be 8, "
				"16, 32, 64\n", kdata_size);
		return TLU_INVALID_PARAM;
	}
	if (hash_bits <= 0 || hash_bits > 16) {
		HTK_WARN("Invalid hash bit size: %d. A valid value must be "
				"between 1 and 16 (inclusive)\n", hash_bits);
		return TLU_INVALID_PARAM;
	}
	if (key_bits / 8 > kdata_size) {
		HTK_WARN("Key size (%d bits) is bigger than key-data size "
				"(%d bytes)\n", key_bits, kdata_size);
		return TLU_INVALID_PARAM;
	}
	if (kdata_entry_num < 1) {
		HTK_WARN("key-data entry number (%d) must be greater than 0\n",
				kdata_entry_num);
		return TLU_INVALID_PARAM;
	}

	htk->bank = bank;
	htk->table_start = _tlu_bank_mem_alloc(bank, size);
	if (htk->table_start < 0) {
		HTK_WARN("Out of bank memory. size = %d\n", size);
		return TLU_OUT_OF_MEM;
	}
	_tlu_table_config(tlu, table, TLU_TABLE_TYPE_HASH, key_bits,
			kdata_size, hash_bits, bank->index, htk->table_start);

	data_table_size = kdata_size * kdata_entry_num;
	hash_table_size = (1 << hash_bits) * sizeof(struct tlu_node);
	/* Make it kdata_size aligned. kdata_size is always 2^n */
	data_start = (size - data_table_size) & (~(kdata_size - 1));
	trie_start = hash_table_size;
	if (data_start <= trie_start) {
		HTK_WARN("Total table size (%d) is too small to contain "
				"Key/Data table (%d) and hash table(%d)."
				"\nkdata size = %d, kdata entry = %d "
				"data_start = %x trie_start = %x\n",
				size, data_table_size, hash_table_size,
				kdata_size, kdata_entry_num,
				data_start, trie_start);
		_tlu_bank_mem_free(bank, htk->table_start);
		return TLU_INVALID_PARAM;
	}

	htk->tlu = tlu;
	htk->table = table;
	htk->hash_bits = hash_bits;
	htk->key_bits = key_bits;
	htk->kdata_size = kdata_size;
	htk->data_start = data_start/kdata_size;
	HTK_LOG("Create kdata heap: %d\n", kdata_entry_num);
	htk->data_heap = tlu_heap_create_data(tlu, table,
				htk->data_start, kdata_entry_num);
	if (htk->data_heap < 0) {
		HTK_CRIT("Failed to create data heap\n");
		_tlu_bank_mem_free(bank, htk->table_start);
		return TLU_MEM_ERROR;
	}
	HTK_LOG("Trie heap size: %d\n",
		(data_start - trie_start) / sizeof(struct tlu_trie_entry));
	htk->trie_heap = tlu_heap_create(tlu, table,
			TLU_ADDR_TO_INDEX(trie_start),
			(data_start - trie_start)
			/ sizeof(struct tlu_trie_entry),
			sizeof(struct tlu_trie_entry) / TLU_UNIT_SIZE);
	if (htk->trie_heap < 0) {
		HTK_CRIT("Failed to create trie heap\n");
		_tlu_bank_mem_free(bank, htk->table_start);
		return TLU_MEM_ERROR;
	}

	HTK_LOG("Init hash table\n");
	rc = tlu_init_hash_table(tlu, table, hash_bits);
	if (rc < 0)
		_tlu_bank_mem_free(bank, htk->table_start);

	return rc;
}

/* Descriptions in the header file */
int htk_free(struct htk *htk)
{
	return _tlu_bank_mem_free(htk->bank, htk->table_start);
}

/* Descriptions in the header file */
int htk_insert(struct htk *htk, void *entry)
{
	int hash_index;
	struct tlu_hash_entry *hash_entry;
	struct tlu_node *node;
	int err;
	int data_index;

	HTK_LOG_MEM(entry, htk->kdata_size, "INSERT\n");

	if (htk_data_heap_empty(htk)) {
		HTK_LOG("Table full\n");
		return TLU_TABLE_FULL;
	}

	hash_index = htk_hash(htk, entry);
	HTK_LOG("hash_index: %x/%d\n", hash_index >> 1, hash_index & 1);
	hash_entry = htk_read_trie_entry(htk, hash_index >> 1, NULL);
	if (hash_entry == NULL)
		return TLU_MEM_ERROR;

	node = &hash_entry->nodes[hash_index & 1];
	/* No collision */
	if (tlu_node_get_link(node) == 0) {
		struct tlu_node tmp_node;

		/* No need to check return value any more because it is
		 * already check before.
		 */
		data_index = htk_alloc_data_entry(htk);
		err = htk_write_data(htk, data_index, entry,
						htk->kdata_size);
		if (err < 0) {
			htk_free_data_entry(htk, data_index);
			return err;
		}
		htk_build_data_node(htk, &tmp_node, data_index);
		err = htk_write_node(htk, hash_index, &tmp_node);
		if (err < 0) {
			htk_free_data_entry(htk, data_index);
			return err;
		}
		return data_index - htk->data_start;
	} else {
		HTK_LOG_MEM(hash_entry, sizeof(struct tlu_hash_entry),
				"Collision\n");
		data_index = htk_insert_collision(htk, hash_index, node,
						entry);
		if (data_index < 0)
			return data_index;

		return data_index - htk->data_start;
	}
}

int htk_delete(struct htk *htk, void *key)
{
	int trie_index, node_index, prev_node, rc, link;
	struct tlu_trie_entry cur_trie;
	struct tlu_node *node;
	int count;
	void *data_key;

	HTK_LOG_MEM(key, htk->key_bits >> 3, "DELETE\n");

	count = 0;
	prev_node = -1;
	trie_index = htk_hash(htk, key);
	node_index = trie_index & 1;
	trie_index >>= 1;
	while (++count <= HTK_MAX_TRIE_LEVELS) {
		HTK_LOG("trie_index: %x/%d  prev_node: %x/%d\n",
				trie_index, node_index, prev_node >> 1,
				prev_node & 1);
		if ((htk_read_trie_entry(htk, trie_index, &cur_trie))
				== NULL) {
			return TLU_MEM_ERROR;
		}
		HTK_LOG("Node: %08x\n", cur_trie.nodes[node_index]);
		node = &cur_trie.nodes[node_index];
		/* Empty link */
		link = tlu_node_get_link(node);
		if (link == 0)
			return TLU_ENTRY_NOT_EXIST;

		/* Data link */
		if (tlu_node_link_is_data(node)) {
			data_key = htk_read_data(htk, link,
					htk->key_bits >> 3, NULL);
			if (data_key == NULL)
				return TLU_MEM_ERROR;

			if (tlu_memcmp32(key, data_key, htk->key_bits >> 3)) {
				HTK_LOG("Key does not exist\n");
				return TLU_ENTRY_NOT_EXIST;
			}
			if (prev_node >= 0) {
				/* Replace the previous node by another node
				 * of the current trie.
				 */
				rc = htk_write_node(htk, prev_node,
					&cur_trie.nodes[node_index ^ 1]);
				if (rc < 0)
					return rc;
				rc = htk_free_trie_entry(htk, trie_index);
				if (rc < 0)
					return rc;

			} else {
				tlu_build_node(cur_trie.nodes, 0, 0, 0);
				rc = htk_write_node(htk, (trie_index << 1) |
						node_index, cur_trie.nodes);
				if (rc < 0)
					return rc;
			}
			/* free data entry */
			rc = htk_free_data_entry(htk, link);
			if (rc < 0)
				return rc;

			return 0;
		}
		/* Trie link */
		prev_node = (trie_index << 1) | node_index;
		trie_index = link;
		node_index = tlu_get_bit(key, tlu_node_get_count(node));
	}
	HTK_CRIT("Chain too long\n");
	return TLU_CHAIN_TOO_LONG;
}

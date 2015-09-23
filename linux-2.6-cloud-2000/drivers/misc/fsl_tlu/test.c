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
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Wed Sep 19 2007
 *
 * Description:
 * This file contains functions to test both the TLU hardware and the driver.
 */
#include <linux/slab.h>
#include "test.h"

#define TEST_LOG(format, ...) \
	TLU_LOG(TLU_LOG_TEST, "TEST", format, ##__VA_ARGS__)
#define TEST_LOG_MEM(buf, len, format, ...) \
	TLU_LOG_MEM(TLU_LOG_TEST, buf, len, "TEST", format, ##__VA_ARGS__)
#define TEST_CRIT(format, ...) \
	TLU_LOG(TLU_LOG_CRIT, "TEST", format, ##__VA_ARGS__)
#define TEST_WARN(format, ...) \
	TLU_LOG(TLU_LOG_WARN, "TEST", format, ##__VA_ARGS__)

#ifdef TLU_DEBUG
#define TEST_PRINT(format, ...) \
	do { printk(format, ##__VA_ARGS__); printk(KERN_INFO "\n"); } while (0)
#else
#define TEST_PRINT printk
#endif

static inline int memcmp32(void *s1, void *s2, int len)
{
	int i;

	for (i = 0; i < len/4; i++) {
		if (((uint32_t *)s1)[i] != ((uint32_t *)s2)[i])
			return 1;

	}
	return 0;
}

void test_dump_stats(struct tlu *tlu)
{
	union tlu_stats stats;

	tlu_get_stats(tlu, &stats);
	printk(KERN_INFO "mem_read = %10d  mem_write = %10d  total_find = %10d\n",
		stats.mem_read, stats.mem_write, stats.total_find);
	printk(KERN_INFO "htk_find = %10d  crt_find = %10d  chained_hash_find = %10d\n",
		stats.htk_find, stats.crt_find, stats.chained_hash_find);
	printk(KERN_INFO "flat_data_find = %10d  find_success = %10d  find_fail = %10d\n",
		stats.flat_data_find, stats.find_success, stats.find_fail);
	printk(KERN_INFO "hash_collision = %10d  crt_levels = %10d  carry_out = %10d\n",
		stats.hash_collision, stats.crt_levels, stats.carry_out);
	printk(KERN_INFO "carry_mask = %10d\n", stats.carry_mask);
}

int tlu_test_bank_mem(struct tlu *tlu, int bank)
{
	int mem1, mem2, mem3, mem4, mem5;

	mem1 = tlu_bank_mem_alloc(tlu, bank, 0x100);
	printk(KERN_INFO "mem1 = %x\n", mem1);
	mem2 = tlu_bank_mem_alloc(tlu, bank, 0x10000);
	printk(KERN_INFO "mem2 = %x\n", mem2);
	mem3 = tlu_bank_mem_alloc(tlu, bank, 0x400);
	printk(KERN_INFO "mem3 = %x\n", mem3);
	mem4 = tlu_bank_mem_alloc(tlu, bank, 0x1100);
	printk(KERN_INFO "mem4 = %x\n", mem4);
	tlu_bank_mem_free(tlu, bank, mem2);
	tlu_bank_mem_free(tlu, bank, mem4);
	tlu_bank_mem_free(tlu, bank, mem3);
	mem2 = tlu_bank_mem_alloc(tlu, bank, 0x500);
	printk(KERN_INFO "mem2 = %x\n", mem2);
	mem5 = tlu_bank_mem_alloc(tlu, bank, 0x20000);
	printk(KERN_INFO "mem5 = %x\n", mem5);
	tlu_bank_mem_free(tlu, bank, mem1);
	tlu_bank_mem_free(tlu, bank, mem5);
	tlu_bank_mem_free(tlu, bank, mem2);
	return 0;
}

int tlu_test_basic(struct tlu *tlu, int table, int key_bits, int entry_bytes,
		int mask)
{
	int index_entry, addr_entry, index_hash;
	uint32_t key[8], tmp[8];
	uint32_t data[8];
	struct tlu_hash_entry entry;
	struct tlu_node *node;
	int pass, index;
	uint32_t mem_start;
	int bank = 0;

	TEST_PRINT("Reading TLU ID......");
	TEST_PRINT("%08x\n", tlu_read_reg(tlu->handle, TLU_ID1));
	pass = 1;

	/* tlu_test_bank_mem(tlu, bank); */

	mem_start = tlu_bank_mem_alloc(tlu, bank, 0x10000);

	if (mem_start < 0) {
		TEST_CRIT("Out of bank memory\n");
		return 0;
	}

	tlu_table_config(tlu, table, TLU_TABLE_TYPE_HASH, key_bits,
			entry_bytes, mask, bank, mem_start);
	TEST_LOG("key bits = %d  data bytes = %d  mask = %d\n",
			key_bits, entry_bytes, mask);

	key[0] = 0xc0a80102;
	key[1] = 0xc0a80903;
	key[2] = 0x01060000;
	key[3] = 0x00503456;
	key[4] = 0x12345678;
	key[5] = 0x90abcdef;
	key[6] = 0x11223344;
	key[7] = 0x55667788;

	addr_entry = (1 << mask) * 4;
	index_entry = TLU_ADDR_TO_INDEX(addr_entry);

	TEST_PRINT("Testing Write and Read......");
	if (tlu_write(tlu, table, index_entry, 32, key) < 0) {
		TEST_CRIT("TLU write error\n");
		return 0;
	}

	memset(data, 0, 32);
	if (tlu_read(tlu, table, index_entry, 32, data) == NULL) {
		TEST_CRIT("TLU read error\n");
		return 0;
	}

	if (!memcmp32(key, data, sizeof(key))) {
		TEST_PRINT("Pass\n");
	} else {
		pass = 0;
		printk(KERN_INFO "Fail\n");
		printk(KERN_INFO "Data Written:\n");
		tlu_print_memory(key, sizeof(key));
		printk(KERN_INFO "Data Read Back:\n");
		tlu_print_memory(data, sizeof(key));
	}

	TEST_PRINT("Testing Write Byte......");
	if (tlu_write_byte(tlu,
			table,
			(index_entry << 3) + 3, 3, key + 6) < 0) {
		printk(KERN_INFO "TLU write byte error\n");
		return 0;
	}
	memcpy(((uint8_t *)key) + 3, key + 6, 3);
	if (tlu_read(tlu, table, index_entry, 32, data) < 0) {
		printk(KERN_INFO "TLU read error\n");
		return 0;
	}

	if (!memcmp32(key, data, sizeof(key)))
		printk(KERN_INFO "Pass\n");
	else {
		pass = 0;
		printk(KERN_INFO "Fail\n");
		printk(KERN_INFO "Data Written:\n");
		tlu_print_memory(key, sizeof(key));
		printk(KERN_INFO "Data Read Back:\n");
		tlu_print_memory(data, sizeof(key));
	}

	TEST_PRINT("Testing Find......");
	index_hash = tlu_hash(key, 4, mask);
	TEST_LOG("index_hash = %x/%d\n", index_hash>>1, index_hash & 1);
	node = &entry.nodes[index_hash & 1];
	node->flag = 1;
	node->count = 0;
	node->link = index_entry >> tlu_get_table_index_scale(tlu->handle,
			table);

	if (tlu_write(tlu, table, index_hash >> 1, 8, &entry) < 0)
		printk(KERN_INFO "TLU write error\n");

	index = tlu_find(tlu, table, key, 16);

	if (index < 0) {
		pass = 0;
		printk(KERN_INFO "Fail\n");
		return 0;
	} else{
		if ((index_entry >> tlu_get_table_index_scale(tlu->handle,
						table)) != index) {
			pass = 0;
			printk(KERN_INFO "Fail\n");
			printk(KERN_INFO "Expected Index = %08x  Find Index = %08x\n",
					index_entry, index);
		} else{
			printk(KERN_INFO "Pass\n");
		}
	}

	TEST_PRINT("Testing Findr......");
	index = tlu_findr(tlu, table, key, 16, 0, 32, data);
	if (index < 0) {
		pass = 0;
		printk(KERN_INFO "Fail\n");
	} else if (memcmp32(key, data, sizeof(key))) {
		pass = 0;
		printk(KERN_INFO "Fail\n");
		printk(KERN_INFO "Data Expected:\n");
		tlu_print_memory(key, sizeof(key));
		printk(KERN_INFO "Data Read Back:\n");
		tlu_print_memory(data, sizeof(key));
	} else if ((index_entry >> tlu_get_table_index_scale(tlu->handle,
					table)) != index) {
		pass = 0;
		printk(KERN_INFO "Fail\n");
		printk(KERN_INFO "Expected Index = %08x  Find Index = %08x\n",
				index_entry, index);
	} else{
		printk(KERN_INFO "Pass\n");
	}

	TEST_PRINT("Testing Findw......");
	index = tlu_findw(tlu, table, key, 16, 3*8,
					0xaabbccddeeff0011LL);
	if (index < 0) {
		pass = 0;
		printk(KERN_INFO "Fail\n");
		printk(KERN_INFO "Findw error = %08x\n", index);
	}
	index = tlu_findr(tlu, table, key, 16, 0, 32, data);
	if (index < 0) {
		pass = 0;
		printk(KERN_INFO "Fail\n");
		printk(KERN_INFO "Findr error = %08x\n", index);
	}
	tlu_memcpy32(tmp, key, sizeof(key));
	((uint64_t *)tmp)[3] = 0xaabbccddeeff0011LL;
	if (memcmp32(tmp, data, sizeof(key))) {
		pass = 0;
		printk(KERN_INFO "Fail\n");
		printk(KERN_INFO "Data Expected:\n");
		tlu_print_memory(tmp, sizeof(key));
		printk(KERN_INFO "Data Read Back:\n");
		tlu_print_memory(data, sizeof(key));
	} else{
		printk(KERN_INFO "Pass\n");
	}

	TEST_PRINT("Testing Add......");
	if (tlu_add(tlu, table, index_entry*8 + 20, 0x4111) < 0)
		printk(KERN_INFO "TLU add error\n");

	if (tlu_read(tlu, table, index_entry, 32, data) < 0)
		printk(KERN_INFO "TLU read error\n");

	((uint32_t *)tmp)[20/4] += 0x4111;
	if (!memcmp32(tmp, data, sizeof(key)))
		printk(KERN_INFO "Pass\n");
	else {
		pass = 0;
		printk(KERN_INFO "Fail\n");
		printk(KERN_INFO "Data Expected:\n");
		tlu_print_memory(tmp, sizeof(key));
		printk(KERN_INFO "Data Read Back:\n");
		tlu_print_memory(data, sizeof(key));
		return 0;
	}

	TEST_PRINT("Testing Acchash......");

	index_entry += 32/8;
	if (tlu_write(tlu, table, index_entry, 32, key) < 0)
		printk(KERN_INFO "TLU write error\n");

	index_hash = tlu_hash_cont32(key, 4, tlu_hash32(key + 4, 4));

	index_hash &= (1<<16)-1;
	TEST_LOG("index_hash = %x/%d\n", index_hash>>1, index_hash & 1);
	node = &entry.nodes[index_hash & 1];
	node->flag = 1;
	node->count = 0;
	node->link = index_entry >> tlu_get_table_index_scale(tlu->handle,
			table);

	if (tlu_write(tlu, table, index_hash >> 1, 8, &entry) < 0)
		printk(KERN_INFO "TLU write error\n");

	if (tlu_acchash(tlu, 0, key + 4, 16) < 0)
		printk(KERN_INFO "TLU acchash error\n");

	index = tlu_findr(tlu, table, key, 16, 0, 32, tmp);

	if (index < 0) {
		pass = 0;
		printk(KERN_INFO "Fail\n");
		printk(KERN_INFO "Findr err = %08x\n", index);
	} else if (memcmp32(key, tmp, sizeof(key))) {
		pass = 0;
		printk(KERN_INFO "Fail\n");
		printk(KERN_INFO "Data Expected:\n");
		tlu_print_memory(key, sizeof(key));
		printk(KERN_INFO "Data Read Back:\n");
		tlu_print_memory(tmp, sizeof(key));
	} else if ((index_entry >> tlu_get_table_index_scale(tlu->handle,
					table))
			!= index) {
		pass = 0;
		printk(KERN_INFO "Fail\n");
		printk(KERN_INFO "Expected Index = %08x  Find Index = %08x\n",
				index_entry, index);
	} else {
		printk(KERN_INFO "Pass\n");
	}
	if (tlu_bank_mem_free(tlu, bank, mem_start) < 0) {
		printk(KERN_INFO "Free memory failed\n");
		pass = 0;
	}
	return pass;
}

#define MAX_ENTRY_NUM 30
#define HTK_MODIFY_MAGIC 0x55aa1ef077bb23cdLL
#define HTK_KEY_SIZE 128

int tlu_test_htk(struct tlu *tlu)
{
	struct tlu_htk htk;
	int index;
	int i;
	int index_list[MAX_ENTRY_NUM];
	uint8_t data_backup;
	uint8_t tmp_buf[32];
	uint8_t data[] = {
		0x56, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
		0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
		0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
		0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
		0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7
	};

	TEST_PRINT("Testing HTK......");
	if (tlu_htk_create(&htk, tlu, 0, 2500, HTK_KEY_SIZE,
				sizeof(data), MAX_ENTRY_NUM, 4, 0) < 0) {
		printk(KERN_INFO "Table can not be created\n");
		return 0;
	}
	data_backup = data[1];
	for (i = 0; i < MAX_ENTRY_NUM; i++) {
		index = tlu_htk_insert(&htk, data);
		if (index < 0) {
			printk(KERN_INFO "Fail\ntlu_test_htk[%d]: Insert "
					"failed. err = %d\n", i, index);
			return 0;
		}
		index_list[i] = index;
		data[1]++;
	}
	data[1] = data_backup;
	for (i = 0; i < MAX_ENTRY_NUM; i++) {
		uint64_t tmp_data;
		int offset;

		/* test find */
		index = tlu_htk_find(&htk, data);
		if (index < 0) {
			TEST_CRIT("Fail\ntlu_test_htk[%d]: findr: Not found. "
					"err = %d\n", i, index);
			return 0;
		}
		if (index != index_list[i]) {
			TEST_CRIT("Fail\ntlu_test_htk[%d]: Found index "
				"mismatch. Expected = %d  Find Return = %d \n",
				i, index_list[i], index);
			return 0;
		}
		/* test findr
		 * Note: Data size has to be 64 bytes for this test to pass */
		index = tlu_htk_findr(&htk, data, 32, 32, tmp_buf);
		if (index < 0) {
			TEST_CRIT("Fail\ntlu_test_htk[%d]: findr: Not found. "
					"err = %d\n", i, index);
			return 0;
		}
		if (memcmp(tmp_buf, data + 32, 32)) {
			TEST_CRIT("Fail\ntlu_test_htk[%d]: Findr data "
					"mismatch:\n", i);
			tlu_print_memory(tmp_buf, 32);
			TEST_CRIT("Expected:\n");
			tlu_print_memory(data + 32, 32);
			return 0;
		}
		/* Test Modify and Findr */
		offset = HTK_KEY_SIZE/8 + i % (sizeof(data) - HTK_KEY_SIZE/8);
		index = tlu_htk_findw(&htk, data, offset,
						HTK_MODIFY_MAGIC);
		if (index < 0) {
			TEST_CRIT("Fail\ntlu_test_htk[%d]: findw: Not found. "
					"err = %d\n", i, index);
			return 0;
		}
		if (index != index_list[i]) {
			TEST_CRIT("Fail\ntlu_test_htk[%d]: findw index "
				"mismatch. Expected = %d  Find Return = %d \n",
				i, index_list[i], index);
			return 0;
		}

		index = tlu_htk_findr(&htk, data, offset,
					sizeof(tmp_data), &tmp_data);

		if (index < 0) {
			TEST_CRIT("Fail\ntlu_test_htk[%d]: findr: Not found. "
					"err = %d\n", i, index);
			return 0;
		}
		if (index != index_list[i]) {
			TEST_CRIT("Fail\ntlu_test_htk[%d]: findr indexi "
				"mismatch. Expected = %d  Find Return = %d \n",
				i, index_list[i], index);
			return 0;
		}
		if (tmp_data != HTK_MODIFY_MAGIC) {
			TEST_CRIT("Fail\ntlu_test_htk[%d]: Findr data "
					"incorrect: %016llu\n", i, tmp_data);
			TEST_CRIT("Expected data: %016llu\n", HTK_MODIFY_MAGIC);
			return 0;
		}

		data[1]++;
	}
	data[1] = data_backup;
	for (i = 0; i < MAX_ENTRY_NUM; i++) {
		index = tlu_htk_delete(&htk, data);
		if (index < 0) {
			printk(KERN_INFO "Fail\ntlu_test_htk[%d]: delete fail. err = %d\n",
					i, index);
			return 0;
		}
		data[1]++;
	}

	index = tlu_htk_free(&htk);

	if (index < 0) {
		TEST_CRIT("HTK free failed: %d\n", index);
		return 0;
	}
	printk(KERN_INFO "Pass\n");
	return 1;
}

#define CRT_TEST_ITEM_NUM 12
#define CRT_TEST_DATA_SIZE 8
#define CRT_TEST_DATA_ENTRY_NUM 20
struct crt_entry{
	uint32_t key;
	uint32_t bits;
	uint8_t data[0];
};

int tlu_test_crt(struct tlu *tlu, int compress, int reverse_del)
{
	struct tlu_crt crt;
	int index, i;
	int index_list[CRT_TEST_ITEM_NUM];
	struct crt_entry *entry;

	uint32_t entry_list[] = {
		0xc0000000, 8, 0x11223344, 0xAABB8877,
		0xc0000000, 4, 0x44442222, 0xcccc1234,
		0xc8010000, 16, 0x11111111, 0x9ABCDEF0,	/* Case 3.1 */
		0xc0010000, 16, 0x22222222, 0x9ABCDEF0,	/* Case 3.2 */
		0xc0010200, 24, 0x33333333, 0x9ABCDEF0,	/* Case 3.3 */
		0xc8000000, 8,  0x44444444, 0x9ABCDEF0,	/* Case 2.3 */
		0xa0010000, 16, 0xa0a0a0a0, 0xafafafaf,
		/* Next 3 items are for Case 2.3 with recursive call */
		0xa0010200, 24, 0xa1a1a1a1, 0xaeaeaeae,
		0xa0000000, 8,  0xa2a2a2a2, 0xadadadad,
		0xc8800000, 12, 0x55555555, 0x9ABCDEF0,	/* Case 1 */
		0xc0a90100, 24, 0x0a0a0a0a, 0xa9a9a9a9,
		0xc0a90000, 16, 0x0b0b0b0b, 0xaaaaaaaa,	/* Case 2.3 */
		/* For data size 16 bytes where entries are overlapped. */
		0x16161616, 0x16161616
	};
	uint32_t key[] = {
		0xc0090909,
		0xc501ff02,
		0xc8010902,
		0xc0010908, 0xc0010209,
		0xc8090909, 0xa0010101, 0xa0010203, 0xa0020101,
		0xc8810101,
		0xc0a90123, 0xc0a9aabb
	};

	int free_data, free_mem;

	TEST_PRINT("Testing CRT (compress = %d reverse_delete = %d)......",
			compress, reverse_del);
	tlu_reset_stats(tlu);

	if (tlu_crt_create(&crt, tlu, 0, 80000, 32, CRT_TEST_DATA_SIZE,
				CRT_TEST_DATA_ENTRY_NUM, 0, compress) < 0) {
		printk(KERN_INFO "Table can not be created\n");
		return 0;
	}

	free_data = crt.crt.free_data_count;
	free_mem = crt.crt.free_mem_size;

	TEST_LOG("Inserting......\n");
	for (i = 0; i < CRT_TEST_ITEM_NUM; i++) {
		entry = (struct crt_entry *)(((uint8_t *)entry_list) +
			i * (sizeof(struct crt_entry) + CRT_TEST_DATA_SIZE));
		index = tlu_crt_insert(&crt, &entry->key, entry->bits,
						entry->data);
		if (index < 0) {
			printk(KERN_INFO "Fail\ntlu_test_crt[%d]: Insert "
					"failed. err=%d\n", i, index);
			return 0;
		}
		index_list[i] = index;
	}

	TEST_LOG("Finding......\n");
	for (i = 0; i < CRT_TEST_ITEM_NUM; i++) {
		index = tlu_crt_find(&crt, key + i);
		if (index < 0) {
			printk(KERN_INFO "Fail\ntlu_test_crt[%d]: Not found."
					" err = %d\n", i, index);
			return 0;
		}
		TEST_LOG("[%d] index = %d\n", i, index);
		if (index_list[i] != index) {
			printk(KERN_INFO "Fail\ntlu_test_crt[%d]: Found index"
				"mismatch. Expected = %d  Find Return = %d \n",
				i, index_list[i], index);
			return 0;
		}
		if (i >= sizeof(key)/sizeof(key[0]))
			break;

	}

	/* Findw and Findr. Only test one */
	TEST_LOG("Finding with w/r......\n");
	for (i = 0; i < 1; i++) {
		uint64_t tmp_data;
		int offset;
		offset = 0;
		index = tlu_crt_findw(&crt, key + i, offset,
						HTK_MODIFY_MAGIC);
		if (index < 0) {
			TEST_CRIT("Fail\ntlu_test_crt[%d]: findw: Not found. "
					"err = %d\n", i, index);
			return 0;
		}
		if (index_list[i] != index) {
			TEST_CRIT("Fail\ntlu_test_crt[%d]: findw index "
				"mismatch. Expected = %d  Find Return = %d \n",
				i, index_list[i], index);
			return 0;
		}
		index = tlu_crt_findr(&crt, key + i, offset,
					sizeof(tmp_data), &tmp_data);
		if (index < 0) {
			TEST_CRIT("Fail\ntlu_test_crt[%d]: findr: Not found. "
					"err = %d\n", i, index);
			return 0;
		}
		TEST_LOG("[%d] index = %d\n", i, index);
		if (index_list[i] != index) {
			TEST_CRIT("Fail\ntlu_test_crt[%d]: findr index "
				"mismatch. Expected = %d  Find Return = %d \n",
				i, index_list[i], index);
			return 0;
		}
		if (tmp_data != HTK_MODIFY_MAGIC) {
			TEST_CRIT("Fail\ntlu_test_crt[%d]: Findr data "
					"incorrect: %016llu\n", i, tmp_data);
			TEST_CRIT("Expected data: %016llu\n", HTK_MODIFY_MAGIC);
			return 0;
		}
		if (i >= sizeof(key)/sizeof(key[0]))
			break;

	}

	TEST_LOG("Deleting......\n");
	for (i = 0; i < CRT_TEST_ITEM_NUM; i++) {
		if (reverse_del) {
			entry = (struct crt_entry *)(((uint8_t *)entry_list)
				+ (CRT_TEST_ITEM_NUM - 1 - i)
				* (sizeof(struct crt_entry)
				+ CRT_TEST_DATA_SIZE));
		} else{
			entry = (struct crt_entry *)(((uint8_t *)entry_list) +
				i * (sizeof(struct crt_entry)
				+ CRT_TEST_DATA_SIZE));
		}

		index = tlu_crt_delete(&crt, &entry->key, entry->bits);

		if (index < 0) {
			printk(KERN_INFO "Fail\ntlu_test_crt[%d]: delete: err = %d\n",
					i, index);
			return 0;
		}
	}

	if (free_data != crt.crt.free_data_count
			|| free_mem != crt.crt.free_mem_size) {
		printk(KERN_INFO "Fail\ntlu_test_crt: Memory is not cleaned\n");
		printk(KERN_INFO "		   Initial  Current\n");
		printk(KERN_INFO "Data Entry %5d	%5d\n",
				free_data, crt.crt.free_data_count);
		printk(KERN_INFO "Free Mem   %5d	%5d\n",
				free_mem, crt.crt.free_mem_size);
		return 0;
	}

	index = tlu_crt_free(&crt);

	if (index < 0) {
		TEST_CRIT("CRT free failed: %d\n", index);
		return 0;
	}
	/* test_dump_stats(tlu); */
	printk(KERN_INFO "Pass\n");
	return 1;
}

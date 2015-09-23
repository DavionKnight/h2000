/*
 * Copyright (C) 2007-2011 Freescale Semiconductor, Inc. All rights reserved.
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
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Thu Jan 31 2008
 *
 * Description:
 * This file contains routines to perform TLU raw performance testing.
 */
#include <linux/kernel.h>
#include <generated/autoconf.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <asm/tlu_access.h>
#include <asm/tlu.h>
#include <asm/tlu_driver.h>

#define HTK_PERF_TEST_TABLE_ENTRY_NUM 55000
#define HTK_PERF_TEST_TABLE_KEY_BITS 128
#define HTK_PERF_TEST_TABLE_HASH_BITS 16
#define HTK_PERF_TEST_TABLE_ENTRY_SIZE 32
#define HTK_PERF_TEST_TABLE_BANK 0
#define HTK_PERF_TEST_TABLE_KEY_BYTES (HTK_PERF_TEST_TABLE_KEY_BITS / 8)

#define CRT_PERF_TEST_ITEM_NUM 1
#define CRT_PERF_TEST_DATA_SIZE 8
#define CRT_PERF_TEST_DATA_ENTRY_NUM 20

static uint32_t htk_key[] = {
	0x00000000, 0x01010101, 0xF2E35D6B, 0x8F7E6D5C,
	0x00000000, 0x00000000, 0x00000000, 0x00000000
};

#define mem_sync() __asm__ __volatile__ ("mbar 1"/*"sync"*/ : : : "memory");

int htk_perf_test_add(struct tlu_htk *htk, uint32_t entry_num)
{
	int i, rc;

	for (i = 0; i < entry_num; i++) {
		htk_key[0] = i;
		rc = tlu_htk_insert(htk, htk_key);
		if (rc < 0)
			return -i;

	}
	return entry_num;
}

void dump_stats(struct tlu *tlu)
{
	union tlu_stats stats;

	tlu_get_stats(tlu, &stats);
	printk(KERN_INFO "mem_read:       %10d  mem_write:    %10d  "
			"total_find:        %10d\n",
		stats.mem_read, stats.mem_write, stats.total_find);
	printk(KERN_INFO "htk_find:       %10d  crt_find:     %10d  "
			"chained_hash_find: %10d\n",
		stats.htk_find, stats.crt_find, stats.chained_hash_find);
	printk(KERN_INFO "flat_data_find: %10d  find_success: %10d  "
			"find_fail:         %10d\n",
		stats.flat_data_find, stats.find_success, stats.find_fail);
	printk(KERN_INFO "hash_collision: %10d  crt_levels:   %10d  "
			"carry_out:         %10d\n",
		stats.hash_collision, stats.crt_levels, stats.carry_out);
	printk(KERN_INFO "carry_mask:     %10d\n", stats.carry_mask);
}

int htk_perf_test(struct tlu *tlu, uint32_t entry_num, int findr, int stats)
{
	int rc;
	struct tlu_htk htk;
	uint32_t count;
	uint32_t time_start;
	uint32_t data[8];

	rc = tlu_htk_create(&htk, tlu, -1, 0,
		HTK_PERF_TEST_TABLE_KEY_BITS, HTK_PERF_TEST_TABLE_ENTRY_SIZE,
		/* 55000 entry space yields better performance on 8572,rcan not
		 * explain it. */
		entry_num >= 55000 ? entry_num : 55000,
		HTK_PERF_TEST_TABLE_HASH_BITS,
		HTK_PERF_TEST_TABLE_BANK);
	if (rc < 0) {
		printk(KERN_ERR "ERROR: Failed to create HTK table: %d\n", rc);
		return -1;
	}

	rc = htk_perf_test_add(&htk, entry_num);

	if (rc <= 0) {
		printk(KERN_INFO "Failed to insert all entries. %d entries inserted.\n",
				-rc);
		tlu_htk_free(&htk);
		return -1;
	}

	if (stats)
		tlu_reset_stats(tlu);

	if (!findr)
		tlu_htk_find(&htk, htk_key);
	else
		tlu_htk_findr(&htk, htk_key, HTK_PERF_TEST_TABLE_KEY_BYTES,
				8, data);

	count = 0;
	time_start = jiffies;
	htk_key[0] = 0;
	if (!findr) {
		do {
			rc = tlu_htk_find(&htk, htk_key);
			if (rc < 0) {
				if (rc != TLU_NOT_FOUND)
					printk(KERN_INFO "TLU find error %d\n",
							rc);

				tlu_htk_free(&htk);
				return -1;
			}
			count++;
		} while (jiffies - time_start < HZ);
	} else {
		do {
			rc = tlu_htk_findr(&htk, htk_key,
					HTK_PERF_TEST_TABLE_KEY_BYTES,
					8, data);
			if (rc < 0) {
				if (rc != TLU_NOT_FOUND)
					printk(KERN_INFO "TLU find error %d\n",
							rc);

				tlu_htk_free(&htk);
				return -1;
			}
			count++;
		} while (jiffies - time_start < HZ);
	}
	printk(KERN_INFO "HTK: %7d entries, %d lookups/second\n",
			entry_num, count);
	if (stats) {
		dump_stats(tlu);
		printk(KERN_INFO "\n");
	}

	tlu_htk_free(&htk);
	return 0;
}

int crt_perf_test(struct tlu *tlu, int bits, int compress, int findr, int stats)
{
	struct tlu_crt crt;
	uint32_t count;
	uint32_t time_start;
	uint32_t key = 0xc0a93b2a;
	uint32_t data[8];
	int rc;

	if (tlu_crt_create(&crt, tlu, 0, 1000000, 32, CRT_PERF_TEST_DATA_SIZE,
			    CRT_PERF_TEST_DATA_ENTRY_NUM, 0, compress) < 0) {
		printk(KERN_INFO "Table can not be created\n");
		return -1;
	}
	/* Inserting */
	rc = tlu_crt_insert(&crt, &key, bits, data);
	if (rc < 0) {
		printk(KERN_INFO "Insert failed. err = %d\n", rc);
		tlu_crt_free(&crt);
		return -1;
	}

	if (stats)
		tlu_reset_stats(tlu);

	/* push the code into cache */
	if (!findr)
		tlu_crt_find(&crt, &key);
	else
		tlu_crt_findr(&crt, &key, 0, 8, data);

	count = 0;
	time_start = jiffies;
	if (!findr) {
		do {
			rc = tlu_crt_find(&crt, &key);
			if (rc < 0) {
				if (rc != TLU_NOT_FOUND)
					printk(KERN_INFO "TLU find error %d\n",
							rc);

				tlu_crt_free(&crt);
				return -1;
			}
			count++;
		} while (jiffies - time_start < HZ);
	} else {
		do {
			rc = tlu_crt_findr(&crt, &key, 0, 8, data);
			if (rc < 0) {
				if (rc != TLU_NOT_FOUND)
					printk(KERN_INFO "TLU find error %d\n",
							rc);

				tlu_crt_free(&crt);
				return -1;
			}
			count++;
		} while (jiffies - time_start < HZ);
	}

	printk(KERN_INFO "CRT: %7d bits,    %d lookups/second\n", bits, count);
	if (stats) {
		dump_stats(tlu);
		printk(KERN_INFO "\n");
	}

	rc = tlu_crt_free(&crt);

	if (rc < 0) {
		printk(KERN_INFO "CRT free failed: %d\n", rc);
		return -1;
	}
	return 0;
}

int tlu_perf_test(struct tlu *tlu, int compress, int findr, int stats)
{
	printk(KERN_INFO "---------Mutex, no inline -------------------\n");
	if (htk_perf_test(tlu, 1, findr, stats) < 0)
		return -1;

	if (htk_perf_test(tlu, 1000, findr, stats) < 0)
		return -1;

	if (htk_perf_test(tlu, 10000, findr, stats) < 0)
		return -1;

	if (htk_perf_test(tlu, 40000, findr, stats) < 0)
		return -1;

	if (htk_perf_test(tlu, 100000, findr, stats) < 0)
		return -1;

	if (crt_perf_test(tlu, 8, compress, findr, stats) < 0)
		return -1;

	if (crt_perf_test(tlu, 16, compress, findr, stats) < 0)
		return -1;

	if (crt_perf_test(tlu, 24, compress, findr, stats) < 0)
		return -1;

	if (crt_perf_test(tlu, 32, compress, findr, stats) < 0)
		return -1;

	return 0;
}

#define TLU_NOT_READY 	-100
/*  < 0 error or not ready
 * >= 0 success
 */
static inline int tlu_poll(tlu_handle_t tlu)
{
	uint32_t cstat;

	cstat = TLU_REG(tlu, TLU_CSTAT);
	if (!(cstat & TLU_CSTAT_RDY))
		return TLU_NOT_READY;

	if (cstat & TLU_CSTAT_FAIL) {
		if ((cstat & TLU_CSTAT_ERR) == 0)
			return TLU_NOT_FOUND;
	else {
			TLU_ACC_CRIT("Find fail. cstat = %08x tlu = %x\n",
					cstat, tlu);
			return TLU_MEM_ERROR;
		}
	}
	return cstat & TLU_CSTAT_INDEX;
}

static inline int htk_ready(struct htk *htk)
{
	int index;

	index = tlu_poll(htk->tlu);

	if (index >= 0) {
		if (index >= htk->data_start)
			return index - htk->data_start;
		else
			return TLU_NOT_FOUND;
	}
	return index;
}

static inline void _htk_find(struct htk *htk, void *key)
{
	tlu_send_cmd_find(htk->tlu, htk->table, key, htk->key_bits >> 3);
}

int htk_perf_test_async(struct tlu *tlu1, struct tlu *tlu2, uint32_t entry_num,
		int stats)
{
	int rc;
	struct tlu_htk tlu_htk1, tlu_htk2;
	uint32_t count;
	uint32_t time_start;

	rc = tlu_htk_create(&tlu_htk1, tlu1, -1, 0,
		HTK_PERF_TEST_TABLE_KEY_BITS, HTK_PERF_TEST_TABLE_ENTRY_SIZE,
		entry_num >= 55000 ? entry_num : 55000,
		HTK_PERF_TEST_TABLE_HASH_BITS,
		HTK_PERF_TEST_TABLE_BANK);
	if (rc < 0) {
		printk(KERN_ERR "ERROR: Failed to create HTK table: %d\n", rc);
		return -1;
	}
	if (tlu2) {
		rc = tlu_htk_create(&tlu_htk2, tlu2, -1, 0,
			HTK_PERF_TEST_TABLE_KEY_BITS,
			HTK_PERF_TEST_TABLE_ENTRY_SIZE,
			entry_num >= 55000 ? entry_num : 55000,
			HTK_PERF_TEST_TABLE_HASH_BITS,
			HTK_PERF_TEST_TABLE_BANK);
		if (rc < 0) {
			printk(KERN_ERR "ERROR: Failed to create HTK"
				       " table: %d\n", rc);
			tlu_htk_free(&tlu_htk1);
			return -1;
		}
	}

	rc = htk_perf_test_add(&tlu_htk1, entry_num);

	if (rc <= 0) {
		printk(KERN_INFO "Failed to insert all entries. "
				"%d entries inserted.\n", -rc);
		tlu_htk_free(&tlu_htk1);
		if (tlu2)
			tlu_htk_free(&tlu_htk2);

		return -1;
	}
	if (tlu2) {
		rc = htk_perf_test_add(&tlu_htk2, entry_num);
		if (rc <= 0) {
			printk(KERN_INFO "Failed to insert all entries. "
					"%d entries inserted.\n", -rc);
			tlu_htk_free(&tlu_htk1);
			if (tlu2)
				tlu_htk_free(&tlu_htk2);
			return -1;
		}
	}

	if (stats) {
		tlu_reset_stats(tlu1);
		if (tlu2)
			tlu_reset_stats(tlu2);

	}

	count = 0;
	time_start = jiffies;
	htk_key[0] = 0;
	if (tlu2) {
		do {
			_htk_find(&tlu_htk1.htk, htk_key);
			_htk_find(&tlu_htk2.htk, htk_key);
			mem_sync();

			do {} while ((rc = htk_ready(&tlu_htk1.htk)) ==
					TLU_NOT_READY);
			if (rc < 0) {
				if (rc != TLU_NOT_FOUND)
					printk(KERN_INFO "TLU find error %d\n",
							rc);
				else
					printk(KERN_INFO "TLU not found %d\n",
							rc);
				break;
			}
			do {} while ((rc = htk_ready(&tlu_htk2.htk)) ==
					TLU_NOT_READY);
			if (rc < 0) {
				if (rc != TLU_NOT_FOUND)
					printk(KERN_INFO "TLU find error %d\n",
							rc);
				 else
					printk(KERN_INFO "TLU not found %d\n",
							rc);
				break;
			}
			count += 2;
		} while (jiffies - time_start < HZ);
	} else {
		do {
			_htk_find(&tlu_htk1.htk, htk_key);
			mem_sync();

			do {} while ((rc = htk_ready(&tlu_htk1.htk)) ==
					TLU_NOT_READY);
			if (rc < 0) {
				if (rc != TLU_NOT_FOUND)
					printk(KERN_INFO "TLU find error %d\n",
							rc);
				 else
					printk(KERN_INFO "TLU not found %d\n",
							rc);
				break;
			}
			count++;
		} while (jiffies - time_start < HZ);
	}
	printk(KERN_INFO "HTK: %7d entries, %d lookups/second\n",
			entry_num, count);
	if (stats) {
		dump_stats(tlu1);
		if (tlu2)
			dump_stats(tlu2);

		printk(KERN_INFO "\n");
	}

	tlu_htk_free(&tlu_htk1);
	if (tlu2)
		tlu_htk_free(&tlu_htk2);

	return 0;
}

static inline int crt_ready(struct crt *crt)
{
	int index;

	index = tlu_poll(crt->tlu);

	if (index >= 0) {
		if (index >= crt->data_start)
			return index - crt->data_start;
	else
			return TLU_NOT_FOUND;
	}
	return index;
}

static inline void _crt_find(struct crt *crt, void *key)
{
	tlu_send_cmd_find(crt->tlu, crt->table, key, crt->key_bits >> 3);
}

int crt_perf_test_async(struct tlu *tlu1, struct tlu *tlu2, int bits,
		int compress, int stats)
{
	struct tlu_crt tlu_crt1, tlu_crt2;
	uint32_t count;
	uint32_t time_start;
	uint32_t key = 0xc0a93b2a;
	uint32_t data[8];
	int rc;

	if (tlu_crt_create(&tlu_crt1, tlu1, 0, 1000000, 32,
			CRT_PERF_TEST_DATA_SIZE,
			CRT_PERF_TEST_DATA_ENTRY_NUM, 0, compress) < 0) {
		printk(KERN_INFO "Table can not be created\n");
		return -1;
	}
	if (tlu2 && tlu_crt_create(&tlu_crt2, tlu2, 0, 1000000, 32,
			CRT_PERF_TEST_DATA_SIZE,
			CRT_PERF_TEST_DATA_ENTRY_NUM, 0, compress) < 0) {
		printk(KERN_INFO "Table can not be created\n");
		tlu_crt_free(&tlu_crt1);
		return -1;
	}
	/* Inserting */
	rc = tlu_crt_insert(&tlu_crt1, &key, bits, data);
	if (rc < 0) {
		printk(KERN_INFO "Insert failed. err = %d\n", rc);
		tlu_crt_free(&tlu_crt1);
		if (tlu2)
			tlu_crt_free(&tlu_crt2);

		return -1;
	}
	if (tlu2) {
		rc = tlu_crt_insert(&tlu_crt2, &key, bits, data);
		if (rc < 0) {
			printk(KERN_INFO "Insert failed. err = %d\n", rc);
			tlu_crt_free(&tlu_crt1);
			if (tlu2)
				tlu_crt_free(&tlu_crt2);
			return -1;
		}
	}

	if (stats) {
		tlu_reset_stats(tlu1);
		if (tlu2)
			tlu_reset_stats(tlu2);

	}

	count = 0;
	time_start = jiffies;
	if (tlu2) {
		do {
			_crt_find(&tlu_crt1.crt, &key);
			_crt_find(&tlu_crt2.crt, &key);
			mb();

			do {} while ((rc = crt_ready(&tlu_crt1.crt)) ==
					TLU_NOT_READY);
			if (rc < 0) {
				if (rc != TLU_NOT_FOUND)
					printk(KERN_INFO "TLU find error %d\n",
							rc);
				else
					printk(KERN_INFO "TLU not found %d\n",
							rc);
				break;
			}
			do {} while ((rc = crt_ready(&tlu_crt2.crt)) ==
					TLU_NOT_READY);
			if (rc < 0) {
				if (rc != TLU_NOT_FOUND)
					printk(KERN_INFO "TLU find error %d\n",
							rc);
				else
					printk(KERN_INFO "TLU not found %d\n",
							rc);
				break;
			}
			count += 2;
		} while (jiffies - time_start < HZ);
	} else {
		do {
			_crt_find(&tlu_crt1.crt, &key);
			mb();

			do {} while ((rc = crt_ready(&tlu_crt1.crt)) ==
					TLU_NOT_READY);
			if (rc < 0) {
				if (rc != TLU_NOT_FOUND)
					printk(KERN_INFO "TLU find error %d\n",
							rc);
				else
					printk(KERN_INFO "TLU not found %d\n",
							rc);
				break;
			}
			count += 1;
		} while (jiffies - time_start < HZ);
	}

	printk(KERN_INFO "CRT: %7d bits,    %d lookups/second\n", bits, count);
	if (stats) {
		dump_stats(tlu1);
		if (tlu2)
			dump_stats(tlu2);

		printk(KERN_INFO "\n");
	}

	tlu_crt_free(&tlu_crt1);
	if (tlu2)
		tlu_crt_free(&tlu_crt2);

	return 0;
}

int tlu_perf_test_async(struct tlu *tlu1, int compress, int findr, int stats)
{
	printk(KERN_INFO "---------No mutex, inline -------------------\n");
	if (htk_perf_test_async(tlu1, NULL, 1, stats) < 0)
		return -1;

	if (htk_perf_test_async(tlu1, NULL, 1000, stats) < 0)
		return -1;

	if (htk_perf_test_async(tlu1, NULL, 10000, stats) < 0)
		return -1;

	if (htk_perf_test_async(tlu1, NULL, 50000, stats) < 0)
		return -1;

	if (htk_perf_test_async(tlu1, NULL, 100000, stats) < 0)
		return -1;

	if (crt_perf_test_async(tlu1, NULL, 8, compress, stats) < 0)
		return -1;

	if (crt_perf_test_async(tlu1, NULL, 16, compress, stats) < 0)
		return -1;

	if (crt_perf_test_async(tlu1, NULL, 24, compress, stats) < 0)
		return -1;

	if (crt_perf_test_async(tlu1, NULL, 32, compress, stats) < 0)
		return -1;

	return 0;
}

int tlu_perf_test_async_interleave(struct tlu *tlu1, struct tlu *tlu2,
		int compress, int findr, int stats)
{
	printk(KERN_INFO "---------Interleave on two TLUs-----------\n");
	if (htk_perf_test_async(tlu1, tlu2, 1, stats) < 0)
		return -1;

	if (htk_perf_test_async(tlu1, tlu2, 1000, stats) < 0)
		return -1;

	if (htk_perf_test_async(tlu1, tlu2, 10000, stats) < 0)
		return -1;

	if (htk_perf_test_async(tlu1, tlu2, 50000, stats) < 0)
		return -1;

	if (crt_perf_test_async(tlu1, tlu2, 8, compress, stats) < 0)
		return -1;

	if (crt_perf_test_async(tlu1, tlu2, 16, compress, stats) < 0)
		return -1;

	if (crt_perf_test_async(tlu1, tlu2, 24, compress, stats) < 0)
		return -1;

	if (crt_perf_test_async(tlu1, tlu2, 32, compress, stats) < 0)
		return -1;

	return 0;
}

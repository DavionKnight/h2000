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
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Wed Mar 28 2007
 *
 * Description:
 * This file contains an implementation of a routing cache table using TLU.
 */
#include <linux/kernel.h>
#include <generated/autoconf.h>
#include <linux/slab.h>
#include "route_cache.h"
#include <asm/tlu.h>
#include <asm/tlu_driver.h>

/*#define ROUTE_CACHE_DEBUG*/
#define ROUTE_CACHE_STATS
#define USE_FIND

#define ROUTE_CACHE_TABLE_ENTRY_NUM 100000
#define ROUTE_CACHE_TABLE_KEY_BITS 128
#define ROUTE_CACHE_TABLE_HASH_BITS 16
#define ROUTE_CACHE_TABLE_ENTRY_SIZE 32
#define ROUTE_CACHE_TABLE_BANK 0
#define ROUTE_CACHE_TABLE_KEY_BYTES (ROUTE_CACHE_TABLE_KEY_BITS / 8)

#define ROUTE_CACHE_TABLE_SIZE \
	((1 << (ROUTE_CACHE_TABLE_HASH_BITS)) * 4 \
	+ (ROUTE_CACHE_TABLE_ENTRY_NUM * 2) * 4 \
	+ ROUTE_CACHE_TABLE_ENTRY_NUM * ROUTE_CACHE_TABLE_ENTRY_SIZE)

static struct tlu *tlu;
static struct tlu_htk htk;
static struct route_cache_stats _route_cache_stats;
#ifdef USE_FIND
void *route_cache_ctx_table[ROUTE_CACHE_TABLE_ENTRY_NUM];
#endif

struct route_cache_stats *route_cache_get_stats(void)
{
	return &_route_cache_stats;
}

void route_cache_reset_stats(void)
{
	memset(&_route_cache_stats, 0, sizeof(_route_cache_stats));
}

#ifdef ROUTE_CACHE_DEBUG
static void print_memory(const void *buf, int len, const void *print_addr,
		const char *prefix)
{
	int i;

	for (i = 0; i < len / 4; i++) {
		if (!(i & 7)) {
			if (prefix != NULL)
				printk(KERN_INFO "%s ", prefix);

			if (print_addr != NULL)
				printk(KERN_INFO "%p: ",
					&((uint32_t *)print_addr)[i]);

		}
		printk(KERN_INFO "%08x ", ((uint32_t *)buf)[i]);
		if ((i & 7) == 7)
			printk(KERN_INFO "\n");

	}
	for (i = len & (~3); i < len; i++) {
		if (!(i & 7)) {
			if (prefix != NULL)
				printk(KERN_INFO "%s ", prefix);

			if (print_addr != NULL)
				printk(KERN_INFO "%p: ",
					&((uint8_t *)print_addr)[i]);

		}
		printk(KERN_INFO "%02x", ((uint8_t *)buf)[i]);
	}
	if ((i & 31) != 0)
		printk(KERN_INFO "\n");

}
#endif /*ifdef ROUTE_CACHE_DEBUG*/

int route_cache_add(short iif, short oif, uint32_t daddr, uint32_t saddr,
		int tos, void *ctx)
{
	struct route_cache entry;
	int rc;

	entry.iif = iif;
	entry.oif = oif;
	entry.daddr = daddr;
	entry.saddr = saddr;
	entry.tos = tos;
	entry.ctx = ctx;
#ifdef ROUTE_CACHE_DEBUG
	print_memory(entry.key, ROUTE_CACHE_TABLE_ENTRY_SIZE,
			entry.key, "ADD  ");
#endif
	rc = tlu_htk_insert(&htk, entry.key);
	if (rc < 0) {
		_route_cache_stats.add_fail_count++;
		return rc;
	}
#ifdef USE_FIND
	route_cache_ctx_table[rc] = ctx;
#endif
	_route_cache_stats.entry_num++;
	return rc;
}

int route_cache_del(short iif, short oif, uint32_t daddr, uint32_t saddr,
		int tos)
{
	struct route_cache entry;
	int rc;

	entry.iif = iif;
	entry.oif = oif;
	entry.daddr = daddr;
	entry.saddr = saddr;
	entry.tos = tos;

#ifdef ROUTE_CACHE_DEBUG
	print_memory(entry.key, ROUTE_CACHE_TABLE_ENTRY_SIZE,
			entry.key, "DEL  ");
#endif
	rc = tlu_htk_delete(&htk, entry.key);
	if (rc < 0)
		return rc;

	_route_cache_stats.entry_num--;
	return rc;
}

void *route_cache_lookup(short iif, short oif, uint32_t daddr, uint32_t saddr,
		int tos)
{
	int rc;
	struct route_cache entry;

	entry.iif = iif;
	entry.oif = oif;
	entry.daddr = daddr;
	entry.saddr = saddr;
	entry.tos = tos;

#ifdef ROUTE_CACHE_STATS
	_route_cache_stats.lookup_count++;
#endif
#ifdef ROUTE_CACHE_DEBUG
	print_memory(entry.key, ROUTE_CACHE_TABLE_ENTRY_SIZE,
			entry.key, "LOOK ");
#endif
#ifdef USE_FIND
	rc = tlu_htk_find(&htk, entry.key);
	if (rc < 0) {
		if (rc != TLU_NOT_FOUND)
			printk(KERN_INFO "TLU find error %d\n", rc);

		return NULL;
	}
#ifdef ROUTE_CACHE_DEBUG
	printk(KERN_INFO "FIND %p\n", entry.ctx);
#endif
#ifdef ROUTE_CACHE_STATS
	_route_cache_stats.hit_count++;
#endif
	return route_cache_ctx_table[rc];
#else	/*ifdef USE_FIND*/
	rc = tlu_htk_findr(&htk, entry.key, ROUTE_CACHE_TABLE_KEY_BYTES,
			 sizeof(entry.ctx), &entry.ctx);
	if (rc < 0) {
		if (rc != TLU_NOT_FOUND)
			printk(KERN_INFO "TLU find error %d\n", rc);

		return NULL;
	}
#ifdef ROUTE_CACHE_DEBUG
	printk(KERN_INFO "FIND %p\n", entry.ctx);
#endif
#ifdef ROUTE_CACHE_STATS
	_route_cache_stats.hit_count++;
#endif
	return entry.ctx;
#endif	/*ifdef USE_FIND*/
}

int route_cache_init(void)
{
	int rc;

	tlu = tlu_get(0);

	if (tlu == NULL) {
		printk(KERN_ERR "ERROR: Unable to open TLU 0\n");
		return -1;
	}
	rc = tlu_htk_create(&htk, tlu, -1, 0,
		ROUTE_CACHE_TABLE_KEY_BITS, ROUTE_CACHE_TABLE_ENTRY_SIZE,
		ROUTE_CACHE_TABLE_ENTRY_NUM, ROUTE_CACHE_TABLE_HASH_BITS,
		ROUTE_CACHE_TABLE_BANK);
	if (rc < 0) {
		printk(KERN_ERR "ERROR: Failed to create HTK table: %d\n", rc);
		return -1;
	}
	route_cache_reset_stats();
	return 0;
}

int route_cache_free(void)
{
	tlu_htk_free(&htk);
	return 0;
}

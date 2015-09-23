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
/* Author: Zhichun Hua, zhichun.hua@freescale.com, Wed Mar 28 2007
 *
 * Description:
 * This file contains an implementation of a routing cache table using TLU.
 */


#ifndef ROUTE_CACHE_H
#define ROUTE_CACHE_H

#include <linux/types.h>

struct route_cache {
	struct route_cache *next;
	union {
		uint8_t key[0];
		struct {
			short	oif;
			short	iif;
			__be32	daddr;
			__be32	saddr;
			int	tos;
		};
	};
	void	*ctx;
	uint32_t pad[3];	/* Make key-data size 32 bytes */
};

struct route_cache_stats {
	uint32_t entry_num;
	uint32_t lookup_count;
	uint32_t hit_count;
	uint32_t add_fail_count;
};

int route_cache_add(short iif, short oif, uint32_t daddr, uint32_t saddr,
		int tos, void *ctx);
int route_cache_del(short iif, short oif, uint32_t daddr, uint32_t saddr,
		int tos);
void *route_cache_lookup(short iif, short oif, uint32_t daddr, uint32_t saddr,
		int tos);
int route_cache_free(void);
void route_cache_reset_stats(void);
struct route_cache_stats *route_cache_get_stats(void);
int route_cache_init(void);

#endif

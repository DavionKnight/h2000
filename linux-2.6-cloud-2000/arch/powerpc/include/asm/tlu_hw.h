/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains basic TLU definitions, inline functions and public APIs.
 *
 * This file is part of the Linux kernel
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef TLU_HW_H
#define TLU_HW_H

#include <asm/tlu_osdef.h>

/*======================== Public Definitions ================================*/
#define TLU_KEY_LEN(len) ((len) / 32 - 1)
#define TLU_TABLE_TYPE_FLAT 1
#define TLU_TABLE_TYPE_IPTD 2
#define TLU_TABLE_TYPE_CRT TLU_TABLE_TYPE_IPTD
#define TLU_TABLE_TYPE_HASH 4
/* "& 0x3" is not a must but it prevents the result being out of range */
#define TLU_UNIT_SIZE 8
#define TLU_LEN_CODE(len) (((((len) + TLU_UNIT_SIZE - 1) >> 3) - 1) & 0x3)
#define TLU_NODE_SIZE (TLU_UNIT_SIZE / 2)
#define TLU_ADDR_TO_INDEX(addr) ((addr) / TLU_UNIT_SIZE)
#define TLU_INDEX_TO_ADDR(index) ((index) * TLU_UNIT_SIZE)
#define TLU_ADDR_TO_NODE_INDEX(addr) ((addr) / TLU_NODE_SIZE)
#define TLU_NODE_INDEX_TO_ADDR(index) ((index) * TLU_NODE_SIZE)
#define TLU_ENTRY_SIZE(size) (1 << (3 + (size)))

/* Register Memory Map based on PowerQUICC III Reference Manuals containing
 * TLU 1.0. Note that the offset value 0x2Fxxx documented some reference manuals
 * is not included in the following definitions.
 */
#define TLU_ID1	 	0x000
#define TLU_ID2	 	0x004
#define TLU_IEVENT  	0x010
#define TLU_IMASK   	0x014
#define TLU_IEATR   	0x018
#define TLU_IEADD   	0x01C
#define TLU_IEDIS   	0x020
#define TLU_MBANK0  	0x040
#define TLU_PTBL0   	0x100
#define TLU_CRAMR   	0x500
#define TLU_CRAMW   	0x504
#define TLU_CFIND   	0x508
#define TLU_CTHTK   	0x510
#define TLU_CTCRT   	0x514
#define TLU_CTCHS   	0x518
#define TLU_CTDAT   	0x51c
#define TLU_CHITS   	0x530
#define TLU_CMISS   	0x534
#define TLU_CHCOL   	0x538
#define TLU_CCRTL   	0x53C
#define TLU_CARO	0x5F0
#define TLU_CARM	0x5F4
#define TLU_CMDOP   	0x600
#define TLU_CMDIX   	0x604
#define TLU_CSTAT   	0x60C
#define TLU_KDATA   	0x800
#define TLU_KD0B	TLU_KDATA

/* The total memory size in byte of TLU registers */
#define TLU_REG_SPACE 	0x1000

/* Memory bank configurations */
#define TLU_MEM_PARITY_ENABLE 	1
#define TLU_MEM_PARITY_DISABLE 	0
#define TLU_MEM_LOCAL_BUS   	0
#define TLU_MEM_SYSTEM_DDR  	1

/* Command Codes */
#define TLU_WRITE 	0x1
#define TLU_ADD	 	0x3
#define TLU_READ	0x4
#define TLU_ACCHASH 	0x6
#define TLU_FIND	0x8
#define TLU_FINDR   	0xA
#define TLU_FINDW   	0xC

/* TLU Command Fields */
#define TLU_CMD_CMD_SHIFT	(31-3)
#define TLU_CMD_LEN_SHIFT	(31-5)
#define TLU_CMD_OFFSET_SHIFT 	(31-8)
#define TLU_CMD_TBL_SHIFT	(31-15)
#define TLU_CMD_DATA_SHIFT   	(31-31)
#define TLU_CMD_TAG_SHIFT	(31-7)
#define TLU_CMD_INDEX_SHIFT  	(31-31)
#define TLU_CMD_LEN_MASK	 0x3
#define TLU_CMD_INDEX_MASK   	0xFFFFFF
#define TLU_CMD_OFFSET_MASK  	0x7

/* TLU CSTAT Fields */
#define TLU_CSTAT_RDY   (1<<(31-0))
#define TLU_CSTAT_FAIL  (1<<(31-1))
#define TLU_CSTAT_ERR   (1<<(31-2))
#define TLU_CSTAT_OVFL  (1<<(31-3))
#define TLU_CSTAT_INDEX ((1<<(31-7))-1)

#define TLU_NODE_LINK_MASK   	0x00FFFFFF
#define TLU_NODE_COUNT_MASK  	0x7F000000
#define TLU_NODE_FLAG_MASK   	0x80000000
#define TLU_NODE_FLAG_SHIFT  	31
#define TLU_NODE_COUNT_SHIFT 	24

#define TLU_NODE_FLAG_DATA 	1
#define TLU_NODE_FLAG_TRIE 	0

#define TLU_MAX_TABLE_NUM   	32
#define TLU_MAX_KEY_BYTES   	16
#define TLU_MAX_KDATA_BYTES 	64
#define TLU_MAX_KEY_BITS (TLU_MAX_KEY_BYTES*8)

#define TLU_MAX_BANK_NUM	4
#define TLU_BANK_BIT_BOUNDARY 	28
#define TLU_BANK_BOUNDARY	(1 << TLU_BANK_BIT_BOUNDARY)
#define TLU_BANK_CFG_BASE_SHIFT 0
#define TLU_BANK_CFG_BASE_MASK 	0xFF

#define TLU_TABLE_BIT_BOUNDARY 	12

#define TLU_TABLE_CFG_BANK_SHIFT	(31 - 15)
#define TLU_TABLE_CFG_BANK_MASK	 	0x0003
#define TLU_TABLE_CFG_KEYSHL_SHIFT  	(31 - 12)
#define TLU_TABLE_CFG_KEYSHL_MASK   	0x000F

#define TLU_TABLE_BASE_ALIGNMENT 	4096

#define TLU_KDATA_SIZE_8  	0
#define TLU_KDATA_SIZE_16 	1
#define TLU_KDATA_SIZE_32 	2
#define TLU_KDATA_SIZE_64 	3

#define TLU_HASH_SEED 		0x9e3779b9

/*======================== Public Data Structures ============================*/
/* Since TLU registers require 32-bit word aligned word access, applications
 * should be careful when using those fields defined in the following data
 * structures.
 */
struct tlu_table_config {
	uint32_t type:3;
	uint32_t:1;
	uint32_t klen:2;
	uint32_t size:2;
	uint32_t mask:5;
	uint32_t:1;
	uint32_t bank:2;
	uint32_t baddr:16;
};

struct tlu_bank {
	uint32_t par:1;
	uint32_t tgt:1;
	uint32_t:22;
	uint32_t base:8;
};

struct tlu_node {
	uint32_t flag:1;
	uint32_t count:7;
	uint32_t link:24;
};

struct tlu_hash_entry {
	struct tlu_node nodes[2];
};

struct tlu_trie_entry {
	struct tlu_node nodes[2];
};

struct tlu_iptd_entry {
	union{
		struct{
			uint32_t entropy;
			uint32_t base:24;
			uint32_t keyshl:4;
			uint32_t etype:4;
		};
		struct{
			uint32_t words[2];
		};
	};
};

extern char *tlu_cmd_str[];

/*======================== Public APIs =======================================*/

/*******************************************************************************
 * Description:
 *   This function calculates TLU hash over a buffer with provided seed. It can
 *   be used for continuous hash.
 * Parameters:
 *   key  - A pointer to the key buffer
 *   len  - Total number of 32-bit words in the buffer
 *   c	- Seed of the hash.
 * Return:
 *   A 32 bit hash value
 ******************************************************************************/
uint32_t tlu_hash_cont32(void *key, int len, uint32_t c);

/******************** TLU Hash Functions **************************************/
/*******************************************************************************
 * Description:
 *   This function calculates TLU hash over a buffer.
 * Parameters:
 *   key  - A pointer to the key buffer
 *   len  - Total number of 32-bit words in the buffer
 * Return:
 *   A 32 bit hash value
 ******************************************************************************/
static inline uint32_t tlu_hash32(void *key, int len)
{
	return tlu_hash_cont32(key, len, TLU_HASH_SEED);
}

/*******************************************************************************
 * Description:
 *   This function calculates hash for a given key using the same algorithm as
 *   TLU's.
 * Parameters:
 *   key        - A pointer to the key
 *   len	- The key length in 4-byte words.
 *   hash_bits  - Total number of bits the result hash will be.
 * Return:
 *   The least significat <hash_bits> bits are hash result. The rest of them
 *   are 0s.
 ******************************************************************************/
static inline int tlu_hash(void *key, int len, int hash_bits)
{
	return tlu_hash32(key, len) & ((1 << hash_bits) - 1);
}

/******************** TLU node access APIs *************************************
 * Since TLU registers require 32-bit word aligned word access, applications
 * should call the following APIs to access node fields instead of directly
 * access using data structure.
 ******************************************************************************/
/* Get the link field from the node */
static inline uint32_t tlu_node_get_link(struct tlu_node *node)
{
	return (*(uint32_t *)node) & TLU_NODE_LINK_MASK;
}

/* Get the count field from the node */
static inline uint32_t tlu_node_get_count(struct tlu_node *node)
{
	return ((*(uint32_t *)node) & TLU_NODE_COUNT_MASK)
		>> TLU_NODE_COUNT_SHIFT;
}

/* Return true if the node links a data entry */
static inline int tlu_node_link_is_data(struct tlu_node *node)
{
	return (*(uint32_t *)node) & TLU_NODE_FLAG_MASK;
}

/* Build a node with give field values */
static inline void tlu_build_node(struct tlu_node *node, int flag,
		uint32_t count, uint32_t link)
{
	*(uint32_t *)node = (flag << TLU_NODE_FLAG_SHIFT)
		| (count << TLU_NODE_COUNT_SHIFT) | link;
}

#endif

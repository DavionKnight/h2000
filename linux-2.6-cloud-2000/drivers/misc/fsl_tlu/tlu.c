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
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains an implementation of TLU Linux APIs.
 */
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/crt.h>
#include <asm/htk.h>
#include <asm/tlu.h>

/* The maximum retry times when allocating a page memory for a TLU bank */
#define TLU_MEM_ALLOC_MAX_RETRY 3

struct device tlu_dma_dev;

int tlu_bank_config(struct tlu *tlu, int bank, int par, int tgt,
		unsigned long base_addr)
{
	int rc;

	TLU_LOCK(&tlu->lock);
	rc = _tlu_bank_config(tlu->handle, bank, par, tgt, base_addr);
	TLU_UNLOCK(&tlu->lock);
	return rc;
}

int tlu_bank_validate(struct tlu *tlu, int bank)
{
	if (bank >= tlu->bank_num) {
		printk(KERN_INFO "Invalid bank ID: %d. A valid value should "
				"be between 0 and %d\n", bank, tlu->bank_num);
		return TLU_INVALID_PARAM;
	}
	/* Addr 0 indicates the bank is not initialized. */
	if (tlu->bank_cfg[bank].paddr == 0) {
		printk(KERN_INFO "Bank %d does not exist\n", bank);
		return TLU_INVALID_PARAM;
	}
	return 0;
}

void tlu_table_config(struct tlu *tlu, int table, int type, int key_bits,
		int entry_size, int mask, int bank, uint32_t base_addr)
{
	TLU_LOCK(&tlu->lock);
	_tlu_table_config(tlu->handle, table, type, key_bits, entry_size,
			mask, bank, base_addr);
	TLU_UNLOCK(&tlu->lock);
}

void *tlu_read(struct tlu *tlu, int table, uint32_t index, int len, void *data)
{
	void *rc;

	TLU_LOCK(&tlu->lock);
	rc = _tlu_read(tlu->handle, table, index, len, data);
	TLU_UNLOCK(&tlu->lock);
	return rc;
}

int tlu_write(struct tlu *tlu, int table, uint32_t index, int len, void *data)
{
	int rc;

	TLU_LOCK(&tlu->lock);
	rc = _tlu_write(tlu->handle, table, index, len, data);
	TLU_UNLOCK(&tlu->lock);
	return rc;
}

int tlu_write_byte(struct tlu *tlu, int table, uint32_t addr, int len,
		void *data)
{
	int rc;

	TLU_LOCK(&tlu->lock);
	rc = _tlu_write_byte(tlu->handle, table, addr, len, data);
	TLU_UNLOCK(&tlu->lock);
	return rc;
}

int tlu_add(struct tlu *tlu, int table, int addr, uint16_t data)
{
	int rc;

	TLU_LOCK(&tlu->lock);
	rc = _tlu_add(tlu->handle, table, addr, data);
	TLU_UNLOCK(&tlu->lock);
	return rc;
}

int tlu_acchash(struct tlu *tlu, int table, void *key, int key_len)
{
	int rc;

	TLU_LOCK(&tlu->lock);
	rc = _tlu_acchash(tlu->handle, table, key, key_len);
	TLU_UNLOCK(&tlu->lock);
	return rc;
}

int tlu_find(struct tlu *tlu, int table, void *key, int key_bytes)
{
	int rc;

	TLU_LOCK(&tlu->lock);
	rc = _tlu_find(tlu->handle, table, key, key_bytes);
	TLU_UNLOCK(&tlu->lock);
	return rc;
}
int tlu_findr(struct tlu *tlu, int table, void *key, int key_bytes, int offset,
		int len, void *data)
{
	int rc;

	TLU_LOCK(&tlu->lock);
	rc = _tlu_findr(tlu->handle, table, key, key_bytes, offset, len, data);
	TLU_UNLOCK(&tlu->lock);
	return rc;
}

int tlu_findw(struct tlu *tlu, int table, void *key, int key_bytes, int offset,
		uint64_t data)
{
	int rc;

	TLU_LOCK(&tlu->lock);
	rc = _tlu_findw(tlu->handle, table, key, key_bytes, offset, data);
	TLU_UNLOCK(&tlu->lock);
	return rc;
}

void tlu_get_stats(struct tlu *tlu, union tlu_stats *stats)
{
	TLU_LOCK(&tlu->lock);
	_tlu_get_stats(tlu->handle, stats);
	TLU_UNLOCK(&tlu->lock);
}

void tlu_reset_stats(struct tlu *tlu)
{
	TLU_LOCK(&tlu->lock);
	_tlu_reset_stats(tlu->handle);
	TLU_UNLOCK(&tlu->lock);
}

int tlu_bank_mem_alloc(struct tlu *tlu, int bank, int size)
{
	int rc;

	TLU_LOCK(&tlu->lock);
	rc = _tlu_bank_mem_alloc(tlu->bank_mem + bank, size);
	TLU_UNLOCK(&tlu->lock);
	return rc;
}

int tlu_bank_mem_free(struct tlu *tlu, int bank, int addr)
{
	int rc;

	TLU_LOCK(&tlu->lock);
	rc = _tlu_bank_mem_free(tlu->bank_mem + bank, addr);
	TLU_UNLOCK(&tlu->lock);
	return rc;
}

/* Lock the table if available */
int tlu_table_lock(struct tlu *tlu, int table)
{
	int rc;

	rc = (((1 << table) & tlu->table_map) == 0);
	if (rc)
		tlu->table_map |= 1 << table;

	return rc;
}

/*******************************************************************************
 * Description:
  This function allocates a table ID for a specified TLU.
 * Parameters:
  tlu   - A pointer to a tlu_t data structure where a table will be allocated.
  table	- Requested table ID if it is not negative. Otherwise, -1 indicates
	  allocating an ID.
 * Return:
  Allocated table ID is returned if it is non-negative. Otherwise, a negative
  value indicates one of the error below:
  TLU_INVALID_PARAM - The table ID is out of range
  TLU_TABLE_ALLOC_FAIL - No more table IDs available
  TLU_TABLE_ASSIGNED - The table ID has already assigned.
*******************************************************************************/
int tlu_table_alloc(struct tlu *tlu, int table)
{
	if (table == -1) {
		table = 0;
		while (table < TLU_MAX_TABLE_NUM) {
			if (tlu_table_lock(tlu, table))
				return table;

			table++;
		}
		return TLU_TABLE_ALLOC_FAIL;
	} else {
		if (table >= TLU_MAX_TABLE_NUM)
			return TLU_INVALID_PARAM;

		if (tlu_table_lock(tlu, table))
			return table;

		return TLU_TABLE_ASSIGNED;
	}
}

/* Free and unlock the previously allocated table */
int tlu_table_free(struct tlu *tlu, int table)
{
	if (table >= TLU_MAX_TABLE_NUM)
		return TLU_INVALID_PARAM;

	tlu->table_map &= ~(1 << table);
	return 0;
}

/*------------------------------HTK APIs--------------------------------------*/
int tlu_htk_create(struct tlu_htk *tlu_htk, struct tlu *tlu, int table,
		int size, int key_bits, int kdata_size, int kdata_entry_num,
		int hash_bits, int bank)
{
	int rc;

	rc = tlu_bank_validate(tlu, bank);

	if (rc < 0)
		return rc;

	table = tlu_table_alloc(tlu, table);

	if (table < 0)
		return table;

	tlu_htk->tlu = tlu;
	TLU_LOCK(&tlu->lock);
	rc = htk_create(&tlu_htk->htk, tlu->handle, table, size, key_bits,
					kdata_size, kdata_entry_num, hash_bits,
					tlu->bank_mem + bank);
	if (rc < 0)
		tlu_table_free(tlu, table);

	TLU_UNLOCK(&tlu->lock);
	return rc;

}

int tlu_htk_insert(struct tlu_htk *tlu_htk, void *entry)
{
	int rc;

	TLU_LOCK(&tlu_htk->tlu->lock);
	rc = htk_insert(&tlu_htk->htk, entry);
	TLU_UNLOCK(&tlu_htk->tlu->lock);
	return rc;
}

int tlu_htk_delete(struct tlu_htk *tlu_htk, void *key)
{
	int rc;

	TLU_LOCK(&tlu_htk->tlu->lock);
	rc = htk_delete(&tlu_htk->htk, key);
	TLU_UNLOCK(&tlu_htk->tlu->lock);
	return rc;
}

int tlu_htk_free(struct tlu_htk *tlu_htk)
{
	int rc;

	TLU_LOCK(&tlu_htk->tlu->lock);
	/* Free the table id */
	tlu_table_free(tlu_htk->tlu, tlu_htk->htk.table);
	rc = htk_free(&tlu_htk->htk);
	TLU_UNLOCK(&tlu_htk->tlu->lock);
	return rc;
}

int tlu_htk_find(struct tlu_htk *htk, void *key)
{
	int rc;

	TLU_LOCK(&htk->tlu->lock);
	rc = htk_find(&htk->htk, key);
	TLU_UNLOCK(&htk->tlu->lock);
	return rc;
}

int tlu_htk_findr(struct tlu_htk *htk, void *key, int offset, int len,
		void *data)
{
	int rc;

	TLU_LOCK(&htk->tlu->lock);
	rc = htk_findr(&htk->htk, key, offset, len, data);
	TLU_UNLOCK(&htk->tlu->lock);
	return rc;
}

int tlu_htk_findw(struct tlu_htk *htk, void *key, int offset, uint64_t data)
{
	int rc;

	TLU_LOCK(&htk->tlu->lock);
	rc = htk_findw(&htk->htk, key, offset, data);
	TLU_UNLOCK(&htk->tlu->lock);
	return rc;
}

int tlu_crt_create(struct tlu_crt *tlu_crt, struct tlu *tlu, int table,
		int size, int key_bits, int kdata_size,
		int kdata_entry_num, int bank, int compress)
{
	int rc;

	rc = tlu_bank_validate(tlu, bank);

	if (rc < 0)
		return rc;

	table = tlu_table_alloc(tlu, table);

	if (table < 0) {
		printk(KERN_INFO "Table allocation failed. Table map = %08x\n",
				tlu->table_map);
		return table;
	}

	tlu_crt->tlu = tlu;
	/*if ((tlu_crt->buf = kmalloc(crt_required_mem_size(kdata_entry_num), */
	tlu_crt->buf = vmalloc(crt_required_mem_size(kdata_entry_num));
	if (tlu_crt->buf == NULL) {
		printk(KERN_INFO "vmalloc failed to allocate %d bytes\n",
				crt_required_mem_size(kdata_entry_num));
		tlu_table_free(tlu, table);
		return TLU_OUT_OF_MEM;
	}
	TLU_LOCK(&tlu->lock);
	rc = crt_create(&tlu_crt->crt, tlu->handle, table, size, key_bits,
				kdata_size, kdata_entry_num,
				tlu->bank_mem + bank, compress,
				tlu_crt->buf);
	if (rc < 0) {
		/*kfree(tlu_crt->buf);*/
		vfree(tlu_crt->buf);
		tlu_table_free(tlu, table);
		tlu_crt->buf = NULL;
	}
	TLU_UNLOCK(&tlu->lock);
	return rc;
}

int tlu_crt_free(struct tlu_crt *tlu_crt)
{
	int rc;

	TLU_LOCK(&tlu_crt->tlu->lock);
	/* Free the buffer */
	if (tlu_crt->buf) {
		/*kfree(tlu_crt->buf);*/
		vfree(tlu_crt->buf);
		tlu_crt->buf = NULL;
	}

	/* Free the table id */
	tlu_table_free(tlu_crt->tlu, tlu_crt->crt.table);
	/* Free crt */
	rc = crt_free(&tlu_crt->crt);
	TLU_UNLOCK(&tlu_crt->tlu->lock);
	return rc;
}

int tlu_crt_insert(struct tlu_crt *tlu_crt, void *key, int bits, void *data)
{
	int rc;

	TLU_LOCK(&tlu_crt->tlu->lock);
	rc = crt_insert(&tlu_crt->crt, key, bits, data);
	TLU_UNLOCK(&tlu_crt->tlu->lock);
	return rc;

}

int tlu_crt_delete(struct tlu_crt *tlu_crt, void *key, int key_bits)
{
	int rc;

	TLU_LOCK(&tlu_crt->tlu->lock);
	rc = crt_delete(&tlu_crt->crt, key, key_bits);
	TLU_UNLOCK(&tlu_crt->tlu->lock);
	return rc;
}

int tlu_crt_find(struct tlu_crt *tlu_crt, void *key)
{
	int rc;

	TLU_LOCK(&tlu_crt->tlu->lock);
	rc = crt_find(&tlu_crt->crt, key);
	TLU_UNLOCK(&tlu_crt->tlu->lock);
	return rc;
}

int tlu_crt_findr(struct tlu_crt *tlu_crt, void *key, int offset, int len,
		void *data)
{
	int rc;

	TLU_LOCK(&tlu_crt->tlu->lock);
	rc = crt_findr(&tlu_crt->crt, key, offset, len, data);
	TLU_UNLOCK(&tlu_crt->tlu->lock);
	return rc;
}

int tlu_crt_findw(struct tlu_crt *tlu_crt, void *key, int offset, uint64_t data)
{
	int rc;

	TLU_LOCK(&tlu_crt->tlu->lock);
	rc = crt_findw(&tlu_crt->crt, key, offset, data);
	TLU_UNLOCK(&tlu_crt->tlu->lock);
	return rc;
}

static int get_page_order(int size)
{
	int order;

	order = 0;
	while (PAGE_SIZE * (1 << order) < size)
		order++;
	return order;
}


/*******************************************************************************
 * Description:
  This function is to allocate a memory block which is suitable for TLU's bank
  memory. TLU requires that a bank memory must not across 256M address boundary.
  This function will fail if it has tries maximum TLU_MEM_ALLOC_MAX_RETRY times
  and still not able to allocate a memory meets that requirement.
 * Parameters:
  order   - Requested memory size in power of 2
 * Return:
  The virtual address of allocated memory is returned if success. Otherwise 0
  is returned.
*******************************************************************************/
static unsigned long alloc_phys_pages(int order, dma_addr_t *dma_addr)
{
	unsigned long virt_addr[TLU_MEM_ALLOC_MAX_RETRY];
	dma_addr_t phys_addr;
	int retry, i;

	tlu_dma_dev.archdata.dma_ops = &dma_direct_ops;
	retry = 0;
	while (retry < TLU_MEM_ALLOC_MAX_RETRY) {
		virt_addr[retry] =
			__get_free_pages(GFP_KERNEL, order);
		if (virt_addr[retry] == 0)
			break;

		phys_addr = dma_map_single(&tlu_dma_dev,
				(void *)(virt_addr[retry]),
				PAGE_SIZE * (1 << order), DMA_BIDIRECTIONAL);
		if (dma_mapping_error(&tlu_dma_dev, phys_addr)) {
			printk(KERN_ERR "Map virtual address (0x%lx) failed\n",
					virt_addr[retry]);
			return 0;
		}
		if ((phys_addr >> TLU_BANK_BIT_BOUNDARY) ==
				((phys_addr + (1 << order))
				 >> TLU_BANK_BIT_BOUNDARY)) {
			*dma_addr = phys_addr;
			break;
		}
		dma_unmap_single(&tlu_dma_dev, virt_addr[retry],
				PAGE_SIZE * (1 << order), DMA_BIDIRECTIONAL);
		retry++;
	}
	for (i = 0; i < retry - 1; i++)
		free_pages(virt_addr[i], order);
	/* Free the last one */
	if (retry == TLU_MEM_ALLOC_MAX_RETRY)
		free_pages(virt_addr[TLU_MEM_ALLOC_MAX_RETRY - 1], order);


	if (retry < TLU_MEM_ALLOC_MAX_RETRY)
		return virt_addr[retry];

	return 0;
}

/*******************************************************************************
 * Description:
 *   This function is to initialize a bank memory. It allocates a kernel memory
 *   as bank memory if it is requested.
 * Parameters:
 *   tlu      - A pointer to tlu_t data structure with processor interface
 * 	        having been initialized.
 *   bank     - The index if the bank
 *   cfg      - Requested configuration parameters
 *   bank_cfg - Initialized configuation
 * Return:
 *   0               Success
 *   -1 or -ENOMEM   Failure. A kernel failure message will be printed.
*******************************************************************************/
static int tlu_bank_init(struct tlu *tlu, int bank, struct tlu_bank_param *cfg,
		struct tlu_bank_config *bank_cfg)
{
	dma_addr_t addr;
	int rc;

	bank_cfg->vaddr = 0;
	/* Allocate from the kernel memory */
	addr = cfg->addr;
	if (addr == 0) {
		cfg->type = TLU_MEM_SYSTEM_DDR;
		cfg->parity = TLU_MEM_PARITY_DISABLE;
		bank_cfg->size_order = get_page_order(cfg->size);
		bank_cfg->vaddr = alloc_phys_pages(bank_cfg->size_order,
						&addr);
		if (bank_cfg->vaddr == 0) {
			printk(KERN_ERR "ERROR: Unable to allocate bank "
					"memory. Page order = %d\n",
					bank_cfg->size_order);
			return -ENOMEM;
		}
		printk(KERN_INFO "TLU Table Physical Memory: %08x\n", addr);
	}

	bank_cfg->paddr = addr;
	rc = _tlu_bank_mem_config(tlu->bank_mem + bank, tlu->handle, bank,
					cfg->parity, cfg->type, addr,
					cfg->size);
	if (rc < 0) {
		printk(KERN_ERR "TLU configure bank %d failed. Addr = %08lx "
				"size = %08x  Error = %d\n",
				bank, cfg->addr, cfg->size, rc);
		return rc;
	}
	return 0;
}

void tlu_free(struct tlu *tlu)
{
	int i;
	struct tlu_bank_config *bank_cfg;

	if (tlu->page_base) {
		iounmap((void *)tlu->page_base);
		tlu->page_base = 0;
	}

	for (i = 0; i < tlu->bank_num; i++) {
		bank_cfg = tlu->bank_cfg + i;
		/* Non-zero vaddr indicates the memory is dynamically
		 * allocated.
		 */
		if (bank_cfg->vaddr != 0) {
			dma_unmap_single(&tlu_dma_dev, bank_cfg->vaddr,
					PAGE_SIZE * (1 << bank_cfg->size_order),
					DMA_BIDIRECTIONAL);
			free_pages(bank_cfg->vaddr, bank_cfg->size_order);
			bank_cfg->vaddr = 0;
		}
	}
}

int tlu_init(struct tlu *tlu, unsigned long tlu_phy_addr, int bank_num,
		struct tlu_bank_param *bank_list)
{
	int bank;
	int rc;

	/* Set up TLU's processor interface */
	tlu->page_base = (unsigned long)
			ioremap_nocache((tlu_phy_addr) & PAGE_MASK,
			(TLU_REG_SPACE) + ((tlu_phy_addr) & (~PAGE_MASK)));
	if (!tlu->page_base)
		return -EINVAL;

	tlu->handle = tlu_setup(
			tlu->page_base + ((tlu_phy_addr) & (~PAGE_MASK)));

	/* Setup table memory */
	tlu->bank_num = bank_num;
	/* Initialise the configuration so tlu_free can be called upon error.
	 */
	for (bank = 0; bank < bank_num; bank++)
		tlu->bank_cfg[bank].vaddr = 0;

	for (bank = 0; bank < bank_num; bank++) {
		rc = tlu_bank_init(tlu, bank, bank_list + bank,
					tlu->bank_cfg + bank);
		if (rc < 0) {
			tlu_free(tlu);
			return rc;
		}
	}
	TLU_LOCK_INIT(&tlu->lock);
	tlu->ref_count = 0;
	tlu->table_map = 0;
	return 0;
}

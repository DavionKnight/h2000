/*
 * CAAM xor support header.
 * Definitions to perform XOR Parity Calculations, asynchrously using CAAM
 *
 * Copyright 2011 Freescale Semiconductor, Inc
 */

#ifndef CAAMXOR_H
#define CAAMXOR_H

#include <linux/dmaengine.h>

#include "intern.h"

#define MAX_INITIAL_DESCS	64
#define MAX_XOR_SRCS		8

#define JOB_DESC_LEN		7
#define SH_DESC_LEN		53
#define CMD_DESC_LEN		32

struct caam_xor_sh_desc {
	u32 desc[SH_DESC_LEN];
	dma_addr_t sh_desc_phys;
};

struct caam_dma_async_tx_desc {
	struct dma_async_tx_descriptor async_tx;
	struct list_head node;
	struct caam_dma_jq *dma_jq;
	u32 job_desc[JOB_DESC_LEN];
	u32 cmd_desc[CMD_DESC_LEN];
	dma_addr_t cmd_desc_phys;
};

struct caam_dma_desc_pool {
	int desc_cnt;
	struct list_head head;
};

struct caam_dma_jq {
	dma_cookie_t completed_cookie;
	struct dma_chan chan;
	struct device *dev;
	spinlock_t desc_lock;
	struct list_head submit_q;
	struct caam_drv_private_jq *caam_hw_jq;
	struct caam_dma_desc_pool *soft_desc;
};

int caam_jq_dma_init(struct device *ctrldev);

#endif /* CAAMXOR_H */

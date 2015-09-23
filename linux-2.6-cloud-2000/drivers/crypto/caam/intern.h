/*
 * CAAM/SEC 4.x driver backend
 * Private/internal definitions between modules
 *
 * Copyright (c) 2008-2011 Freescale Semiconductor, Inc.
 * All Rights Reserved
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

#ifndef INTERN_H
#define INTERN_H

#define JOBQ_UNASSIGNED 0
#define JOBQ_ASSIGNED 1

/* Currently comes from Kconfig param as a ^2 (driver-required) */
#define JOBQ_DEPTH (1 << CONFIG_CRYPTO_DEV_FSL_CAAM_RINGSIZE)

/* Kconfig params for interrupt coalescing if selected (else zero) */
#ifdef CONFIG_CRYPTO_DEV_FSL_CAAM_INTC
#define JOBQ_INTC JQCFG_ICEN
#define JOBQ_INTC_TIME_THLD CONFIG_CRYPTO_DEV_FSL_CAAM_INTC_TIME_THLD
#define JOBQ_INTC_COUNT_THLD CONFIG_CRYPTO_DEV_FSL_CAAM_INTC_COUNT_THLD
#else
#define JOBQ_INTC 0
#define JOBQ_INTC_TIME_THLD 0
#define JOBQ_INTC_COUNT_THLD 0
#endif

#define MAX_DESC_LEN		512
#define MAX_RECYCLE_DESC	64
#define CAAM_NAPI_WEIGHT	12

/*
 * Storage for tracking each in-process entry moving across a queue
 * Each entry on an output ring needs one of these
 */
struct caam_jqentry_info {
	void (*callbk)(struct device *dev, u32 *desc, u32 status, void *arg);
	void *cbkarg;	/* Argument per ring entry */
	u32 *desc_addr_virt;	/* Stored virt addr for postprocessing */
	dma_addr_t desc_addr_dma;	/* Stored bus addr for done matching */
	u32 desc_size;	/* Stored size for postprocessing, header derived */
};

/* Private sub-storage for a single JobQ */
struct caam_drv_private_jq {
	struct device *parentdev;	/* points back to controller dev */
	int qidx;
	struct caam_job_queue *qregs;	/* points to JobQ's register space */

	struct napi_struct *irqtask;
	struct net_device *net_dev;

	int irq;			/* One per queue */
	int assign;			/* busy/free */

	/* Job ring info */
	int ringsize;	/* Size of rings (assume input = output) */
	struct caam_jqentry_info *entinfo; 	/* Alloc'ed 1 per ring entry */
	spinlock_t inplock ____cacheline_aligned; /* Input ring index lock */
	int inp_ring_write_index;	/* Input index "tail" */
	int head;			/* entinfo (s/w ring) head index */
	dma_addr_t *inpring;	/* Base of input ring, alloc DMA-safe */
	spinlock_t outlock ____cacheline_aligned; /* Output ring index lock */
	int out_ring_read_index;	/* Output index "tail" */
	int tail;			/* entinfo (s/w ring) tail index */
	struct jq_outentry *outring;	/* Base of output ring, DMA-safe */
};

/*
 * Driver-private storage for a single CAAM block instance
 */
struct caam_drv_private {

	struct device *dev;
	struct device **jqdev; /* Alloc'ed array per sub-device */
	spinlock_t jq_alloc_lock;
#ifdef CONFIG_OF
	struct of_device *ofdev;
#else
	/* Non-OF-specific defs */
#endif

	/* Physical-presence section */
	struct caam_ctrl *ctrl; /* controller region */
	struct caam_deco **deco; /* DECO/CCB views */
	struct caam_assurance *ac;
	struct caam_queue_if *qi; /* QI control region */

	/*
	 * Detected geometry block. Filled in from device tree if powerpc,
	 * or from register-based version detection code
	 */
	u8 total_jobqs;		/* Total Job Queues in device */
	u8 qi_present;		/* Nonzero if QI present in device */
	u8 qi_spids;		/* Number subportal IDs in use */
	int secvio_irq;		/* Security violation interrupt number */

	/* which jq allocated to scatterlist crypto */
	int num_jqs_for_algapi;
	struct device **algapi_jq;
	/* list of registered crypto algorithms (mk generic context handle?) */
	struct list_head alg_list;

	/* For DMA-XOR support */
	struct dma_device dma_dev;
	struct caam_xor_sh_desc *xor_sh_desc;

	/* pointer to the cache pool */
	struct kmem_cache *netcrypto_cache;
	/* pointer to edescriptor recycle queue */
	struct ipsec_esp_edesc *edesc_rec_queue[NR_CPUS][MAX_RECYCLE_DESC];
	/* index in edesc recycle queue */
	u8 curr_edesc[NR_CPUS];

	/*
	 * debugfs entries for developer view into driver/device
	 * variables at runtime.
	 */
#ifdef CONFIG_DEBUG_FS
	struct dentry *dfs_root;
	struct dentry *ctl; /* controller dir */
	struct dentry *ctl_rq_dequeued, *ctl_ob_enc_req, *ctl_ib_dec_req;
	struct dentry *ctl_ob_enc_bytes, *ctl_ob_prot_bytes;
	struct dentry *ctl_ib_dec_bytes, *ctl_ib_valid_bytes;
	struct dentry *ctl_faultaddr, *ctl_faultdetail, *ctl_faultstatus;

	struct debugfs_blob_wrapper ctl_kek_wrap, ctl_tkek_wrap, ctl_tdsk_wrap;
	struct dentry *ctl_kek, *ctl_tkek, *ctl_tdsk;
#endif
};

/* CAAM Block ID's */
#define P4080_CAAM_BLOCK 0x0A100200
#define P1010_CAAM_BLOCK 0x0A140100

void caam_jq_algapi_init(struct device *dev);
void caam_jq_algapi_remove(struct device *dev);
#endif /* INTERN_H */

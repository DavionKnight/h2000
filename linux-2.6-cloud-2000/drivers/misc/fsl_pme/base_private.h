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
 * Author: Roy Pledge, Roy.Pledge@freescale.com
 *         Geoff Thorpe, Geoff.Thorpe@freescale.com
 *
 * Description:
 * This file contains internal APIs, shared declarations, macros, and flags for
 * use within the pattern-matcher driver.
 *
 */

#include "common.h"

/*********************/
/* init/finish hooks */
/*********************/

int update_command_setup(void);
void update_command_teardown(void);

/********************************/
/* flags for pme_object commands */
/********************************/

/* Lock DE processing to the current channel until the next command */
#define PME_FLAG_EXCLUSIVITY		(u32)0x80000000
/* Invoke callback for deflate notification (ie. don't suppress) */
#define PME_FLAG_CB_DEFLATE		(u32)0x40000000
/* Invoke callback for report notification (ie. don't suppress) */
#define PME_FLAG_CB_REPORT		(u32)0x20000000
/* The command is to update the object, not pass data through it */
#define PME_FLAG_UPDATE			(u32)0x10000000
/* The input is a freebuffer chain that should be recycled on consumption */
#define PME_FLAG_RECYCLE		(u32)0x08000000
/* Override interrupt-throttling on command-completion */
#define PME_FLAG_INT_ON_COMPLETION	(u32)0x04000000
/* Override interrupt-throttling on notifications */
#define PME_FLAG_INT_ON_NOTIFICATION	(u32)0x02000000
/* Fail (instead of sleeping) if the command can't be written immediately */
#define PME_FLAG_DONT_SLEEP		(u32)0x01000000
/* Mask for all CB flags */
#define PME_FLAG_CB_ALL			(PME_FLAG_CB_COMMAND | \
					PME_FLAG_CB_DEFLATE | \
					PME_FLAG_CB_REPORT)

/*******************/
/* pme_callback API */
/*******************/

struct pme_object;

/* The command FIFO permits 16 bytes of context - the pme_base layer uses 4
 * bytes to point to the pme_object, the remaining 12 bytes is for the user's
 * completion callback and any associated context. This structure permits the
 * caller's information to be mapped onto the FIFO entry. */
struct pme_callback {
	/* A completion callback is typically executed in interrupt context
	 * (from a tasklet). It should never sleep! Only return non-zero if you
	 * wish to intentionally stop hardware processing and trigger a
	 * tear-down. Any buffers that remain in the provided "fbchain" after
	 * the callback completes will be automatically released. Otherwise,
	 * callers should declare their own chains and move any buffers they
	 * wish to retain into their own chain and release them later. NB, the
	 * provided stream_id is the stream_id of the presentation, not the
	 * object the command was performed on. In this way, deflate+scanning
	 * can produce two presentations and the caller will be able to discern
	 * which is which. */
	int (*completion)(struct pme_object *,
			u32 pme_completion_flags, u8 exception_code,
			u64 stream_id,
			struct pme_callback *cb,
			size_t output_used, struct pme_fbchain *fb_output);
	/* Put any per-command context here. */
	union {
		unsigned char bytes[8];
		u32 words[2];
	} ctx;
};

/*****************/
/* pme_object API */
/*****************/

/* (De)allocate a "stream" object bound to the given channel. */
int pme_object_alloc(struct pme_object **, struct pme_channel *,
			unsigned int gfp_flags, void *userctx,
			void (*dtor)(void *userctx, enum pme_dtor reason));
void pme_object_free(struct pme_object *);
/* Read/write the opaque value associated with an object (as supplied as
 * 'userctx' in _alloc() and passed to the 'dtor' callback). */
void pme_object_write(struct pme_object *, void *);
void *pme_object_read(struct pme_object *);

/* Can be called to wait for outstanding operations to complete. Returns
 * -ENODEV if the channel experiences(/ed) an error. */
int pme_object_wait(struct pme_object *);

/* Issue a command. 'flags' indicates, among other things, whether the
 * completion callback will be invoked on command-expiry and/or deflate/report
 * notifications. The 'deflate' and 'report' parameters should be NULL if their
 * corresponding callback flag bit isn't set. */
int pme_object_cmd(struct pme_object *, u32 flags,
			struct pme_callback *cb,
			struct pme_data *input,
			struct pme_data *deflate,
			struct pme_data *report);

/* If a command absolutely must be blocking in that the caller's completion(s)
 * must be invoked prior to returning and the caller may not be put to sleep
 * (ie. the caller is in interrupt-context), use this interface. If this is not
 * the case, don't do this. The return code and parameters are the same as the
 * above API, with an additional expression parameter that is re-evaluated in a
 * loop until true, as a terminating condition. */
#define pme_object_cmd_blocking(obj, fl, cb, inp, def, rep, cond) \
	({ \
		/* Ensure that 'cond' is the *only* thing re-evaluated */ \
		int __ret; \
		struct pme_object *__obj = (obj); \
		u32 __flags = (fl) | PME_FLAG_DONT_SLEEP; \
		struct pme_callback *__cb = (cb); \
		struct pme_data *__input = (inp); \
		struct pme_data *__deflate = (def); \
		struct pme_data *__report = (rep); \
		atomic_inc(&__obj->refs); \
		__ret = pme_object_cmd(__obj, __flags, __cb, __input, \
					__deflate, __report); \
		if (!__ret) { \
			/* Sync the tasklet without blocking, as it may be
			 * running on another CPU. */ \
			__pme_object_suspend_tasklet(__obj, 1); \
			while (!(cond)) \
				__pme_object_invoke_tasklet(__obj); \
			__pme_object_suspend_tasklet(__obj, 0); \
		} \
		atomic_dec(&__obj->refs); \
		__ret; \
	})

/* Don't use directly, use only via the pme_object_cmd_blocking() macro */
void __pme_object_suspend_tasklet(struct pme_object *, int suspended);
void __pme_object_invoke_tasklet(struct pme_object *);

/* Enable or disable this object as "top dog" for the channel - this is a
 * channel mechanism that allows commands from only one object. If another
 * object has already locked the channel, this function will block until it
 * obtains the lock. Calling this function with 'enable'=0 will release this
 * object's lock on the channel. Returns zero on success (it can fail if
 * abortions start occuring before it obtained the lock). */
int pme_object_topdog(struct pme_object *, int enable);

/* Enable or disable residue for this object - when disabling, the call will
 * block until there are no outstanding uses of the object (existing commands
 * complete). The caller must make sure however not to race *new* commands
 * against this call. It is the caller's responsibility to manage any/all
 * associations between stream contexts and residues (ie. in a higher
 * "patter-matching" layer), this API provides a context/residue bind purely
 * for the purposes of allocation management (and hiding the distinctions
 * between dynamic versus tabular residue allocation). */
int pme_object_residue(struct pme_object *, int enable, u64 *residue);

/***************/
/* pme_data API */
/***************/

/* Some helpers for 'enum pme_data_type' */
static inline int pme_data_input(struct pme_data *p)
{
	int val = (p->type <= data_in_fbchain);
	return val;
}
static inline int pme_data_output(struct pme_data *p)
{
	int val = (p->type >= data_out_normal);
	return val;
}

/***************/
/* IRQ hacking */
/***************/

int pme_hal_init(void);
void pme_hal_finish(void);

/* This enum needs to match the order of IRQ resources parsed from the
 * device-tree for the SoC node. Edit with but the upmost caution. */
enum pme_hal_irq {
	pme_hal_irq_ctrl,
	pme_hal_irq_ch0,
	pme_hal_irq_ch1,
	pme_hal_irq_ch2,
	pme_hal_irq_ch3
};

#define PME_CH2IRQ(c)	(pme_hal_irq_ch0 + (c))

/* For the PCI-based CDS, we need to demux interrupts - see pci.h */
int REQUEST_IRQ(enum pme_hal_irq irq, irq_handler_t handler,
		unsigned long flags, const char *devname, void *dev_id);
void FREE_IRQ(enum pme_hal_irq irq, void *dev_id);

/* The HAL code manages differences between FPGA-over-PCI and SoC modes, but we
 * export the is_SoC global to allow run-time selection of constants outside
 * the HAL that depend on the mode. */
extern int is_SoC;

/*************/
/* Constants */
/*************/

#define PME_DMA_CONTEXT_SIZE		64
#define PME_DMA_CACHELINE		32

/* Constants used by the SRE ioctl. */
#define PME_DMA_SRE_POLL_MS		100
#define PME_DMA_SRE_INDEX_MAX		(1 << 26)
#define PME_DMA_SRE_INC_MAX		(1 << 12)
#define PME_DMA_SRE_REP_MAX		(1 << 24)
#define PME_DMA_SRE_INTERVAL_MAX	(1 << 12)

#define PME_DMA_EOSRP_MASK		(0x0003FFFF)
#define PME_DMA_MCL_MASK		(0x00007FFF)
/* Hard-coded CC configuration settings; */
/*    FBM memory transaction priority, snooping, read-safe disable (0 indicates
 *    read-safe enabled) */
#define PME_DMA_FBM_MTP			(is_SoC ? 0 : 0)
#define PME_DMA_FBM_SNOOP		(is_SoC ? 1 : 2)
#define PME_DMA_FBM_RDUNSAFE		(is_SoC ? 0 : 1)
/*    DXE pattern range counter config, read-safe disable, instruction read
 *    priority, snooping, error config */
#define PME_DMA_DXE_DRCC		0
#define PME_DMA_DXE_RDUNSAFE		0
#define PME_DMA_DXE_MTP			(is_SoC ? 0 : 2)
#define PME_DMA_DXE_SNOOP		(is_SoC ? 1 : 0)
#define PME_DMA_DXE_ERRORCONFIG		(u32)0xFFFFFFFF
/*    SRE code {snooping, priority, read-safe disable}, metastate read {snoop,
 *    priority}, metastate write {snoop, priority}, metastate read-safe
 *    disable, EOS reaction ptr (18 bits), error config (1-3), free running
 *    counter configuration */
#define PME_DMA_SRE_CODE_SNOOP		(is_SoC ? 1 : 0)
#define PME_DMA_SRE_CODE_MTP		(is_SoC ? 0 : 1)
#define PME_DMA_SRE_CODE_RDUNSAFE	(is_SoC ? 0 : 0)
#define PME_DMA_SRE_M_RD_SNOOP		(is_SoC ? 1 : 0)
#define PME_DMA_SRE_M_RD_MTP		(is_SoC ? 0 : 0)
#define PME_DMA_SRE_M_WR_SNOOP		(is_SoC ? 1 : 0)
#define PME_DMA_SRE_M_WR_MTP		(is_SoC ? 0 : 3)
#define PME_DMA_SRE_M_RDUNSAFE		(is_SoC ? 0 : 0)
#define PME_DMA_SRE_EOS_PTR		0
#define PME_DMA_SRE_ERRORCONFIG1	(u32)0xFFFFFFFF
#define PME_DMA_SRE_FRCCONFIG		333
/*    KES error config */
#define PME_DMA_KES_ERRORCONFIG		(u32)0x00007FFF

/* Hard-coded channel configuration settings; */
/*    Sync time for ODD disables */
#define PME_DMA_CH_ODDSYNC_MS		(is_SoC ? 10 : 100)
/*    Time between going in to and out of reset */
#define PME_DMA_CH_RESET_MS		(is_SoC ? 10 : 100)
/*    Time to allow FB thread to exit */
#define PME_DMA_CH_UNLOAD_MS		100
/*    Time after cmd FIFO disable to wait for DE to cease servicing */
#define PME_DMA_CH_DISABLE_MS		1000
/*    Cache snooping; command FIFO, FB FIFO, scan output, input, deflate
 *    output, residue, context, notification FIFO */
#define PME_DMA_CH_SNOOP_CMD		(is_SoC ? 1 : 0)
#define PME_DMA_CH_SNOOP_FB		(is_SoC ? 1 : 0)
#define PME_DMA_CH_SNOOP_REPORT		(is_SoC ? 1 : 0)
#define PME_DMA_CH_SNOOP_INPUT		(is_SoC ? 1 : 0)
#define PME_DMA_CH_SNOOP_DEFLATE	(is_SoC ? 1 : 0)
#define PME_DMA_CH_SNOOP_RESIDUE	(is_SoC ? 1 : 0)
#define PME_DMA_CH_SNOOP_CONTEXT	(is_SoC ? 1 : 0)
#define PME_DMA_CH_SNOOP_NOTIFY		(is_SoC ? 1 : 0)
/*    Memory transaction priorities; deflate output, scan output, input,
 *    residue, context, command FIFO, notification FIFO, FB FIFO */
#define PME_DMA_CH_MTP_DEFLATE		(is_SoC ? 0 : 1)
#define PME_DMA_CH_MTP_REPORT		(is_SoC ? 0 : 1)
#define PME_DMA_CH_MTP_INPUT		(is_SoC ? 0 : 0)
#define PME_DMA_CH_MTP_RESIDUE		(is_SoC ? 0 : 0)
#define PME_DMA_CH_MTP_CONTEXT		(is_SoC ? 0 : 0)
#define PME_DMA_CH_MTP_CMD		(is_SoC ? 0 : 1)
#define PME_DMA_CH_MTP_NOTIFY		(is_SoC ? 0 : 1)
#define PME_DMA_CH_MTP_FB		(is_SoC ? 0 : 1)

/* Stats sleeper(s) use this as a context */
struct stat_sleeper {
	struct list_head list;
	wait_queue_head_t queue;
	int woken;
};

/*************************************/
/* Global (master) device - module.c */
/*************************************/

void pme_wake_stats(void);

/* Memory resources */
dma_addr_t pme_mem_phys_alloc(size_t sz);
void pme_mem_phys_free(dma_addr_t addr);

/* Global Device */
void pme_cds_dev_set(struct device *dev);

/****************************************/
/* Hardware abstraction stuff (PCI/SoC) */
/****************************************/

/* Register space */
struct pme_regmap;
int pme_regmap_init(void);
void pme_regmap_finish(void);
/* The register space is divided into 5 4Kb regions. */
struct pme_regmap *pme_regmap_map(unsigned int region);
void pme_regmap_unmap(struct pme_regmap *mapped);
/* WARNING! You must use these get/set macros when reading/writing registers as
 * they are needed to guarantee that only 32-bit memory accesses are used.
 * Otherwise the compiler feels free to optimise and this blows up. 'idx' is
 * the index in 32-bit increments. */
static inline u32 reg_get(struct pme_regmap *map, unsigned int idx)
{
	u32 ret;
	u32 *ptr = (u32 *)map + idx;
	ret = (is_SoC ? in_be32(ptr) : in_le32(ptr));
	return ret;
}
static inline void reg_set(struct pme_regmap *map, unsigned int idx, u32 val)
{
	u32 *ptr = (u32 *)map + idx;
	if (is_SoC)
		out_be32(ptr, val);
	else
		out_le32(ptr, val);
}
/* These helpers assume two registers side-by-side form a 64-bit value
 * (high-word first). */
static inline u64 reg_get64(struct pme_regmap *map, unsigned int idx)
{
	return ((u64)reg_get(map, idx) << 32) | (u64)reg_get(map, idx + 1);
}
static inline void reg_set64(struct pme_regmap *map, unsigned int idx, u64 val)
{
	reg_set(map, idx, (u32)(val >> 32));
	reg_set(map, idx + 1, (u32)(val & 0xFFFFFFFF));
}

/*******************************/
/* Channel control - channel.c */
/*******************************/

/* Basic channel functions used by module.c (kill/reset are exported APIs and
 * so not listed here). */
int pme_channel_init(struct pme_channel **, struct pme_ctrl_load *p);
int pme_channel_finish(struct pme_channel *);
int pme_channel_ioctl(struct pme_channel *, int cmd,
			unsigned long arg);

/* This type is embedded within objects for "table"-allocated resources and the
 * channel allocation APIs for object.h use it. It ensures we can deallocate
 * without having to convert from dma_addr_t to void* (which we can't do in,
 * for example, PCI). */
struct __pme_dma_resource {
	dma_addr_t addr;
	void *ptr;
};
/* This is a placeholder "null" value for dma_addr_t. It's an impossible
 * address for our uses, by the simple fact that it isn't aligned. */
#define EMPTY_DMA_ADDR	((dma_addr_t)-1)
static inline void pme_dma_resource_init(struct __pme_dma_resource *res)
{
	res->addr = EMPTY_DMA_ADDR;
}
static inline int pme_dma_resource_empty(const struct __pme_dma_resource *res)
{
	int val = (res->addr == EMPTY_DMA_ADDR);
	return val;
}

/**************************/
/* Free-buffer management */
/**************************/

/* All '_ext' elements use this enumeration (notification entries too) */
enum fifo_ext {
	EXT_CONTIGUOUS = 0x0,
	EXT_SCATTERGATHER = 0x1
};
/* Same for '_fmt' */
enum fifo_fmt {
	BV = 0x0,
	LL = 0x1
};
/* These are memory overlay structures used by fbchains */
struct __ll_ned {
	u64	phys;
	u64	virt;
	u32	offset;
	u32	bufflen;
};
struct __sg_entry {
	u64	addr;
	u32	bufflen;
	u32	__extoffset;
};

struct pme_fbchain {
	struct pme_channel *channel;
	u8			freelist_id;
	enum pme_fbchain_e	type;
	struct list_head	list;
	/* Overall chain data */
	size_t			data_len;
	unsigned int		num_blocks;
	size_t			blocksize;
	/* linked-list or scatter-gather state */
	union {
		struct {
			struct __ll_ned		head;
			void *tail_virt;
			u64			tail_phys;
			/* iterator state */
			unsigned int		idx;
			size_t			bytes;
			struct __ll_ned *cursor;
		} ll;
		struct {
			/* This entry has a virtual "addr" element */
			struct __sg_entry	head;
			u64			phys;
			unsigned int 		tbls;
			/* iterator state */
			unsigned int		blks_left;
			size_t			bytes;
			struct __sg_entry *cursor;
		} sg;
	};
};

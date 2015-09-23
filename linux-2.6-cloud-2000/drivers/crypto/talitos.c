/*
 * talitos - Freescale Integrated Security Engine (SEC) device driver
 *
 * Copyright (c) 2008-2011 Freescale Semiconductor, Inc.
 *
 * Scatterlist Crypto API glue code copied from files with the following:
 * Copyright (c) 2006-2007 Herbert Xu <herbert@gondor.apana.org.au>
 *
 * Crypto algorithm registration code copied from hifn driver:
 * 2007+ Copyright (c) Evgeniy Polyakov <johnpol@2ka.mipt.ru>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/crypto.h>
#include <linux/hw_random.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/rtnetlink.h>
#include <linux/slab.h>
#include <linux/dmaengine.h>
#include <linux/raid/xor.h>

#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <crypto/des.h>
#include <crypto/sha.h>
#include <crypto/md5.h>
#include <crypto/aead.h>
#include <crypto/authenc.h>
#include <crypto/skcipher.h>
#include <crypto/hash.h>
#include <crypto/internal/hash.h>
#include <crypto/scatterwalk.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>

#include "talitos.h"
#ifdef CONFIG_AS_FASTPATH
struct secfp_ivInfo_s {
	dma_addr_t paddr;
	unsigned long *vaddr;
	unsigned long ulIVIndex;
	bool bUpdatePending;
	unsigned int ulNumAvail;
	unsigned int ulUpdateIndex;
} secfp_ivInfo_s;
#define SECFP_NUM_IV_DATA_GET_AT_ONE_TRY 1
#define SECFP_NUM_IV_ENTRIES 8
#endif

#define TALITOS_TIMEOUT 100000
#define TALITOS_MAX_DATA_LEN 65535
#define MAX_CHAN	4
#define MAX_GROUPS	2

#define DESC_TYPE(desc_hdr) ((be32_to_cpu(desc_hdr) >> 3) & 0x1f)
#define PRIMARY_EU(desc_hdr) ((be32_to_cpu(desc_hdr) >> 28) & 0xf)
#define SECONDARY_EU(desc_hdr) ((be32_to_cpu(desc_hdr) >> 16) & 0xf)

#define MAP_ARRAY(chan_no)	(3 << (chan_no * 2))
#define MAP_ARRAY_DONE(chan_no)	(1 << (chan_no * 2))
#define MAX_DESC_LEN   160

#ifdef CONFIG_AS_FASTPATH
static struct device *pg_talitos_dev;
static struct talitos_private *pg_talitos_privdata;
#endif

static const struct talitos_ptr zero_entry = {
	.len = 0,
	.j_extent = 0,
	.eptr = 0,
	.ptr = 0
};


/**
 * talitos_request - descriptor submission request
 * @desc: descriptor pointer (kernel virtual)
 * @dma_desc: descriptor's physical bus address
 * @callback: whom to call when descriptor processing is done
 * @context: caller context (optional)
 */
struct talitos_request {
	struct talitos_desc *desc;
	dma_addr_t dma_desc;
	void (*callback) (struct device *dev, struct talitos_desc *desc,
	                  void *context, int error);
	void *context;
};

/* per-channel fifo management */
struct talitos_channel {
	/* request fifo */
	struct talitos_request *fifo;
	/* number of requests pending in channel h/w fifo */
	int submit_count;
	/* index to next free descriptor request */
	u8 head;
	/* index to next in-progress/done descriptor request */
	u8 tail;
	/* Channel id */
	u8 id;
	struct talitos_private *priv;
};

struct talitos_private {
	struct device *dev;
	struct of_device *ofdev;
	void __iomem *reg;
	int irq[MAX_GROUPS];
	int dual;
	int secondary;

	/* SEC version geometry (from device tree node) */
	unsigned int num_channels;
	unsigned int chfifo_len;
	unsigned int exec_units;
	unsigned int desc_types;
	unsigned int chan_remap;

	/* SEC Compatibility info */
	unsigned long features;

	/*
	 * length of the request fifo
	 * fifo_len is chfifo_len rounded up to next power of 2
	 * so we can use bitwise ops to wrap
	 */
	unsigned int fifo_len;

	struct talitos_channel *chan;

	/* next channel to be assigned next incoming
		descriptor */
	u8 last_chan[MAX_GROUPS];
	u32 chan_isr[MAX_GROUPS];
	u32 chan_imr[MAX_GROUPS];
	/* number of channels mapped to a core */
	u8 core_num_chan[MAX_GROUPS];
	/* channels numbers of channels mapped to a core */
	u8 core_chan_no[MAX_GROUPS][MAX_CHAN] ____cacheline_aligned;
	/* pointer to the cache pool */
	struct kmem_cache *netcrypto_cache;
	/* pointer to edescriptor recycle queue */
	struct talitos_edesc *edesc_rec_queue[NR_CPUS][MAX_IPSEC_RECYCLE_DESC];
	/* index in edesc recycle queue */
	u8 curr_edesc[MAX_GROUPS];

	/* request callback napi */
	struct napi_struct *done_task;

	/* list of registered algorithms */
	struct list_head alg_list;

	/* hwrng device */
	struct hwrng rng;
	bool bRngInit;
#ifdef CONFIG_AS_FASTPATH
	atomic_t ulRngInUse;
#endif
	/* XOR Device */
	struct dma_device dma_dev_common;
};

/*
 * talitos_edesc - s/w-extended descriptor
 * @src_nents: number of segments in input scatterlist
 * @dst_nents: number of segments in output scatterlist
 * @dma_len: length of dma mapped link_tbl space
 * @dma_link_tbl: bus physical address of link_tbl
 * @desc: h/w descriptor
 * @link_tbl: input and output h/w link tables (if {src,dst}_nents > 1)
 *
 * if decrypting (with authcheck), or either one of src_nents or dst_nents
 * is greater than 1, an integrity check value is concatenated to the end
 * of link_tbl data
 */
struct talitos_edesc {
	int src_nents;
	int dst_nents;
	int src_is_chained;
	int dst_is_chained;
	int dma_len;
	dma_addr_t dma_link_tbl;
	struct talitos_desc desc;
	struct talitos_ptr link_tbl[0];
};

/* .features flag */
#define TALITOS_FTR_SRC_LINK_TBL_LEN_INCLUDES_EXTENT 0x00000001
#define TALITOS_FTR_HW_AUTH_CHECK 0x00000002
#define TALITOS_FTR_SHA224_HWINIT 0x00000004

struct talitos_edesc *crypto_edesc_alloc(int len, int flags,
					struct talitos_private *priv)
{
	int check;
	struct talitos_edesc *ret;
	u32 smp_processor_id = smp_processor_id();
	check = in_softirq();

	if (!check)
		local_bh_disable();

	u32 current_edesc = priv->curr_edesc[smp_processor_id];
	if (unlikely(current_edesc == 0)) {
		ret = kmem_cache_alloc(priv->netcrypto_cache,
					GFP_KERNEL | flags);
	} else {
		 priv->curr_edesc[smp_processor_id] = current_edesc - 1;
		ret = priv->edesc_rec_queue[smp_processor_id]
					[current_edesc - 1];
	}

	if (!check)
		local_bh_enable();

	return ret;
}
void crypto_edesc_free(struct talitos_edesc *edesc,
			struct talitos_private *priv)
{
	int check;
	u32 smp_processor_id = smp_processor_id();
	check = in_softirq();

	if (!check)
		local_bh_disable();

	u32 current_edesc = priv->curr_edesc[smp_processor_id];
	if (unlikely(current_edesc == (MAX_IPSEC_RECYCLE_DESC - 1))) {
		kmem_cache_free(priv->netcrypto_cache, edesc);
	} else {
		priv->edesc_rec_queue[smp_processor_id][current_edesc] =
								edesc;
		priv->curr_edesc[smp_processor_id] = current_edesc + 1;
	}

	if (!check)
		local_bh_enable();
}

static inline unsigned int get_chan_remap(struct talitos_private *priv)
{
	return priv->chan_remap;
}

static inline int is_channel_alt(int ch, struct talitos_private *priv)
{
	return ((1 << (priv->num_channels - ch - 1)) & get_chan_remap(priv));
}

static inline int is_channel_used(int ch, struct talitos_private *priv)
{
	if (priv->dual)
		return 1;

	if (priv->secondary)
		return is_channel_alt(ch, priv);
	else
		return !is_channel_alt(ch, priv);
}

static inline int get_grp_id(struct talitos_private *priv)
{
	return priv->dual ? smp_processor_id() : priv->secondary;
}

static void to_talitos_ptr(struct talitos_ptr *talitos_ptr, dma_addr_t dma_addr)
{
	talitos_ptr->ptr = cpu_to_be32(lower_32_bits(dma_addr));
	talitos_ptr->eptr = cpu_to_be32(upper_32_bits(dma_addr));
}

/*
 * map virtual single (contiguous) pointer to h/w descriptor pointer
 */
static void map_single_talitos_ptr(struct device *dev,
				   struct talitos_ptr *talitos_ptr,
				   unsigned short len, void *data,
				   unsigned char extent,
				   enum dma_data_direction dir)
{
	dma_addr_t dma_addr = dma_map_single(dev, data, len, dir);

	talitos_ptr->len = cpu_to_be16(len);
	to_talitos_ptr(talitos_ptr, dma_addr);
	talitos_ptr->j_extent = extent;
}

/*
 * unmap bus single (contiguous) h/w descriptor pointer
 */
static void unmap_single_talitos_ptr(struct device *dev,
				     struct talitos_ptr *talitos_ptr,
				     enum dma_data_direction dir)
{
	dma_unmap_single(dev, be32_to_cpu(talitos_ptr->ptr),
			 be16_to_cpu(talitos_ptr->len), dir);
}

static int reset_channel(struct device *dev, int ch)
{
	struct talitos_private *priv = dev_get_drvdata(dev);
	unsigned int timeout = TALITOS_TIMEOUT;

	setbits32(priv->reg + TALITOS_CCCR(ch, priv), TALITOS_CCCR_RESET);

	while ((in_be32(priv->reg + TALITOS_CCCR(ch, priv))
		& TALITOS_CCCR_RESET) && --timeout)
		cpu_relax();

	if (timeout == 0) {
		dev_err(dev, "failed to reset channel %d\n", ch);
		return -EIO;
	}

	/* set 36-bit addressing, done writeback enable and done IRQ enable */
	setbits32(priv->reg + TALITOS_CCCR_LO(ch, priv), TALITOS_CCCR_LO_EAE |
		  TALITOS_CCCR_LO_CDWE | TALITOS_CCCR_LO_CDIE);

	/* and ICCR writeback, if available */
	if (priv->features & TALITOS_FTR_HW_AUTH_CHECK)
		setbits32(priv->reg + TALITOS_CCCR_LO(ch, priv),
		          TALITOS_CCCR_LO_IWSE);

	return 0;
}

static int reset_device(struct device *dev)
{
	struct talitos_private *priv = dev_get_drvdata(dev);
	unsigned int timeout = TALITOS_TIMEOUT;

	setbits32(priv->reg + TALITOS_MCR, TALITOS_MCR_SWR);

	while ((in_be32(priv->reg + TALITOS_MCR) & TALITOS_MCR_SWR)
	       && --timeout)
		cpu_relax();

	if (timeout == 0) {
		dev_err(dev, "failed to reset device\n");
		return -EIO;
	}

	return 0;
}

/*
 * Reset and initialize the device
 */
static int init_device(struct device *dev)
{
	struct talitos_private *priv = dev_get_drvdata(dev);
	int ch, err;

	/*
	 * Master reset
	 * errata documentation: warning: certain SEC interrupts
	 * are not fully cleared by writing the MCR:SWR bit,
	 * set bit twice to completely reset
	 */
	err = reset_device(dev);
	if (err)
		return err;

	err = reset_device(dev);
	if (err)
		return err;

	if (priv->chan_remap)
		/* Remap channels */
		setbits32(priv->reg + TALITOS_MCR,
			(in_be32(priv->reg + TALITOS_MCR)
			| (priv->chan_remap << 12)));

	/* reset channels */
	for (ch = 0; ch < priv->num_channels; ch++) {
		err = reset_channel(dev, ch);
		if (err)
			return err;
	}

	/* enable channel done and error interrupts */
	setbits32(priv->reg + TALITOS_IMR, TALITOS_IMR_INIT);
	setbits32(priv->reg + TALITOS_IMR_LO, TALITOS_IMR_LO_INIT);

	/* disable integrity check error interrupts (use writeback instead) */
	if (priv->features & TALITOS_FTR_HW_AUTH_CHECK) {
		setbits32(priv->reg + TALITOS_MDEUICR_LO,
			  TALITOS_MDEUICR_LO_ICE);
#ifdef CONFIG_AS_FASTPATH
		printk(KERN_INFO "Masking ICV Error interrupt\r\n");
		setbits32(priv->reg + TALITOS_AESUICR_LO,
			  TALITOS_AESUICR_LO_ICE);
#endif
	} else {
		printk(KERN_INFO "Not setting ICE\r\n");
	}

	return 0;
}

/**
 * talitos_submit - submits a descriptor to the device for processing
 * @dev:	the SEC device to be used
 * @desc:	the descriptor to be processed by the device
 * @callback:	whom to call when processing is complete
 * @context:	a handle for use by caller (optional)
 *
 * desc must contain valid dma-mapped (bus physical) address pointers.
 * callback must check err and feedback in descriptor header
 * for device processing status.
 */
static int talitos_submit(struct device *dev, struct talitos_desc *desc,
			  void (*callback)(struct device *dev,
					   struct talitos_desc *desc,
					   void *context, int error),
			  void *context)
{
	struct talitos_private *priv = dev_get_drvdata(dev);
	struct talitos_request *request;
	u8 ch;
	int grp_id = get_grp_id(priv);

	u8 head, last_chan, total_chan;
	if (priv->core_num_chan[grp_id] > 0) {
		total_chan = priv->core_num_chan[grp_id];
		last_chan = priv->last_chan[grp_id];
		/* select done notification */
		desc->hdr |= DESC_HDR_DONE_NOTIFY;

		if (last_chan <  total_chan) {
			ch = priv->core_chan_no[grp_id][last_chan];
			priv->last_chan[grp_id]++;
		} else {
			ch = priv->core_chan_no[grp_id][0];
			priv->last_chan[grp_id] = 1;
		}
		if (priv->chan[ch].submit_count != 0)
			++priv->chan[ch].submit_count;
		else
			/* h/w fifo is full */
			return -EAGAIN;

		head = priv->chan[ch].head;
		request = &priv->chan[ch].fifo[head];

		/* map descriptor and save caller data */
		request->dma_desc = dma_map_single(dev, desc, sizeof(*desc),
					   DMA_BIDIRECTIONAL);
		request->callback = callback;
		request->context = context;

		/* increment fifo head */
		priv->chan[ch].head = (priv->chan[ch].head + 1) &
					(priv->fifo_len - 1);

		smp_wmb();
		request->desc = desc;

		/* GO! */
		wmb();
		out_be32(priv->reg + TALITOS_FF(ch, priv),
			cpu_to_be32(upper_32_bits(request->dma_desc)));
		out_be32(priv->reg + TALITOS_FF_LO(ch, priv),
			cpu_to_be32(lower_32_bits(request->dma_desc)));
		return -EINPROGRESS;
	} else {
		return -EAGAIN;
	}
}

#ifdef CONFIG_AS_FASTPATH
int secfp_talitos_submit(struct device *dev, struct talitos_desc *desc,
	void (*callback) (struct device *dev, struct talitos_desc *desc,
	void *context, int err), void *context)
{
	return talitos_submit(dev, desc, callback, context);
}
EXPORT_SYMBOL(secfp_talitos_submit);
#endif /* CONFIG_AS_FASTPATH */

/*
 * process what was done, notify callback of error if not
 */
static int flush_channel(struct talitos_channel *chan, int error,
			int reset_ch, int weight)
{
	struct talitos_private *priv = chan->priv;
	struct device *dev = &priv->ofdev->dev;
	struct talitos_request *request, saved_req;
	int tail, status;
	u8 count = 0;

	tail = chan->tail;
	while (chan->fifo[tail].desc && (count < weight)) {
		request = &chan->fifo[tail];

		/* descriptors with their done bits set don't get the error */
		rmb();
		if ((request->desc->hdr & DESC_HDR_DONE) == DESC_HDR_DONE)
			status = 0;
		else
			if (!error)
				break;
			else
				status = error;

		dma_unmap_single(dev, request->dma_desc,
				 sizeof(struct talitos_desc),
				 DMA_BIDIRECTIONAL);

		/* copy entries so we can call callback outside lock */
		saved_req.desc = request->desc;
		saved_req.callback = request->callback;
		saved_req.context = request->context;

		/* release request entry in fifo */
		smp_wmb();
		request->desc = NULL;

		/* increment fifo tail */
		chan->tail  = (tail + 1) & (priv->fifo_len - 1);
		chan->submit_count -= 1;
		saved_req.callback(dev, saved_req.desc, saved_req.context,
				   status);
		count++;
		/* channel may resume processing in single desc error case */
		if (error && !reset_ch && status == error)
			return 0;
		tail = chan->tail;
	}
return count;
}

/*
 * process completed requests for channels that have done status
 */
static int talitos_done(struct napi_struct *napi, int budget)
{
	struct device *dev = &napi->dev->dev;
	struct talitos_private *priv = dev_get_drvdata(dev);
	u8 ch, num_chan;
	u8 budget_per_channel = 0, work_done = 0, ret = 1;
	int grp_id = get_grp_id(priv);

	if (priv->core_num_chan[grp_id] > 0) {
		num_chan =  priv->core_num_chan[grp_id];
		budget_per_channel = budget/num_chan;
		for (ch = 0; ch < num_chan; ch++)
			work_done += flush_channel(priv->chan +
					priv->core_chan_no[grp_id][ch]
					, 0, 0, budget_per_channel);
		if (work_done < budget) {
			napi_complete(per_cpu_ptr(priv->done_task,
						smp_processor_id()));
			/* At this point, all completed channels have been
			 * processed.
			 * Unmask done intrpts for channels completed later on.
			 */
			setbits32(priv->reg + TALITOS_IMR, TALITOS_IMR_INIT);
			setbits32(priv->reg + TALITOS_IMR_LO,
				TALITOS_IMR_LO_INIT);
			ret = 0;
		}
		return ret;
	}
	return 0;
}

/*
 * locate current (offending) descriptor
 */
static struct talitos_desc *current_desc(struct device *dev, int ch)
{
	struct talitos_private *priv = dev_get_drvdata(dev);
	int tail = priv->chan[ch].tail;
	dma_addr_t cur_desc;

	cur_desc = in_be32(priv->reg + TALITOS_CDPR_LO(ch, priv));

	while (priv->chan[ch].fifo[tail].dma_desc != cur_desc) {
		tail = (tail + 1) & (priv->fifo_len - 1);
		if (tail == priv->chan[ch].tail) {
			dev_err(dev, "couldn't locate current descriptor\n");
			return NULL;
		}
	}

	return priv->chan[ch].fifo[tail].desc;
}

/*
 * user diagnostics; report root cause of error based on execution unit status
 */
static void report_eu_error(struct device *dev, int ch,
			    struct talitos_desc *desc)
{
	struct talitos_private *priv = dev_get_drvdata(dev);
	int i;

	switch (desc->hdr & DESC_HDR_SEL0_MASK) {
	case DESC_HDR_SEL0_AFEU:
		dev_err(dev, "AFEUISR 0x%08x_%08x\n",
			in_be32(priv->reg + TALITOS_AFEUISR),
			in_be32(priv->reg + TALITOS_AFEUISR_LO));
		break;
	case DESC_HDR_SEL0_DEU:
		dev_err(dev, "DEUISR 0x%08x_%08x\n",
			in_be32(priv->reg + TALITOS_DEUISR),
			in_be32(priv->reg + TALITOS_DEUISR_LO));
		break;
	case DESC_HDR_SEL0_MDEUA:
	case DESC_HDR_SEL0_MDEUB:
		dev_err(dev, "MDEUISR 0x%08x_%08x\n",
			in_be32(priv->reg + TALITOS_MDEUISR),
			in_be32(priv->reg + TALITOS_MDEUISR_LO));
		break;
	case DESC_HDR_SEL0_RNG:
		dev_err(dev, "RNGUISR 0x%08x_%08x\n",
			in_be32(priv->reg + TALITOS_RNGUISR),
			in_be32(priv->reg + TALITOS_RNGUISR_LO));
		break;
	case DESC_HDR_SEL0_PKEU:
		dev_err(dev, "PKEUISR 0x%08x_%08x\n",
			in_be32(priv->reg + TALITOS_PKEUISR),
			in_be32(priv->reg + TALITOS_PKEUISR_LO));
		break;
	case DESC_HDR_SEL0_AESU:
		dev_err(dev, "AESUISR 0x%08x_%08x\n",
			in_be32(priv->reg + TALITOS_AESUISR),
			in_be32(priv->reg + TALITOS_AESUISR_LO));
		break;
	case DESC_HDR_SEL0_CRCU:
		dev_err(dev, "CRCUISR 0x%08x_%08x\n",
			in_be32(priv->reg + TALITOS_CRCUISR),
			in_be32(priv->reg + TALITOS_CRCUISR_LO));
		break;
	case DESC_HDR_SEL0_KEU:
		dev_err(dev, "KEUISR 0x%08x_%08x\n",
			in_be32(priv->reg + TALITOS_KEUISR),
			in_be32(priv->reg + TALITOS_KEUISR_LO));
		break;
	}

	switch (desc->hdr & DESC_HDR_SEL1_MASK) {
	case DESC_HDR_SEL1_MDEUA:
	case DESC_HDR_SEL1_MDEUB:
		dev_err(dev, "MDEUISR 0x%08x_%08x\n",
			in_be32(priv->reg + TALITOS_MDEUISR),
			in_be32(priv->reg + TALITOS_MDEUISR_LO));
		break;
	case DESC_HDR_SEL1_CRCU:
		dev_err(dev, "CRCUISR 0x%08x_%08x\n",
			in_be32(priv->reg + TALITOS_CRCUISR),
			in_be32(priv->reg + TALITOS_CRCUISR_LO));
		break;
	}

	for (i = 0; i < 8; i++)
		dev_err(dev, "DESCBUF 0x%08x_%08x\n",
		in_be32(priv->reg + TALITOS_DESCBUF(ch, priv) + 8*i),
		in_be32(priv->reg + TALITOS_DESCBUF_LO(ch, priv) + 8*i));
}

static void handle_error(struct talitos_channel *chan, u32 isr, u32 isr_lo)
{
	struct talitos_private *priv = chan->priv;
	struct device *dev = &priv->ofdev->dev;
	unsigned int timeout = TALITOS_TIMEOUT;
	int error, reset_dev = 0, reset_ch = 0;
	u32 v, v_lo;

	error = -EINVAL;

	v = in_be32(priv->reg + TALITOS_CCPSR(chan->id, priv));
	v_lo = in_be32(priv->reg + TALITOS_CCPSR_LO(chan->id, priv));

	if (v_lo & TALITOS_CCPSR_LO_DOF) {
		dev_err(dev, "double fetch fifo overflow error\n");
		error = -EAGAIN;
		reset_ch = 1;
	}
	if (v_lo & TALITOS_CCPSR_LO_SOF) {
		/* h/w dropped descriptor */
		dev_err(dev, "single fetch fifo overflow error\n");
		error = -EAGAIN;
	}
	if (v_lo & TALITOS_CCPSR_LO_MDTE)
		dev_err(dev, "master data transfer error\n");
	if (v_lo & TALITOS_CCPSR_LO_SGDLZ)
		dev_err(dev, "s/g data length zero error\n");
	if (v_lo & TALITOS_CCPSR_LO_FPZ)
		dev_err(dev, "fetch pointer zero error\n");
	if (v_lo & TALITOS_CCPSR_LO_IDH)
		dev_err(dev, "illegal descriptor header error\n");
	if (v_lo & TALITOS_CCPSR_LO_IEU)
		dev_err(dev, "invalid execution unit error\n");
	if (v_lo & TALITOS_CCPSR_LO_EU)
		report_eu_error(dev, chan->id, current_desc(dev, chan->id));
	if (v_lo & TALITOS_CCPSR_LO_GB)
		dev_err(dev, "gather boundary error\n");
	if (v_lo & TALITOS_CCPSR_LO_GRL)
		dev_err(dev, "gather return/length error\n");
	if (v_lo & TALITOS_CCPSR_LO_SB)
		dev_err(dev, "scatter boundary error\n");
	if (v_lo & TALITOS_CCPSR_LO_SRL)
		dev_err(dev, "scatter return/length error\n");

	flush_channel(chan, error, reset_ch, priv->fifo_len);

	if (reset_ch) {
		reset_channel(dev, chan->id);
	} else {
		setbits32(priv->reg + TALITOS_CCCR(chan->id, priv),
			  TALITOS_CCCR_CONT);
		setbits32(priv->reg + TALITOS_CCCR_LO(chan->id, priv), 0);
		while ((in_be32(priv->reg + TALITOS_CCCR(chan->id, priv)) &
		       TALITOS_CCCR_CONT) && --timeout)
			cpu_relax();
		if (timeout == 0) {
			dev_err(dev, "failed to restart channel %d\n",
				chan->id);
			reset_dev = 1;
		}
	}

	if (reset_dev || isr & ~(MAP_ARRAY(chan->id) - MAP_ARRAY_DONE(chan->id))
			|| isr_lo) {
		dev_err(dev, "done overflow, internal time out, or rngu error: "
		        "ISR 0x%08x_%08x\n", isr, isr_lo);

		/* purge request queues */
		flush_channel(chan, -EIO, 1, priv->fifo_len);

		/* reset and reinitialize the device */
		if (reset_dev)
			init_device(dev);
	}
}

/*
 * recover from error interrupts
 */
static void talitos_error(void *data, u32 isr, u32 isr_lo)
{
	struct talitos_private *priv = data;
	u8 i = 0;
	int grp_id = get_grp_id(priv);

	if (priv->core_num_chan[grp_id] > 0)
		for (i = 0; i < priv->core_num_chan[grp_id]; i++) {
			if (isr & (1 <<
				((priv->core_chan_no[grp_id][i] << 1)
				+ 1)))
				handle_error(priv->chan +
					priv->core_chan_no[grp_id][i],
					isr, isr_lo);
		}
}

static irqreturn_t talitos_interrupt(int irq, void *data)
{
	struct talitos_private *priv = data;
	u32 isr, isr_lo, isr_ack = 0;
	u32 intr_mask = 0, isr_ack1 = 0;
	int grp_id = get_grp_id(priv);
	isr = in_be32(priv->reg + TALITOS_ISR);
	isr_lo = in_be32(priv->reg + TALITOS_ISR_LO);

	if (priv->core_num_chan[grp_id] > 0) {
		intr_mask = priv->chan_imr[grp_id];
		isr_ack = 0xffffff00 | priv->chan_isr[grp_id];
		isr = isr & isr_ack;
		/* Acknowledge interrupt */
		out_be32(priv->reg + TALITOS_ICR, isr);
		out_be32(priv->reg + TALITOS_ICR_LO, isr_lo);
		if (unlikely((isr & ~intr_mask) || isr_lo)) {
			/* mask further done interrupts. */
			clrbits32(priv->reg + TALITOS_IMR, isr_ack1);
			talitos_error(data, isr, isr_lo);
		} else {
			if (likely(isr &  intr_mask)) {
				/* mask further done interrupts.  */
				clrbits32(priv->reg + TALITOS_IMR, intr_mask);
				/* Schdeule  respective napi */
				if (napi_schedule_prep(
					per_cpu_ptr(priv->done_task,
						smp_processor_id())))
					__napi_schedule(
						per_cpu_ptr(priv->done_task,
						smp_processor_id()));
			}
		}
	} else {
		/* Acknowledge interrupt */
		out_be32(priv->reg + TALITOS_ICR, isr);
		out_be32(priv->reg + TALITOS_ICR_LO, isr_lo);
	}
	return (isr || isr_lo) ? IRQ_HANDLED : IRQ_NONE;
}

/*
 * hwrng
 */
#ifdef CONFIG_AS_FASTPATH
/* nr_entries = number of 32 bit entries */
#define SECFP_IV_DATA_LO_THRESH 2
int secfp_rng_read_data(struct secfp_ivInfo_s *ptr)
{
	struct device *dev = pg_talitos_dev;
	struct talitos_private *priv = dev_get_drvdata(dev);
	u32 ii, ofl;

	if (ptr && ptr->ulNumAvail < SECFP_IV_DATA_LO_THRESH) {
		while (!atomic_add_unless(&priv->ulRngInUse, 1, 1))
			;

		ofl = in_be32(priv->reg + TALITOS_RNGUSR_LO) &
		TALITOS_RNGUSR_LO_OFL;
		ofl = ((ofl - 1) * 2) < SECFP_NUM_IV_DATA_GET_AT_ONE_TRY ?
			(ofl-1*2) : SECFP_NUM_IV_DATA_GET_AT_ONE_TRY;

		if (ofl) {
			for (ii = 0; ii < ofl; ii += 2) {
				ptr->vaddr[ptr->ulUpdateIndex] =
					in_be32(priv->reg + TALITOS_RNGU_FIFO);
				ptr->ulUpdateIndex = (ptr->ulUpdateIndex + 1)
					& (SECFP_NUM_IV_ENTRIES - 1);
				ptr->vaddr[ptr->ulUpdateIndex] = in_be32(
					priv->reg + TALITOS_RNGU_FIFO_LO);
				ptr->ulUpdateIndex = (ptr->ulUpdateIndex + 1) &
					(SECFP_NUM_IV_ENTRIES - 1);
			}
			ptr->ulNumAvail += (ofl*2);
		}
	}
	atomic_set(&priv->ulRngInUse, 0);
	return 0;
}
EXPORT_SYMBOL(secfp_rng_read_data);
#endif

static int talitos_rng_data_present(struct hwrng *rng, int wait)
{
	struct device *dev = (struct device *)rng->priv;
	struct talitos_private *priv = dev_get_drvdata(dev);
	u32 ofl;
	int i;

	for (i = 0; i < 20; i++) {
		ofl = in_be32(priv->reg + TALITOS_RNGUSR_LO) &
		      TALITOS_RNGUSR_LO_OFL;
		if (ofl || !wait)
			break;
		udelay(10);
	}

	return !!ofl;
}

static int talitos_rng_data_read(struct hwrng *rng, u32 *data)
{
	struct device *dev = (struct device *)rng->priv;
	struct talitos_private *priv = dev_get_drvdata(dev);
#ifdef CONFIG_AS_FASTPATH
	do {
		if (!atomic_add_unless(&priv->ulRngInUse, 1, 1))
			break;
	} while (1);
#endif
	/* rng fifo requires 64-bit accesses */
	*data = in_be32(priv->reg + TALITOS_RNGU_FIFO);
	*data = in_be32(priv->reg + TALITOS_RNGU_FIFO_LO);
#ifdef CONFIG_AS_FASTPATH
	atomic_set(&priv->ulRngInUse, 0);
#endif
	return sizeof(u32);
}

static int talitos_rng_init(struct hwrng *rng)
{
	struct device *dev = (struct device *)rng->priv;
	struct talitos_private *priv = dev_get_drvdata(dev);
	unsigned int timeout = TALITOS_TIMEOUT;
	if (priv->bRngInit)
		return 0;

	setbits32(priv->reg + TALITOS_RNGURCR_LO, TALITOS_RNGURCR_LO_SR);
	while (!(in_be32(priv->reg + TALITOS_RNGUSR_LO) & TALITOS_RNGUSR_LO_RD)
	       && --timeout)
		cpu_relax();
	if (timeout == 0) {
		dev_err(dev, "failed to reset rng hw\n");
		return -ENODEV;
	}

	/* start generating */
	setbits32(priv->reg + TALITOS_RNGUDSR_LO, 0);

	priv->bRngInit = 1;

	return 0;
}

static int talitos_register_rng(struct device *dev)
{
	struct talitos_private *priv = dev_get_drvdata(dev);

	priv->rng.name		= dev_driver_string(dev),
	priv->rng.init		= talitos_rng_init,
	priv->rng.data_present	= talitos_rng_data_present,
	priv->rng.data_read	= talitos_rng_data_read,
	priv->rng.priv		= (unsigned long)dev;

	return hwrng_register(&priv->rng);
}

static void talitos_unregister_rng(struct device *dev)
{
	struct talitos_private *priv = dev_get_drvdata(dev);

	hwrng_unregister(&priv->rng);
}

/*
 * async_tx interface for XOR-capable SECs
 *
 * Dipen Dudhat <Dipen.Dudhat@freescale.com>
 * Maneesh Gupta <Maneesh.Gupta@freescale.com>
 * Vishnu Suresh <Vishnu@freescale.com>
 */

/**
 * talitos_xor_chan - context management for the async_tx channel
 * @completed_cookie: the last completed cookie
 * @desc_lock: lock for tx queue
 * @total_desc: number of descriptors allocated
 * @submit_q: queue of submitted descriptors
 * @pending_q: queue of pending descriptors
 * @in_progress_q: queue of descriptors in progress
 * @free_desc: queue of unused descriptors
 * @dev: talitos device implementing this channel
 * @common: the corresponding xor channel in async_tx
 */
struct talitos_xor_chan {
	dma_cookie_t completed_cookie;
	spinlock_t desc_lock;
	unsigned int total_desc;
	struct list_head submit_q;
	struct list_head pending_q;
	struct list_head in_progress_q;
	struct list_head free_desc;
	struct device *dev;
	struct dma_chan common;
};

/**
 * talitos_xor_desc - software xor descriptor
 * @async_tx: the referring async_tx descriptor
 * @node:
 * @hwdesc: h/w descriptor
 */
struct talitos_xor_desc {
	struct dma_async_tx_descriptor async_tx;
	struct list_head tx_list;
	struct list_head node;
	struct talitos_desc hwdesc;
};

static void talitos_release_xor(struct device *dev, struct talitos_desc *hwdesc,
				void *context, int error);

static enum dma_status talitos_is_tx_complete(struct dma_chan *chan,
					      dma_cookie_t cookie,
					      struct dma_tx_state *state)
{
	struct talitos_xor_chan *xor_chan;
	dma_cookie_t last_used;
	dma_cookie_t last_complete;

	xor_chan = container_of(chan, struct talitos_xor_chan, common);

	last_used = chan->cookie;
	last_complete = xor_chan->completed_cookie;

	if (state->last)
		state->last = last_complete;

	if (state->used)
		state->used = last_used;

	return dma_async_is_complete(cookie, last_complete, last_used);
}

static void talitos_process_pending(struct talitos_xor_chan *xor_chan)
{
	struct talitos_xor_desc *desc, *_desc;
	unsigned long flags;
	int status;

	spin_lock_irqsave(&xor_chan->desc_lock, flags);

	list_for_each_entry_safe(desc, _desc, &xor_chan->pending_q, node) {
		status = talitos_submit(xor_chan->dev, &desc->hwdesc,
					talitos_release_xor, desc);
		if (status != -EINPROGRESS)
			break;

		list_del(&desc->node);
		list_add_tail(&desc->node, &xor_chan->in_progress_q);
	}

	spin_unlock_irqrestore(&xor_chan->desc_lock, flags);
}

static void talitos_release_xor(struct device *dev, struct talitos_desc *hwdesc,
				void *context, int error)
{
	struct talitos_xor_desc *desc = context;
	struct talitos_xor_chan *xor_chan;
	dma_async_tx_callback callback;
	void *callback_param;

	if (unlikely(error)) {
		dev_err(dev, "xor operation: talitos error %d\n", error);
		BUG();
	}

	xor_chan = container_of(desc->async_tx.chan, struct talitos_xor_chan,
				common);
	spin_lock_bh(&xor_chan->desc_lock);
	if (xor_chan->completed_cookie < desc->async_tx.cookie)
		xor_chan->completed_cookie = desc->async_tx.cookie;

	callback = desc->async_tx.callback;
	callback_param = desc->async_tx.callback_param;

	if (callback) {
		spin_unlock_bh(&xor_chan->desc_lock);
		callback(callback_param);
		spin_lock_bh(&xor_chan->desc_lock);
	}

	list_del(&desc->node);
	list_add_tail(&desc->node, &xor_chan->free_desc);
	spin_unlock_bh(&xor_chan->desc_lock);
	if (!list_empty(&xor_chan->pending_q))
		talitos_process_pending(xor_chan);
}

/**
 * talitos_issue_pending - move the descriptors in submit
 * queue to pending queue and submit them for processing
 * @chan: DMA channel
 */
static void talitos_issue_pending(struct dma_chan *chan)
{
	struct talitos_xor_chan *xor_chan;

	xor_chan = container_of(chan, struct talitos_xor_chan, common);
	spin_lock_bh(&xor_chan->desc_lock);
	list_splice_tail_init(&xor_chan->submit_q,
				 &xor_chan->pending_q);
	spin_unlock_bh(&xor_chan->desc_lock);
	talitos_process_pending(xor_chan);
}

static dma_cookie_t talitos_async_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct talitos_xor_desc *desc;
	struct talitos_xor_chan *xor_chan;
	dma_cookie_t cookie;

	desc = container_of(tx, struct talitos_xor_desc, async_tx);
	xor_chan = container_of(tx->chan, struct talitos_xor_chan, common);

	spin_lock_bh(&xor_chan->desc_lock);

	cookie = xor_chan->common.cookie + 1;
	if (cookie < 0)
		cookie = 1;

	desc->async_tx.cookie = cookie;
	xor_chan->common.cookie = desc->async_tx.cookie;

	list_splice_tail_init(&desc->tx_list,
				 &xor_chan->submit_q);

	spin_unlock_bh(&xor_chan->desc_lock);

	return cookie;
}

static struct talitos_xor_desc *talitos_xor_alloc_descriptor(
				struct talitos_xor_chan *xor_chan, gfp_t flags)
{
	struct talitos_xor_desc *desc;

	desc = kmalloc(sizeof(*desc), flags);
	if (desc) {
		xor_chan->total_desc++;
		desc->async_tx.tx_submit = talitos_async_tx_submit;
	}

	return desc;
}

static void talitos_free_chan_resources(struct dma_chan *chan)
{
	struct talitos_xor_chan *xor_chan;
	struct talitos_xor_desc *desc, *_desc;

	xor_chan = container_of(chan, struct talitos_xor_chan, common);

	spin_lock_bh(&xor_chan->desc_lock);

	list_for_each_entry_safe(desc, _desc, &xor_chan->submit_q, node) {
		list_del(&desc->node);
		xor_chan->total_desc--;
		kfree(desc);
	}
	list_for_each_entry_safe(desc, _desc, &xor_chan->pending_q, node) {
		list_del(&desc->node);
		xor_chan->total_desc--;
		kfree(desc);
	}
	list_for_each_entry_safe(desc, _desc, &xor_chan->in_progress_q, node) {
		list_del(&desc->node);
		xor_chan->total_desc--;
		kfree(desc);
	}
	list_for_each_entry_safe(desc, _desc, &xor_chan->free_desc, node) {
		list_del(&desc->node);
		xor_chan->total_desc--;
		kfree(desc);
	}
	BUG_ON(unlikely(xor_chan->total_desc));	/* Some descriptor not freed? */

	spin_unlock_bh(&xor_chan->desc_lock);
}

static int talitos_alloc_chan_resources(struct dma_chan *chan)
{
	struct talitos_xor_chan *xor_chan;
	struct talitos_xor_desc *desc;
	LIST_HEAD(tmp_list);
	int i;

	xor_chan = container_of(chan, struct talitos_xor_chan, common);

	if (!list_empty(&xor_chan->free_desc))
		return xor_chan->total_desc;

	/* 256 initial descriptors */
	for (i = 0; i < 256; i++) {
		desc = talitos_xor_alloc_descriptor(xor_chan, GFP_KERNEL);
		if (!desc) {
			dev_err(xor_chan->common.device->dev,
				"Only %d initial descriptors\n", i);
			break;
		}
		list_add_tail(&desc->node, &tmp_list);
	}

	if (!i)
		return -ENOMEM;

	/* At least one desc is allocated */
	spin_lock_bh(&xor_chan->desc_lock);
	list_splice_init(&tmp_list, &xor_chan->free_desc);
	spin_unlock_bh(&xor_chan->desc_lock);

	return xor_chan->total_desc;
}

static struct dma_async_tx_descriptor * talitos_prep_dma_xor(
			struct dma_chan *chan, dma_addr_t dest, dma_addr_t *src,
			unsigned int src_cnt, size_t len, unsigned long flags)
{
	struct talitos_xor_chan *xor_chan;
	struct talitos_xor_desc *new;
	struct talitos_desc *desc;
	int i, j;

	BUG_ON(unlikely(len > TALITOS_MAX_DATA_LEN));

	xor_chan = container_of(chan, struct talitos_xor_chan, common);

	spin_lock_bh(&xor_chan->desc_lock);
	if (!list_empty(&xor_chan->free_desc)) {
		new = container_of(xor_chan->free_desc.next,
				   struct talitos_xor_desc, node);
		list_del(&new->node);
	} else {
		new = talitos_xor_alloc_descriptor(xor_chan, GFP_KERNEL);
	}
	spin_unlock_bh(&xor_chan->desc_lock);

	if (!new) {
		dev_err(xor_chan->common.device->dev,
			"No free memory for XOR DMA descriptor\n");
		return NULL;
	}
	dma_async_tx_descriptor_init(&new->async_tx, &xor_chan->common);

	INIT_LIST_HEAD(&new->node);
	INIT_LIST_HEAD(&new->tx_list);

	desc = &new->hwdesc;
	/* Set destination: Last pointer pair */
	to_talitos_ptr(&desc->ptr[6], dest);
	desc->ptr[6].len = cpu_to_be16(len);
	desc->ptr[6].j_extent = 0;

	/* Set Sources: End loading from second-last pointer pair */
	for (i = 5, j = 0; (j < src_cnt) && (i > 0); i--, j++) {
		to_talitos_ptr(&desc->ptr[i], src[j]);
		desc->ptr[i].len = cpu_to_be16(len);
		desc->ptr[i].j_extent = 0;
	}

	/*
	 * documentation states first 0 ptr/len combo marks end of sources
	 * yet device produces scatter boundary error unless all subsequent
	 * sources are zeroed out
	 */
	for (; i >= 0; i--) {
		to_talitos_ptr(&desc->ptr[i], 0);
		desc->ptr[i].len = 0;
		desc->ptr[i].j_extent = 0;
	}

	desc->hdr = DESC_HDR_SEL0_AESU | DESC_HDR_MODE0_AESU_XOR
		    | DESC_HDR_TYPE_RAID_XOR;

	new->async_tx.parent = NULL;
	new->async_tx.next = NULL;
	new->async_tx.cookie = 0;
	async_tx_ack(&new->async_tx);

	list_add_tail(&new->node, &new->tx_list);

	new->async_tx.flags = flags;
	new->async_tx.cookie = -EBUSY;

	return &new->async_tx;
}

static void talitos_unregister_async_xor(struct device *dev)
{
	struct talitos_private *priv = dev_get_drvdata(dev);
	struct talitos_xor_chan *xor_chan;
	struct dma_chan *chan, *_chan;

	if (priv->dma_dev_common.chancnt)
		dma_async_device_unregister(&priv->dma_dev_common);

	list_for_each_entry_safe(chan, _chan, &priv->dma_dev_common.channels,
				device_node) {
		xor_chan = container_of(chan, struct talitos_xor_chan,
					common);
		list_del(&chan->device_node);
		priv->dma_dev_common.chancnt--;
		kfree(xor_chan);
	}
}

/**
 * talitos_register_dma_async - Initialize the Freescale XOR ADMA device
 * It is registered as a DMA device with the capability to perform
 * XOR operation with the Async_tx layer.
 * The various queues and channel resources are also allocated.
 */
static int talitos_register_async_tx(struct device *dev, int max_xor_srcs)
{
	struct talitos_private *priv = dev_get_drvdata(dev);
	struct dma_device *dma_dev = &priv->dma_dev_common;
	struct talitos_xor_chan *xor_chan;
	int err;

	xor_chan = kzalloc(sizeof(struct talitos_xor_chan), GFP_KERNEL);
	if (!xor_chan) {
		dev_err(dev, "unable to allocate xor channel\n");
		return -ENOMEM;
	}

	dma_dev->dev = dev;
	dma_dev->device_alloc_chan_resources = talitos_alloc_chan_resources;
	dma_dev->device_free_chan_resources = talitos_free_chan_resources;
	dma_dev->device_prep_dma_xor = talitos_prep_dma_xor;
	dma_dev->max_xor = max_xor_srcs;
	dma_dev->device_tx_status = talitos_is_tx_complete;
	dma_dev->device_issue_pending = talitos_issue_pending;
	INIT_LIST_HEAD(&dma_dev->channels);
	dma_cap_set(DMA_XOR, dma_dev->cap_mask);

	xor_chan->dev = dev;
	xor_chan->common.device = dma_dev;
	xor_chan->total_desc = 0;
	INIT_LIST_HEAD(&xor_chan->submit_q);
	INIT_LIST_HEAD(&xor_chan->pending_q);
	INIT_LIST_HEAD(&xor_chan->in_progress_q);
	INIT_LIST_HEAD(&xor_chan->free_desc);
	spin_lock_init(&xor_chan->desc_lock);

	list_add_tail(&xor_chan->common.device_node, &dma_dev->channels);
	dma_dev->chancnt++;

	err = dma_async_device_register(dma_dev);
	if (err) {
		dev_err(dev, "Unable to register XOR with Async_tx\n");
		goto err_out;
	}

	return err;

err_out:
	talitos_unregister_async_xor(dev);
	return err;
}
/*
 * crypto alg
 */
#define TALITOS_CRA_PRIORITY		3000
#define TALITOS_MAX_KEY_SIZE		64
#define TALITOS_MAX_IV_LENGTH		16 /* max of AES_BLOCK_SIZE, DES3_EDE_BLOCK_SIZE */

#define MD5_BLOCK_SIZE    64

struct talitos_ctx {
	struct device *dev;
	__be32 desc_hdr_template;
	u8 key[TALITOS_MAX_KEY_SIZE];
	u8 iv[TALITOS_MAX_IV_LENGTH];
	unsigned int keylen;
	unsigned int enckeylen;
	unsigned int authkeylen;
	unsigned int authsize;
};

#define HASH_MAX_BLOCK_SIZE		SHA512_BLOCK_SIZE
#define TALITOS_MDEU_MAX_CONTEXT_SIZE	TALITOS_MDEU_CONTEXT_SIZE_SHA384_SHA512

struct talitos_ahash_req_ctx {
	u64 count;
	u32 hw_context[TALITOS_MDEU_MAX_CONTEXT_SIZE / sizeof(u32)];
	unsigned int hw_context_size;
	u8 buf[HASH_MAX_BLOCK_SIZE];
	u8 bufnext[HASH_MAX_BLOCK_SIZE];
	unsigned int swinit;
	unsigned int first;
	unsigned int last;
	unsigned int to_hash_later;
	struct scatterlist bufsl[2];
	struct scatterlist *psrc;
};

static int aead_setauthsize(struct crypto_aead *authenc,
			    unsigned int authsize)
{
	struct talitos_ctx *ctx = crypto_aead_ctx(authenc);

	ctx->authsize = authsize;

	return 0;
}

static int aead_setkey(struct crypto_aead *authenc,
		       const u8 *key, unsigned int keylen)
{
	struct talitos_ctx *ctx = crypto_aead_ctx(authenc);
	struct rtattr *rta = (void *)key;
	struct crypto_authenc_key_param *param;
	unsigned int authkeylen;
	unsigned int enckeylen;

	if (!RTA_OK(rta, keylen))
		goto badkey;

	if (rta->rta_type != CRYPTO_AUTHENC_KEYA_PARAM)
		goto badkey;

	if (RTA_PAYLOAD(rta) < sizeof(*param))
		goto badkey;

	param = RTA_DATA(rta);
	enckeylen = be32_to_cpu(param->enckeylen);

	key += RTA_ALIGN(rta->rta_len);
	keylen -= RTA_ALIGN(rta->rta_len);

	if (keylen < enckeylen)
		goto badkey;

	authkeylen = keylen - enckeylen;

	if (keylen > TALITOS_MAX_KEY_SIZE)
		goto badkey;

	memcpy(&ctx->key, key, keylen);

	ctx->keylen = keylen;
	ctx->enckeylen = enckeylen;
	ctx->authkeylen = authkeylen;

	return 0;

badkey:
	crypto_aead_set_flags(authenc, CRYPTO_TFM_RES_BAD_KEY_LEN);
	return -EINVAL;
}

static int talitos_map_sg(struct device *dev, struct scatterlist *sg,
			  unsigned int nents, enum dma_data_direction dir,
			  int chained)
{
	if (unlikely(chained))
		while (sg) {
			dma_map_sg(dev, sg, 1, dir);
			sg = scatterwalk_sg_next(sg);
		}
	else
		dma_map_sg(dev, sg, nents, dir);
	return nents;
}

static void talitos_unmap_sg_chain(struct device *dev, struct scatterlist *sg,
				   enum dma_data_direction dir)
{
	while (sg) {
		dma_unmap_sg(dev, sg, 1, dir);
		sg = scatterwalk_sg_next(sg);
	}
}

static void talitos_sg_unmap(struct device *dev,
			     struct talitos_edesc *edesc,
			     struct scatterlist *src,
			     struct scatterlist *dst)
{
	unsigned int src_nents = edesc->src_nents ? : 1;
	unsigned int dst_nents = edesc->dst_nents ? : 1;

	if (src != dst) {
		if (edesc->src_is_chained)
			talitos_unmap_sg_chain(dev, src, DMA_TO_DEVICE);
		else
			dma_unmap_sg(dev, src, src_nents, DMA_TO_DEVICE);

		if (dst) {
			if (edesc->dst_is_chained)
				talitos_unmap_sg_chain(dev, dst,
						       DMA_FROM_DEVICE);
			else
				dma_unmap_sg(dev, dst, dst_nents,
					     DMA_FROM_DEVICE);
		}
	} else
		if (edesc->src_is_chained)
			talitos_unmap_sg_chain(dev, src, DMA_BIDIRECTIONAL);
		else
			dma_unmap_sg(dev, src, src_nents, DMA_BIDIRECTIONAL);
}

static void ipsec_esp_unmap(struct device *dev,
			    struct talitos_edesc *edesc,
			    struct aead_request *areq)
{
	unmap_single_talitos_ptr(dev, &edesc->desc.ptr[6], DMA_FROM_DEVICE);
	unmap_single_talitos_ptr(dev, &edesc->desc.ptr[3], DMA_TO_DEVICE);
	unmap_single_talitos_ptr(dev, &edesc->desc.ptr[2], DMA_TO_DEVICE);
	unmap_single_talitos_ptr(dev, &edesc->desc.ptr[0], DMA_TO_DEVICE);

	dma_unmap_sg(dev, areq->assoc, 1, DMA_TO_DEVICE);

	talitos_sg_unmap(dev, edesc, areq->src, areq->dst);

	if (edesc->dma_len)
		dma_unmap_single(dev, edesc->dma_link_tbl, edesc->dma_len,
				 DMA_BIDIRECTIONAL);
}

/*
 * ipsec_esp descriptor callbacks
 */
static void ipsec_esp_encrypt_done(struct device *dev,
				   struct talitos_desc *desc, void *context,
				   int err)
{
	struct aead_request *areq = context;
	struct crypto_aead *authenc = crypto_aead_reqtfm(areq);
	struct talitos_ctx *ctx = crypto_aead_ctx(authenc);
	struct talitos_edesc *edesc;
	struct scatterlist *sg;
	void *icvdata;
	struct talitos_private *priv = dev_get_drvdata(dev);

	edesc = container_of(desc, struct talitos_edesc, desc);

	ipsec_esp_unmap(dev, edesc, areq);

	/* copy the generated ICV to dst */
	if (edesc->dma_len) {
		icvdata = &edesc->link_tbl[edesc->src_nents +
					   edesc->dst_nents + 2];
		sg = sg_last(areq->dst, edesc->dst_nents);
		memcpy((char *)sg_virt(sg) + sg->length - ctx->authsize,
		       icvdata, ctx->authsize);
	}

	crypto_edesc_free(edesc, priv);

	aead_request_complete(areq, err);
}

static void ipsec_esp_decrypt_swauth_done(struct device *dev,
					  struct talitos_desc *desc,
					  void *context, int err)
{
	struct aead_request *req = context;
	struct crypto_aead *authenc = crypto_aead_reqtfm(req);
	struct talitos_ctx *ctx = crypto_aead_ctx(authenc);
	struct talitos_edesc *edesc;
	struct scatterlist *sg;
	void *icvdata;
	struct talitos_private *priv = dev_get_drvdata(dev);

	edesc = container_of(desc, struct talitos_edesc, desc);

	ipsec_esp_unmap(dev, edesc, req);

	if (!err) {
		/* auth check */
		if (edesc->dma_len)
			icvdata = &edesc->link_tbl[edesc->src_nents +
						   edesc->dst_nents + 2];
		else
			icvdata = &edesc->link_tbl[0];

		sg = sg_last(req->dst, edesc->dst_nents ? : 1);
		err = memcmp(icvdata, (char *)sg_virt(sg) + sg->length -
			     ctx->authsize, ctx->authsize) ? -EBADMSG : 0;
	}

	crypto_edesc_free(edesc, priv);

	aead_request_complete(req, err);
}

static void ipsec_esp_decrypt_hwauth_done(struct device *dev,
					  struct talitos_desc *desc,
					  void *context, int err)
{
	struct aead_request *req = context;
	struct talitos_edesc *edesc;
	struct talitos_private *priv = dev_get_drvdata(dev);

	edesc = container_of(desc, struct talitos_edesc, desc);

	ipsec_esp_unmap(dev, edesc, req);

	/* check ICV auth status */
	if (!err && ((desc->hdr_lo & DESC_HDR_LO_ICCR1_MASK) !=
		     DESC_HDR_LO_ICCR1_PASS))
		err = -EBADMSG;

	crypto_edesc_free(edesc, priv);

	aead_request_complete(req, err);
}

/*
 * convert scatterlist to SEC h/w link table format
 * stop at cryptlen bytes
 */
static int sg_to_link_tbl(struct scatterlist *sg, int sg_count,
			   int cryptlen, struct talitos_ptr *link_tbl_ptr)
{
	int n_sg = sg_count;

	while (n_sg--) {
		to_talitos_ptr(link_tbl_ptr, sg_dma_address(sg));
		link_tbl_ptr->len = cpu_to_be16(sg_dma_len(sg));
		link_tbl_ptr->j_extent = 0;
		link_tbl_ptr++;
		cryptlen -= sg_dma_len(sg);
		sg = scatterwalk_sg_next(sg);
	}

	/* adjust (decrease) last one (or two) entry's len to cryptlen */
	link_tbl_ptr--;
	while (be16_to_cpu(link_tbl_ptr->len) <= (-cryptlen)) {
		/* Empty this entry, and move to previous one */
		cryptlen += be16_to_cpu(link_tbl_ptr->len);
		link_tbl_ptr->len = 0;
		sg_count--;
		link_tbl_ptr--;
	}
	link_tbl_ptr->len = cpu_to_be16(be16_to_cpu(link_tbl_ptr->len)
					+ cryptlen);

	/* tag end of link table */
	link_tbl_ptr->j_extent = DESC_PTR_LNKTBL_RETURN;

	return sg_count;
}

/*
 * fill in and submit ipsec_esp descriptor
 */
static int ipsec_esp(struct talitos_edesc *edesc, struct aead_request *areq,
		     u8 *giv, u64 seq,
		     void (*callback) (struct device *dev,
				       struct talitos_desc *desc,
				       void *context, int error))
{
	struct crypto_aead *aead = crypto_aead_reqtfm(areq);
	struct talitos_ctx *ctx = crypto_aead_ctx(aead);
	struct device *dev = ctx->dev;
	struct talitos_desc *desc = &edesc->desc;
	unsigned int cryptlen = areq->cryptlen;
	unsigned int authsize = ctx->authsize;
	unsigned int ivsize = crypto_aead_ivsize(aead);
	int sg_count, ret;
	int sg_link_tbl_len;
	struct talitos_private *priv = dev_get_drvdata(dev);

	/* hmac key */
	map_single_talitos_ptr(dev, &desc->ptr[0], ctx->authkeylen, &ctx->key,
			       0, DMA_TO_DEVICE);
	/* hmac data */
	map_single_talitos_ptr(dev, &desc->ptr[1], areq->assoclen + ivsize,
			       sg_virt(areq->assoc), 0, DMA_TO_DEVICE);
	/* cipher iv */
	map_single_talitos_ptr(dev, &desc->ptr[2], ivsize, giv ?: areq->iv, 0,
			       DMA_TO_DEVICE);

	/* cipher key */
	map_single_talitos_ptr(dev, &desc->ptr[3], ctx->enckeylen,
			       (char *)&ctx->key + ctx->authkeylen, 0,
			       DMA_TO_DEVICE);

	/*
	 * cipher in
	 * map and adjust cipher len to aead request cryptlen.
	 * extent is bytes of HMAC postpended to ciphertext,
	 * typically 12 for ipsec
	 */
	desc->ptr[4].len = cpu_to_be16(cryptlen);
	desc->ptr[4].j_extent = authsize;

	sg_count = talitos_map_sg(dev, areq->src, edesc->src_nents ? : 1,
				  (areq->src == areq->dst) ? DMA_BIDIRECTIONAL
							   : DMA_TO_DEVICE,
				  edesc->src_is_chained);

	if (sg_count == 1) {
		to_talitos_ptr(&desc->ptr[4], sg_dma_address(areq->src));
	} else {
		sg_link_tbl_len = cryptlen;

		if (edesc->desc.hdr & DESC_HDR_MODE1_MDEU_CICV)
			sg_link_tbl_len = cryptlen + authsize;

		sg_count = sg_to_link_tbl(areq->src, sg_count, sg_link_tbl_len,
					  &edesc->link_tbl[0]);
		if (sg_count > 1) {
			desc->ptr[4].j_extent |= DESC_PTR_LNKTBL_JUMP;
			to_talitos_ptr(&desc->ptr[4], edesc->dma_link_tbl);
			dma_sync_single_for_device(dev, edesc->dma_link_tbl,
						   edesc->dma_len,
						   DMA_BIDIRECTIONAL);
		} else {
			/* Only one segment now, so no link tbl needed */
			to_talitos_ptr(&desc->ptr[4],
				       sg_dma_address(areq->src));
		}
	}

	/* cipher out */
	desc->ptr[5].len = cpu_to_be16(cryptlen);
	desc->ptr[5].j_extent = authsize;

	if (areq->src != areq->dst)
		sg_count = talitos_map_sg(dev, areq->dst,
					  edesc->dst_nents ? : 1,
					  DMA_FROM_DEVICE,
					  edesc->dst_is_chained);

	if (sg_count == 1) {
		to_talitos_ptr(&desc->ptr[5], sg_dma_address(areq->dst));
	} else {
		struct talitos_ptr *link_tbl_ptr =
			&edesc->link_tbl[edesc->src_nents + 1];

		to_talitos_ptr(&desc->ptr[5], edesc->dma_link_tbl +
			       (edesc->src_nents + 1) *
			       sizeof(struct talitos_ptr));
		sg_count = sg_to_link_tbl(areq->dst, sg_count, cryptlen,
					  link_tbl_ptr);

		/* Add an entry to the link table for ICV data */
		link_tbl_ptr += sg_count - 1;
		link_tbl_ptr->j_extent = 0;
		sg_count++;
		link_tbl_ptr++;
		link_tbl_ptr->j_extent = DESC_PTR_LNKTBL_RETURN;
		link_tbl_ptr->len = cpu_to_be16(authsize);

		/* icv data follows link tables */
		to_talitos_ptr(link_tbl_ptr, edesc->dma_link_tbl +
			       (edesc->src_nents + edesc->dst_nents + 2) *
			       sizeof(struct talitos_ptr));
		desc->ptr[5].j_extent |= DESC_PTR_LNKTBL_JUMP;
		dma_sync_single_for_device(ctx->dev, edesc->dma_link_tbl,
					   edesc->dma_len, DMA_BIDIRECTIONAL);
	}

	/* iv out */
	map_single_talitos_ptr(dev, &desc->ptr[6], ivsize, ctx->iv, 0,
			       DMA_FROM_DEVICE);

	ret = talitos_submit(dev, desc, callback, areq);
	if (ret != -EINPROGRESS) {
		ipsec_esp_unmap(dev, edesc, areq);
		crypto_edesc_free(edesc, priv);
	}
	return ret;
}

/*
 * derive number of elements in scatterlist
 */
static int sg_count(struct scatterlist *sg_list, int nbytes, int *chained)
{
	struct scatterlist *sg = sg_list;
	int sg_nents = 0;

	*chained = 0;
	while (nbytes > 0) {
		sg_nents++;
		nbytes -= sg->length;
		if (!sg_is_last(sg) && (sg + 1)->length == 0)
			*chained = 1;
		sg = scatterwalk_sg_next(sg);
	}

	return sg_nents;
}

/**
 * sg_copy_end_to_buffer - Copy end data from SG list to a linear buffer
 * @sgl:		 The SG list
 * @nents:		 Number of SG entries
 * @buf:		 Where to copy to
 * @buflen:		 The number of bytes to copy
 * @skip:		 The number of bytes to skip before copying.
 *                       Note: skip + buflen should equal SG total size.
 *
 * Returns the number of copied bytes.
 *
 **/
static size_t sg_copy_end_to_buffer(struct scatterlist *sgl, unsigned int nents,
				    void *buf, size_t buflen, unsigned int skip)
{
	unsigned int offset = 0;
	unsigned int boffset = 0;
	struct sg_mapping_iter miter;
	unsigned long flags;
	unsigned int sg_flags = SG_MITER_ATOMIC;
	size_t total_buffer = buflen + skip;

	sg_flags |= SG_MITER_FROM_SG;

	sg_miter_start(&miter, sgl, nents, sg_flags);

	local_irq_save(flags);

	while (sg_miter_next(&miter) && offset < total_buffer) {
		unsigned int len;
		unsigned int ignore;

		if ((offset + miter.length) > skip) {
			if (offset < skip) {
				/* Copy part of this segment */
				ignore = skip - offset;
				len = miter.length - ignore;
				if (boffset + len > buflen)
					len = buflen - boffset;
				memcpy(buf + boffset, miter.addr + ignore, len);
			} else {
				/* Copy all of this segment (up to buflen) */
				len = miter.length;
				if (boffset + len > buflen)
					len = buflen - boffset;
				memcpy(buf + boffset, miter.addr, len);
			}
			boffset += len;
		}
		offset += miter.length;
	}

	sg_miter_stop(&miter);

	local_irq_restore(flags);
	return boffset;
}

/*
 * allocate and map the extended descriptor
 */
static struct talitos_edesc *talitos_edesc_alloc(struct device *dev,
						 struct scatterlist *src,
						 struct scatterlist *dst,
						 int hash_result,
						 unsigned int cryptlen,
						 unsigned int authsize,
						 int icv_stashing,
						 u32 cryptoflags)
{
	struct talitos_edesc *edesc;
	int src_nents, dst_nents, alloc_len, dma_len;
	int src_chained, dst_chained = 0;
	struct talitos_private *priv = dev_get_drvdata(dev);
	gfp_t flags = cryptoflags & CRYPTO_TFM_REQ_MAY_SLEEP ? GFP_KERNEL :
		      GFP_ATOMIC;

	if (cryptlen + authsize > TALITOS_MAX_DATA_LEN) {
		dev_err(dev, "length exceeds h/w max limit\n");
		return ERR_PTR(-EINVAL);
	}

	src_nents = sg_count(src, cryptlen + authsize, &src_chained);
	src_nents = (src_nents == 1) ? 0 : src_nents;

	if (hash_result) {
		dst_nents = 0;
	} else {
		if (dst == src) {
			dst_nents = src_nents;
		} else {
			dst_nents = sg_count(dst, cryptlen + authsize,
					     &dst_chained);
			dst_nents = (dst_nents == 1) ? 0 : dst_nents;
		}
	}

	/*
	 * allocate space for base edesc plus the link tables,
	 * allowing for two separate entries for ICV and generated ICV (+ 2),
	 * and the ICV data itself
	 */
	alloc_len = sizeof(struct talitos_edesc);
	if (src_nents || dst_nents) {
		dma_len = (src_nents + dst_nents + 2) *
				 sizeof(struct talitos_ptr) + authsize;
		alloc_len += dma_len;
	} else {
		dma_len = 0;
		alloc_len += icv_stashing ? authsize : 0;
	}

	edesc = crypto_edesc_alloc(alloc_len, GFP_DMA | flags, priv);
	if (!edesc) {
		dev_err(dev, "could not allocate edescriptor\n");
		return ERR_PTR(-ENOMEM);
	}

	edesc->src_nents = src_nents;
	edesc->dst_nents = dst_nents;
	edesc->src_is_chained = src_chained;
	edesc->dst_is_chained = dst_chained;
	edesc->dma_len = dma_len;
	if (dma_len)
		edesc->dma_link_tbl = dma_map_single(dev, &edesc->link_tbl[0],
						     edesc->dma_len,
						     DMA_BIDIRECTIONAL);

	return edesc;
}

static struct talitos_edesc *aead_edesc_alloc(struct aead_request *areq,
					      int icv_stashing)
{
	struct crypto_aead *authenc = crypto_aead_reqtfm(areq);
	struct talitos_ctx *ctx = crypto_aead_ctx(authenc);

	return talitos_edesc_alloc(ctx->dev, areq->src, areq->dst, 0,
				   areq->cryptlen, ctx->authsize, icv_stashing,
				   areq->base.flags);
}

static int aead_encrypt(struct aead_request *req)
{
	struct crypto_aead *authenc = crypto_aead_reqtfm(req);
	struct talitos_ctx *ctx = crypto_aead_ctx(authenc);
	struct talitos_edesc *edesc;

	/* allocate extended descriptor */
	edesc = aead_edesc_alloc(req, 0);
	if (IS_ERR(edesc))
		return PTR_ERR(edesc);

	/* set encrypt */
	edesc->desc.hdr = ctx->desc_hdr_template | DESC_HDR_MODE0_ENCRYPT;

	return ipsec_esp(edesc, req, NULL, 0, ipsec_esp_encrypt_done);
}

static int aead_decrypt(struct aead_request *req)
{
	struct crypto_aead *authenc = crypto_aead_reqtfm(req);
	struct talitos_ctx *ctx = crypto_aead_ctx(authenc);
	unsigned int authsize = ctx->authsize;
	struct talitos_private *priv = dev_get_drvdata(ctx->dev);
	struct talitos_edesc *edesc;
	struct scatterlist *sg;
	void *icvdata;

	req->cryptlen -= authsize;

	/* allocate extended descriptor */
	edesc = aead_edesc_alloc(req, 1);
	if (IS_ERR(edesc))
		return PTR_ERR(edesc);

	if ((priv->features & TALITOS_FTR_HW_AUTH_CHECK) &&
	    ((!edesc->src_nents && !edesc->dst_nents) ||
	     priv->features & TALITOS_FTR_SRC_LINK_TBL_LEN_INCLUDES_EXTENT)) {

		/* decrypt and check the ICV */
		edesc->desc.hdr = ctx->desc_hdr_template |
				  DESC_HDR_DIR_INBOUND |
				  DESC_HDR_MODE1_MDEU_CICV;

		/* reset integrity check result bits */
		edesc->desc.hdr_lo = 0;

		return ipsec_esp(edesc, req, NULL, 0,
				 ipsec_esp_decrypt_hwauth_done);

	}

	/* Have to check the ICV with software */
	edesc->desc.hdr = ctx->desc_hdr_template | DESC_HDR_DIR_INBOUND;

	/* stash incoming ICV for later cmp with ICV generated by the h/w */
	if (edesc->dma_len)
		icvdata = &edesc->link_tbl[edesc->src_nents +
					   edesc->dst_nents + 2];
	else
		icvdata = &edesc->link_tbl[0];

	sg = sg_last(req->src, edesc->src_nents ? : 1);

	memcpy(icvdata, (char *)sg_virt(sg) + sg->length - ctx->authsize,
	       ctx->authsize);

	return ipsec_esp(edesc, req, NULL, 0, ipsec_esp_decrypt_swauth_done);
}

static int aead_givencrypt(struct aead_givcrypt_request *req)
{
	struct aead_request *areq = &req->areq;
	struct crypto_aead *authenc = crypto_aead_reqtfm(areq);
	struct talitos_ctx *ctx = crypto_aead_ctx(authenc);
	struct talitos_edesc *edesc;

	/* allocate extended descriptor */
	edesc = aead_edesc_alloc(areq, 0);
	if (IS_ERR(edesc))
		return PTR_ERR(edesc);

	/* set encrypt */
	edesc->desc.hdr = ctx->desc_hdr_template | DESC_HDR_MODE0_ENCRYPT;

	memcpy(req->giv, ctx->iv, crypto_aead_ivsize(authenc));
	/* avoid consecutive packets going out with same IV */
	*(__be64 *)req->giv ^= cpu_to_be64(req->seq);

	return ipsec_esp(edesc, areq, req->giv, req->seq,
			 ipsec_esp_encrypt_done);
}

static int ablkcipher_setkey(struct crypto_ablkcipher *cipher,
			     const u8 *key, unsigned int keylen)
{
	struct talitos_ctx *ctx = crypto_ablkcipher_ctx(cipher);
	struct ablkcipher_alg *alg = crypto_ablkcipher_alg(cipher);

	if (keylen > TALITOS_MAX_KEY_SIZE)
		goto badkey;

	if (keylen < alg->min_keysize || keylen > alg->max_keysize)
		goto badkey;

	memcpy(&ctx->key, key, keylen);
	ctx->keylen = keylen;

	return 0;

badkey:
	crypto_ablkcipher_set_flags(cipher, CRYPTO_TFM_RES_BAD_KEY_LEN);
	return -EINVAL;
}

static void common_nonsnoop_unmap(struct device *dev,
				  struct talitos_edesc *edesc,
				  struct ablkcipher_request *areq)
{
	unmap_single_talitos_ptr(dev, &edesc->desc.ptr[5], DMA_FROM_DEVICE);
	unmap_single_talitos_ptr(dev, &edesc->desc.ptr[2], DMA_TO_DEVICE);
	unmap_single_talitos_ptr(dev, &edesc->desc.ptr[1], DMA_TO_DEVICE);

	talitos_sg_unmap(dev, edesc, areq->src, areq->dst);

	if (edesc->dma_len)
		dma_unmap_single(dev, edesc->dma_link_tbl, edesc->dma_len,
				 DMA_BIDIRECTIONAL);
}

static void ablkcipher_done(struct device *dev,
			    struct talitos_desc *desc, void *context,
			    int err)
{
	struct ablkcipher_request *areq = context;
	struct talitos_edesc *edesc;
	struct talitos_private *priv = dev_get_drvdata(dev);

	edesc = container_of(desc, struct talitos_edesc, desc);

	common_nonsnoop_unmap(dev, edesc, areq);

	crypto_edesc_free(edesc, priv);

	areq->base.complete(&areq->base, err);
}

static int common_nonsnoop(struct talitos_edesc *edesc,
			   struct ablkcipher_request *areq,
			   u8 *giv,
			   void (*callback) (struct device *dev,
					     struct talitos_desc *desc,
					     void *context, int error))
{
	struct crypto_ablkcipher *cipher = crypto_ablkcipher_reqtfm(areq);
	struct talitos_ctx *ctx = crypto_ablkcipher_ctx(cipher);
	struct device *dev = ctx->dev;
	struct talitos_desc *desc = &edesc->desc;
	unsigned int cryptlen = areq->nbytes;
	unsigned int ivsize;
	int sg_count, ret;
	struct talitos_private *priv = dev_get_drvdata(dev);

	/* first DWORD empty */
	desc->ptr[0].len = 0;
	to_talitos_ptr(&desc->ptr[0], 0);
	desc->ptr[0].j_extent = 0;

	/* cipher iv */
	ivsize = crypto_ablkcipher_ivsize(cipher);
	map_single_talitos_ptr(dev, &desc->ptr[1], ivsize, giv ?: areq->info, 0,
			       DMA_TO_DEVICE);

	/* cipher key */
	map_single_talitos_ptr(dev, &desc->ptr[2], ctx->keylen,
			       (char *)&ctx->key, 0, DMA_TO_DEVICE);

	/*
	 * cipher in
	 */
	desc->ptr[3].len = cpu_to_be16(cryptlen);
	desc->ptr[3].j_extent = 0;

	sg_count = talitos_map_sg(dev, areq->src, edesc->src_nents ? : 1,
				  (areq->src == areq->dst) ? DMA_BIDIRECTIONAL
							   : DMA_TO_DEVICE,
				  edesc->src_is_chained);

	if (sg_count == 1) {
		to_talitos_ptr(&desc->ptr[3], sg_dma_address(areq->src));
	} else {
		sg_count = sg_to_link_tbl(areq->src, sg_count, cryptlen,
					  &edesc->link_tbl[0]);
		if (sg_count > 1) {
			to_talitos_ptr(&desc->ptr[3], edesc->dma_link_tbl);
			desc->ptr[3].j_extent |= DESC_PTR_LNKTBL_JUMP;
			dma_sync_single_for_device(dev, edesc->dma_link_tbl,
						   edesc->dma_len,
						   DMA_BIDIRECTIONAL);
		} else {
			/* Only one segment now, so no link tbl needed */
			to_talitos_ptr(&desc->ptr[3],
				       sg_dma_address(areq->src));
		}
	}

	/* cipher out */
	desc->ptr[4].len = cpu_to_be16(cryptlen);
	desc->ptr[4].j_extent = 0;

	if (areq->src != areq->dst)
		sg_count = talitos_map_sg(dev, areq->dst,
					  edesc->dst_nents ? : 1,
					  DMA_FROM_DEVICE,
					  edesc->dst_is_chained);

	if (sg_count == 1) {
		to_talitos_ptr(&desc->ptr[4], sg_dma_address(areq->dst));
	} else {
		struct talitos_ptr *link_tbl_ptr =
			&edesc->link_tbl[edesc->src_nents + 1];

		to_talitos_ptr(&desc->ptr[4], edesc->dma_link_tbl +
					      (edesc->src_nents + 1) *
					      sizeof(struct talitos_ptr));
		desc->ptr[4].j_extent |= DESC_PTR_LNKTBL_JUMP;
		sg_count = sg_to_link_tbl(areq->dst, sg_count, cryptlen,
					  link_tbl_ptr);
		dma_sync_single_for_device(ctx->dev, edesc->dma_link_tbl,
					   edesc->dma_len, DMA_BIDIRECTIONAL);
	}

	/* iv out */
	map_single_talitos_ptr(dev, &desc->ptr[5], ivsize, ctx->iv, 0,
			       DMA_FROM_DEVICE);

	/* last DWORD empty */
	desc->ptr[6].len = 0;
	to_talitos_ptr(&desc->ptr[6], 0);
	desc->ptr[6].j_extent = 0;

	ret = talitos_submit(dev, desc, callback, areq);
	if (ret != -EINPROGRESS) {
		common_nonsnoop_unmap(dev, edesc, areq);
		crypto_edesc_free(edesc, priv);
	}
	return ret;
}

static struct talitos_edesc *ablkcipher_edesc_alloc(struct ablkcipher_request *
						    areq)
{
	struct crypto_ablkcipher *cipher = crypto_ablkcipher_reqtfm(areq);
	struct talitos_ctx *ctx = crypto_ablkcipher_ctx(cipher);

	return talitos_edesc_alloc(ctx->dev, areq->src, areq->dst, 0,
				   areq->nbytes, 0, 0, areq->base.flags);
}

static int ablkcipher_encrypt(struct ablkcipher_request *areq)
{
	struct crypto_ablkcipher *cipher = crypto_ablkcipher_reqtfm(areq);
	struct talitos_ctx *ctx = crypto_ablkcipher_ctx(cipher);
	struct talitos_edesc *edesc;

	/* allocate extended descriptor */
	edesc = ablkcipher_edesc_alloc(areq);
	if (IS_ERR(edesc))
		return PTR_ERR(edesc);

	/* set encrypt */
	edesc->desc.hdr = ctx->desc_hdr_template | DESC_HDR_MODE0_ENCRYPT;

	return common_nonsnoop(edesc, areq, NULL, ablkcipher_done);
}

static int ablkcipher_decrypt(struct ablkcipher_request *areq)
{
	struct crypto_ablkcipher *cipher = crypto_ablkcipher_reqtfm(areq);
	struct talitos_ctx *ctx = crypto_ablkcipher_ctx(cipher);
	struct talitos_edesc *edesc;

	/* allocate extended descriptor */
	edesc = ablkcipher_edesc_alloc(areq);
	if (IS_ERR(edesc))
		return PTR_ERR(edesc);

	edesc->desc.hdr = ctx->desc_hdr_template | DESC_HDR_DIR_INBOUND;

	return common_nonsnoop(edesc, areq, NULL, ablkcipher_done);
}

static void common_nonsnoop_hash_unmap(struct device *dev,
				       struct talitos_edesc *edesc,
				       struct ahash_request *areq)
{
	struct talitos_ahash_req_ctx *req_ctx = ahash_request_ctx(areq);

	unmap_single_talitos_ptr(dev, &edesc->desc.ptr[5], DMA_FROM_DEVICE);

	/* When using hashctx-in, must unmap it. */
	if (edesc->desc.ptr[1].len)
		unmap_single_talitos_ptr(dev, &edesc->desc.ptr[1],
					 DMA_TO_DEVICE);

	if (edesc->desc.ptr[2].len)
		unmap_single_talitos_ptr(dev, &edesc->desc.ptr[2],
					 DMA_TO_DEVICE);

	talitos_sg_unmap(dev, edesc, req_ctx->psrc, NULL);

	if (edesc->dma_len)
		dma_unmap_single(dev, edesc->dma_link_tbl, edesc->dma_len,
				 DMA_BIDIRECTIONAL);

}

static void ahash_done(struct device *dev,
		       struct talitos_desc *desc, void *context,
		       int err)
{
	struct ahash_request *areq = context;
	struct talitos_edesc *edesc =
		 container_of(desc, struct talitos_edesc, desc);
	struct talitos_ahash_req_ctx *req_ctx = ahash_request_ctx(areq);

	if (!req_ctx->last && req_ctx->to_hash_later) {
		/* Position any partial block for next update/final/finup */
		memcpy(req_ctx->buf, req_ctx->bufnext, req_ctx->to_hash_later);
	}
	common_nonsnoop_hash_unmap(dev, edesc, areq);

	kfree(edesc);

	areq->base.complete(&areq->base, err);
}

static int common_nonsnoop_hash(struct talitos_edesc *edesc,
				struct ahash_request *areq, unsigned int length,
				void (*callback) (struct device *dev,
						  struct talitos_desc *desc,
						  void *context, int error))
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(areq);
	struct talitos_ctx *ctx = crypto_ahash_ctx(tfm);
	struct talitos_ahash_req_ctx *req_ctx = ahash_request_ctx(areq);
	struct device *dev = ctx->dev;
	struct talitos_desc *desc = &edesc->desc;
	int sg_count, ret;

	/* first DWORD empty */
	desc->ptr[0] = zero_entry;

	/* hash context in */
	if (!req_ctx->first || req_ctx->swinit) {
		map_single_talitos_ptr(dev, &desc->ptr[1],
				       req_ctx->hw_context_size,
				       (char *)req_ctx->hw_context, 0,
				       DMA_TO_DEVICE);
		req_ctx->swinit = 0;
	} else {
		desc->ptr[1] = zero_entry;
		/* Indicate next op is not the first. */
		req_ctx->first = 0;
	}

	/* HMAC key */
	if (ctx->keylen)
		map_single_talitos_ptr(dev, &desc->ptr[2], ctx->keylen,
				       (char *)&ctx->key, 0, DMA_TO_DEVICE);
	else
		desc->ptr[2] = zero_entry;

	/*
	 * data in
	 */
	desc->ptr[3].len = cpu_to_be16(length);
	desc->ptr[3].j_extent = 0;

	sg_count = talitos_map_sg(dev, req_ctx->psrc,
				  edesc->src_nents ? : 1,
				  DMA_TO_DEVICE,
				  edesc->src_is_chained);

	if (sg_count == 1) {
		to_talitos_ptr(&desc->ptr[3], sg_dma_address(req_ctx->psrc));
	} else {
		sg_count = sg_to_link_tbl(req_ctx->psrc, sg_count, length,
					  &edesc->link_tbl[0]);
		if (sg_count > 1) {
			desc->ptr[3].j_extent |= DESC_PTR_LNKTBL_JUMP;
			to_talitos_ptr(&desc->ptr[3], edesc->dma_link_tbl);
			dma_sync_single_for_device(ctx->dev,
						   edesc->dma_link_tbl,
						   edesc->dma_len,
						   DMA_BIDIRECTIONAL);
		} else {
			/* Only one segment now, so no link tbl needed */
			to_talitos_ptr(&desc->ptr[3],
				       sg_dma_address(req_ctx->psrc));
		}
	}

	/* fifth DWORD empty */
	desc->ptr[4] = zero_entry;

	/* hash/HMAC out -or- hash context out */
	if (req_ctx->last)
		map_single_talitos_ptr(dev, &desc->ptr[5],
				       crypto_ahash_digestsize(tfm),
				       areq->result, 0, DMA_FROM_DEVICE);
	else
		map_single_talitos_ptr(dev, &desc->ptr[5],
				       req_ctx->hw_context_size,
				       req_ctx->hw_context, 0, DMA_FROM_DEVICE);

	/* last DWORD empty */
	desc->ptr[6] = zero_entry;

	ret = talitos_submit(dev, desc, callback, areq);
	if (ret != -EINPROGRESS) {
		common_nonsnoop_hash_unmap(dev, edesc, areq);
		kfree(edesc);
	}
	return ret;
}

static struct talitos_edesc *ahash_edesc_alloc(struct ahash_request *areq,
					       unsigned int nbytes)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(areq);
	struct talitos_ctx *ctx = crypto_ahash_ctx(tfm);
	struct talitos_ahash_req_ctx *req_ctx = ahash_request_ctx(areq);

	return talitos_edesc_alloc(ctx->dev, req_ctx->psrc, NULL, 1,
				   nbytes, 0, 0, areq->base.flags);
}

static int ahash_init(struct ahash_request *areq)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(areq);
	struct talitos_ahash_req_ctx *req_ctx = ahash_request_ctx(areq);

	/* Initialize the context */
	req_ctx->count = 0;
	req_ctx->first = 1; /* first indicates h/w must init its context */
	req_ctx->swinit = 0; /* assume h/w init of context */
	req_ctx->hw_context_size =
		(crypto_ahash_digestsize(tfm) <= SHA256_DIGEST_SIZE)
			? TALITOS_MDEU_CONTEXT_SIZE_MD5_SHA1_SHA256
			: TALITOS_MDEU_CONTEXT_SIZE_SHA384_SHA512;

	return 0;
}

/*
 * on h/w without explicit sha224 support, we initialize h/w context
 * manually with sha224 constants, and tell it to run sha256.
 */
static int ahash_init_sha224_swinit(struct ahash_request *areq)
{
	struct talitos_ahash_req_ctx *req_ctx = ahash_request_ctx(areq);

	ahash_init(areq);
	req_ctx->swinit = 1;/* prevent h/w initting context with sha256 values*/

	req_ctx->hw_context[0] = cpu_to_be32(SHA224_H0);
	req_ctx->hw_context[1] = cpu_to_be32(SHA224_H1);
	req_ctx->hw_context[2] = cpu_to_be32(SHA224_H2);
	req_ctx->hw_context[3] = cpu_to_be32(SHA224_H3);
	req_ctx->hw_context[4] = cpu_to_be32(SHA224_H4);
	req_ctx->hw_context[5] = cpu_to_be32(SHA224_H5);
	req_ctx->hw_context[6] = cpu_to_be32(SHA224_H6);
	req_ctx->hw_context[7] = cpu_to_be32(SHA224_H7);

	/* init 64-bit count */
	req_ctx->hw_context[8] = 0;
	req_ctx->hw_context[9] = 0;

	return 0;
}

static int ahash_process_req(struct ahash_request *areq, unsigned int nbytes)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(areq);
	struct talitos_ctx *ctx = crypto_ahash_ctx(tfm);
	struct talitos_ahash_req_ctx *req_ctx = ahash_request_ctx(areq);
	struct talitos_edesc *edesc;
	unsigned int blocksize =
			crypto_tfm_alg_blocksize(crypto_ahash_tfm(tfm));
	unsigned int nbytes_to_hash;
	unsigned int to_hash_later;
	unsigned int index;
	int chained;

	index = req_ctx->count & (blocksize - 1);
	req_ctx->count += nbytes;

	if (!req_ctx->last && (index + nbytes) < blocksize) {
		/* Buffer the partial block */
		sg_copy_to_buffer(areq->src,
				  sg_count(areq->src, nbytes, &chained),
				  req_ctx->buf + index, nbytes);
		return 0;
	}

	if (index) {
		/* partial block from previous update; chain it in. */
		sg_init_table(req_ctx->bufsl, (nbytes) ? 2 : 1);
		sg_set_buf(req_ctx->bufsl, req_ctx->buf, index);
		if (nbytes)
			scatterwalk_sg_chain(req_ctx->bufsl, 2,
					     areq->src);
		req_ctx->psrc = req_ctx->bufsl;
	} else {
		req_ctx->psrc = areq->src;
	}
	nbytes_to_hash =  index + nbytes;
	if (!req_ctx->last) {
		to_hash_later = (nbytes_to_hash & (blocksize - 1));
		if (to_hash_later) {
			int nents;
			/* Must copy to_hash_later bytes from the end
			 * to bufnext (a partial block) for later.
			 */
			nents = sg_count(areq->src, nbytes, &chained);
			sg_copy_end_to_buffer(areq->src, nents,
					      req_ctx->bufnext,
					      to_hash_later,
					      nbytes - to_hash_later);

			/* Adjust count for what will be hashed now */
			nbytes_to_hash -= to_hash_later;
		}
		req_ctx->to_hash_later = to_hash_later;
	}

	/* allocate extended descriptor */
	edesc = ahash_edesc_alloc(areq, nbytes_to_hash);
	if (IS_ERR(edesc))
		return PTR_ERR(edesc);

	edesc->desc.hdr = ctx->desc_hdr_template;

	/* On last one, request SEC to pad; otherwise continue */
	if (req_ctx->last)
		edesc->desc.hdr |= DESC_HDR_MODE0_MDEU_PAD;
	else
		edesc->desc.hdr |= DESC_HDR_MODE0_MDEU_CONT;

	/* request SEC to INIT hash. */
	if (req_ctx->first && !req_ctx->swinit)
		edesc->desc.hdr |= DESC_HDR_MODE0_MDEU_INIT;

	/* When the tfm context has a keylen, it's an HMAC.
	 * A first or last (ie. not middle) descriptor must request HMAC.
	 */
	if (ctx->keylen && (req_ctx->first || req_ctx->last))
		edesc->desc.hdr |= DESC_HDR_MODE0_MDEU_HMAC;

	return common_nonsnoop_hash(edesc, areq, nbytes_to_hash,
				    ahash_done);
}

static int ahash_update(struct ahash_request *areq)
{
	struct talitos_ahash_req_ctx *req_ctx = ahash_request_ctx(areq);

	req_ctx->last = 0;

	return ahash_process_req(areq, areq->nbytes);
}

static int ahash_final(struct ahash_request *areq)
{
	struct talitos_ahash_req_ctx *req_ctx = ahash_request_ctx(areq);

	req_ctx->last = 1;

	return ahash_process_req(areq, 0);
}

static int ahash_finup(struct ahash_request *areq)
{
	struct talitos_ahash_req_ctx *req_ctx = ahash_request_ctx(areq);

	req_ctx->last = 1;

	return ahash_process_req(areq, areq->nbytes);
}

static int ahash_digest(struct ahash_request *areq)
{
	struct talitos_ahash_req_ctx *req_ctx = ahash_request_ctx(areq);
	struct crypto_ahash *ahash = crypto_ahash_reqtfm(areq);

	ahash->init(areq);
	req_ctx->last = 1;

	return ahash_process_req(areq, areq->nbytes);
}

struct talitos_alg_template {
	u32 type;
	union {
		struct crypto_alg crypto;
		struct ahash_alg hash;
	} alg;
	__be32 desc_hdr_template;
};

static struct talitos_alg_template driver_algs[] = {
	/* AEAD algorithms.  These use a single-pass ipsec_esp descriptor */
	{	.type = CRYPTO_ALG_TYPE_AEAD,
		.alg.crypto = {
			.cra_name = "authenc(hmac(sha1),cbc(aes))",
			.cra_driver_name = "authenc-hmac-sha1-cbc-aes-talitos",
			.cra_blocksize = AES_BLOCK_SIZE,
			.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
			.cra_type = &crypto_aead_type,
			.cra_aead = {
				.setkey = aead_setkey,
				.setauthsize = aead_setauthsize,
				.encrypt = aead_encrypt,
				.decrypt = aead_decrypt,
				.givencrypt = aead_givencrypt,
				.geniv = "<built-in>",
				.ivsize = AES_BLOCK_SIZE,
				.maxauthsize = SHA1_DIGEST_SIZE,
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_IPSEC_ESP |
			             DESC_HDR_SEL0_AESU |
		                     DESC_HDR_MODE0_AESU_CBC |
		                     DESC_HDR_SEL1_MDEUA |
		                     DESC_HDR_MODE1_MDEU_INIT |
		                     DESC_HDR_MODE1_MDEU_PAD |
		                     DESC_HDR_MODE1_MDEU_SHA1_HMAC,
	},
	{	.type = CRYPTO_ALG_TYPE_AEAD,
		.alg.crypto = {
			.cra_name = "authenc(hmac(sha1),cbc(des3_ede))",
			.cra_driver_name = "authenc-hmac-sha1-cbc-3des-talitos",
			.cra_blocksize = DES3_EDE_BLOCK_SIZE,
			.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
			.cra_type = &crypto_aead_type,
			.cra_aead = {
				.setkey = aead_setkey,
				.setauthsize = aead_setauthsize,
				.encrypt = aead_encrypt,
				.decrypt = aead_decrypt,
				.givencrypt = aead_givencrypt,
				.geniv = "<built-in>",
				.ivsize = DES3_EDE_BLOCK_SIZE,
				.maxauthsize = SHA1_DIGEST_SIZE,
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_IPSEC_ESP |
			             DESC_HDR_SEL0_DEU |
		                     DESC_HDR_MODE0_DEU_CBC |
		                     DESC_HDR_MODE0_DEU_3DES |
		                     DESC_HDR_SEL1_MDEUA |
		                     DESC_HDR_MODE1_MDEU_INIT |
		                     DESC_HDR_MODE1_MDEU_PAD |
		                     DESC_HDR_MODE1_MDEU_SHA1_HMAC,
	},
	{	.type = CRYPTO_ALG_TYPE_AEAD,
		.alg.crypto = {
			.cra_name = "authenc(hmac(sha256),cbc(aes))",
			.cra_driver_name = "authenc-hmac-sha256-cbc-aes-talitos",
			.cra_blocksize = AES_BLOCK_SIZE,
			.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
			.cra_type = &crypto_aead_type,
			.cra_aead = {
				.setkey = aead_setkey,
				.setauthsize = aead_setauthsize,
				.encrypt = aead_encrypt,
				.decrypt = aead_decrypt,
				.givencrypt = aead_givencrypt,
				.geniv = "<built-in>",
				.ivsize = AES_BLOCK_SIZE,
				.maxauthsize = SHA256_DIGEST_SIZE,
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_IPSEC_ESP |
			             DESC_HDR_SEL0_AESU |
		                     DESC_HDR_MODE0_AESU_CBC |
		                     DESC_HDR_SEL1_MDEUA |
		                     DESC_HDR_MODE1_MDEU_INIT |
		                     DESC_HDR_MODE1_MDEU_PAD |
		                     DESC_HDR_MODE1_MDEU_SHA256_HMAC,
	},
	{	.type = CRYPTO_ALG_TYPE_AEAD,
		.alg.crypto = {
			.cra_name = "authenc(hmac(sha256),cbc(des3_ede))",
			.cra_driver_name = "authenc-hmac-sha256-cbc-3des-talitos",
			.cra_blocksize = DES3_EDE_BLOCK_SIZE,
			.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
			.cra_type = &crypto_aead_type,
			.cra_aead = {
				.setkey = aead_setkey,
				.setauthsize = aead_setauthsize,
				.encrypt = aead_encrypt,
				.decrypt = aead_decrypt,
				.givencrypt = aead_givencrypt,
				.geniv = "<built-in>",
				.ivsize = DES3_EDE_BLOCK_SIZE,
				.maxauthsize = SHA256_DIGEST_SIZE,
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_IPSEC_ESP |
			             DESC_HDR_SEL0_DEU |
		                     DESC_HDR_MODE0_DEU_CBC |
		                     DESC_HDR_MODE0_DEU_3DES |
		                     DESC_HDR_SEL1_MDEUA |
		                     DESC_HDR_MODE1_MDEU_INIT |
		                     DESC_HDR_MODE1_MDEU_PAD |
		                     DESC_HDR_MODE1_MDEU_SHA256_HMAC,
	},
	{	.type = CRYPTO_ALG_TYPE_AEAD,
		.alg.crypto = {
			.cra_name = "authenc(hmac(md5),cbc(aes))",
			.cra_driver_name = "authenc-hmac-md5-cbc-aes-talitos",
			.cra_blocksize = AES_BLOCK_SIZE,
			.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
			.cra_type = &crypto_aead_type,
			.cra_aead = {
				.setkey = aead_setkey,
				.setauthsize = aead_setauthsize,
				.encrypt = aead_encrypt,
				.decrypt = aead_decrypt,
				.givencrypt = aead_givencrypt,
				.geniv = "<built-in>",
				.ivsize = AES_BLOCK_SIZE,
				.maxauthsize = MD5_DIGEST_SIZE,
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_IPSEC_ESP |
			             DESC_HDR_SEL0_AESU |
		                     DESC_HDR_MODE0_AESU_CBC |
		                     DESC_HDR_SEL1_MDEUA |
		                     DESC_HDR_MODE1_MDEU_INIT |
		                     DESC_HDR_MODE1_MDEU_PAD |
		                     DESC_HDR_MODE1_MDEU_MD5_HMAC,
	},
	{	.type = CRYPTO_ALG_TYPE_AEAD,
		.alg.crypto = {
			.cra_name = "authenc(hmac(md5),cbc(des3_ede))",
			.cra_driver_name = "authenc-hmac-md5-cbc-3des-talitos",
			.cra_blocksize = DES3_EDE_BLOCK_SIZE,
			.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
			.cra_type = &crypto_aead_type,
			.cra_aead = {
				.setkey = aead_setkey,
				.setauthsize = aead_setauthsize,
				.encrypt = aead_encrypt,
				.decrypt = aead_decrypt,
				.givencrypt = aead_givencrypt,
				.geniv = "<built-in>",
				.ivsize = DES3_EDE_BLOCK_SIZE,
				.maxauthsize = MD5_DIGEST_SIZE,
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_IPSEC_ESP |
			             DESC_HDR_SEL0_DEU |
		                     DESC_HDR_MODE0_DEU_CBC |
		                     DESC_HDR_MODE0_DEU_3DES |
		                     DESC_HDR_SEL1_MDEUA |
		                     DESC_HDR_MODE1_MDEU_INIT |
		                     DESC_HDR_MODE1_MDEU_PAD |
		                     DESC_HDR_MODE1_MDEU_MD5_HMAC,
	},
	/* ABLKCIPHER algorithms. */
	{	.type = CRYPTO_ALG_TYPE_ABLKCIPHER,
		.alg.crypto = {
			.cra_name = "cbc(aes)",
			.cra_driver_name = "cbc-aes-talitos",
			.cra_blocksize = AES_BLOCK_SIZE,
			.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER |
                                     CRYPTO_ALG_ASYNC,
			.cra_type = &crypto_ablkcipher_type,
			.cra_ablkcipher = {
				.setkey = ablkcipher_setkey,
				.encrypt = ablkcipher_encrypt,
				.decrypt = ablkcipher_decrypt,
				.geniv = "eseqiv",
				.min_keysize = AES_MIN_KEY_SIZE,
				.max_keysize = AES_MAX_KEY_SIZE,
				.ivsize = AES_BLOCK_SIZE,
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_COMMON_NONSNOOP_NO_AFEU |
				     DESC_HDR_SEL0_AESU |
				     DESC_HDR_MODE0_AESU_CBC,
	},
	{	.type = CRYPTO_ALG_TYPE_ABLKCIPHER,
		.alg.crypto = {
			.cra_name = "cbc(des3_ede)",
			.cra_driver_name = "cbc-3des-talitos",
			.cra_blocksize = DES3_EDE_BLOCK_SIZE,
			.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER |
                                     CRYPTO_ALG_ASYNC,
			.cra_type = &crypto_ablkcipher_type,
			.cra_ablkcipher = {
				.setkey = ablkcipher_setkey,
				.encrypt = ablkcipher_encrypt,
				.decrypt = ablkcipher_decrypt,
				.geniv = "eseqiv",
				.min_keysize = DES3_EDE_KEY_SIZE,
				.max_keysize = DES3_EDE_KEY_SIZE,
				.ivsize = DES3_EDE_BLOCK_SIZE,
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_COMMON_NONSNOOP_NO_AFEU |
			             DESC_HDR_SEL0_DEU |
		                     DESC_HDR_MODE0_DEU_CBC |
		                     DESC_HDR_MODE0_DEU_3DES,
	},
	/* AHASH algorithms. */
	{	.type = CRYPTO_ALG_TYPE_AHASH,
		.alg.hash = {
			.init = ahash_init,
			.update = ahash_update,
			.final = ahash_final,
			.finup = ahash_finup,
			.digest = ahash_digest,
			.halg.digestsize = MD5_DIGEST_SIZE,
			.halg.base = {
				.cra_name = "md5",
				.cra_driver_name = "md5-talitos",
				.cra_blocksize = MD5_BLOCK_SIZE,
				.cra_flags = CRYPTO_ALG_TYPE_AHASH |
					     CRYPTO_ALG_ASYNC,
				.cra_type = &crypto_ahash_type
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_COMMON_NONSNOOP_NO_AFEU |
				     DESC_HDR_SEL0_MDEUA |
				     DESC_HDR_MODE0_MDEU_MD5,
	},
	{	.type = CRYPTO_ALG_TYPE_AHASH,
		.alg.hash = {
			.init = ahash_init,
			.update = ahash_update,
			.final = ahash_final,
			.finup = ahash_finup,
			.digest = ahash_digest,
			.halg.digestsize = SHA1_DIGEST_SIZE,
			.halg.base = {
				.cra_name = "sha1",
				.cra_driver_name = "sha1-talitos",
				.cra_blocksize = SHA1_BLOCK_SIZE,
				.cra_flags = CRYPTO_ALG_TYPE_AHASH |
					     CRYPTO_ALG_ASYNC,
				.cra_type = &crypto_ahash_type
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_COMMON_NONSNOOP_NO_AFEU |
				     DESC_HDR_SEL0_MDEUA |
				     DESC_HDR_MODE0_MDEU_SHA1,
	},
	{	.type = CRYPTO_ALG_TYPE_AHASH,
		.alg.hash = {
			.init = ahash_init,
			.update = ahash_update,
			.final = ahash_final,
			.finup = ahash_finup,
			.digest = ahash_digest,
			.halg.digestsize = SHA224_DIGEST_SIZE,
			.halg.base = {
				.cra_name = "sha224",
				.cra_driver_name = "sha224-talitos",
				.cra_blocksize = SHA224_BLOCK_SIZE,
				.cra_flags = CRYPTO_ALG_TYPE_AHASH |
					     CRYPTO_ALG_ASYNC,
				.cra_type = &crypto_ahash_type
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_COMMON_NONSNOOP_NO_AFEU |
				     DESC_HDR_SEL0_MDEUA |
				     DESC_HDR_MODE0_MDEU_SHA224,
	},
	{	.type = CRYPTO_ALG_TYPE_AHASH,
		.alg.hash = {
			.init = ahash_init,
			.update = ahash_update,
			.final = ahash_final,
			.finup = ahash_finup,
			.digest = ahash_digest,
			.halg.digestsize = SHA256_DIGEST_SIZE,
			.halg.base = {
				.cra_name = "sha256",
				.cra_driver_name = "sha256-talitos",
				.cra_blocksize = SHA256_BLOCK_SIZE,
				.cra_flags = CRYPTO_ALG_TYPE_AHASH |
					     CRYPTO_ALG_ASYNC,
				.cra_type = &crypto_ahash_type
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_COMMON_NONSNOOP_NO_AFEU |
				     DESC_HDR_SEL0_MDEUA |
				     DESC_HDR_MODE0_MDEU_SHA256,
	},
	{	.type = CRYPTO_ALG_TYPE_AHASH,
		.alg.hash = {
			.init = ahash_init,
			.update = ahash_update,
			.final = ahash_final,
			.finup = ahash_finup,
			.digest = ahash_digest,
			.halg.digestsize = SHA384_DIGEST_SIZE,
			.halg.base = {
				.cra_name = "sha384",
				.cra_driver_name = "sha384-talitos",
				.cra_blocksize = SHA384_BLOCK_SIZE,
				.cra_flags = CRYPTO_ALG_TYPE_AHASH |
					     CRYPTO_ALG_ASYNC,
				.cra_type = &crypto_ahash_type
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_COMMON_NONSNOOP_NO_AFEU |
				     DESC_HDR_SEL0_MDEUB |
				     DESC_HDR_MODE0_MDEUB_SHA384,
	},
	{	.type = CRYPTO_ALG_TYPE_AHASH,
		.alg.hash = {
			.init = ahash_init,
			.update = ahash_update,
			.final = ahash_final,
			.finup = ahash_finup,
			.digest = ahash_digest,
			.halg.digestsize = SHA512_DIGEST_SIZE,
			.halg.base = {
				.cra_name = "sha512",
				.cra_driver_name = "sha512-talitos",
				.cra_blocksize = SHA512_BLOCK_SIZE,
				.cra_flags = CRYPTO_ALG_TYPE_AHASH |
					     CRYPTO_ALG_ASYNC,
				.cra_type = &crypto_ahash_type
			}
		},
		.desc_hdr_template = DESC_HDR_TYPE_COMMON_NONSNOOP_NO_AFEU |
				     DESC_HDR_SEL0_MDEUB |
				     DESC_HDR_MODE0_MDEUB_SHA512,
	},
};

struct talitos_crypto_alg {
	struct list_head entry;
	struct device *dev;
	struct talitos_alg_template algt;
};

static int talitos_cra_init(struct crypto_tfm *tfm)
{
	struct crypto_alg *alg = tfm->__crt_alg;
	struct talitos_crypto_alg *talitos_alg;
	struct talitos_ctx *ctx = crypto_tfm_ctx(tfm);

	if ((alg->cra_flags & CRYPTO_ALG_TYPE_MASK) == CRYPTO_ALG_TYPE_AHASH)
		talitos_alg = container_of(__crypto_ahash_alg(alg),
					   struct talitos_crypto_alg,
					   algt.alg.hash);
	else
		talitos_alg = container_of(alg, struct talitos_crypto_alg,
					   algt.alg.crypto);

	/* update context with ptr to dev */
	ctx->dev = talitos_alg->dev;

	/* copy descriptor header template value */
	ctx->desc_hdr_template = talitos_alg->algt.desc_hdr_template;

	return 0;
}

static int talitos_cra_init_aead(struct crypto_tfm *tfm)
{
	struct talitos_ctx *ctx = crypto_tfm_ctx(tfm);

	talitos_cra_init(tfm);

	/* random first IV */
	get_random_bytes(ctx->iv, TALITOS_MAX_IV_LENGTH);

	return 0;
}

static int talitos_cra_init_ahash(struct crypto_tfm *tfm)
{
	struct talitos_ctx *ctx = crypto_tfm_ctx(tfm);

	talitos_cra_init(tfm);

	ctx->keylen = 0;
	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct talitos_ahash_req_ctx));

	return 0;
}

/*
 * given the alg's descriptor header template, determine whether descriptor
 * type and primary/secondary execution units required match the hw
 * capabilities description provided in the device tree node.
 */
static int hw_supports(struct device *dev, __be32 desc_hdr_template)
{
	struct talitos_private *priv = dev_get_drvdata(dev);
	int ret;

	ret = (1 << DESC_TYPE(desc_hdr_template) & priv->desc_types) &&
	      (1 << PRIMARY_EU(desc_hdr_template) & priv->exec_units);

	if (SECONDARY_EU(desc_hdr_template))
		ret = ret && (1 << SECONDARY_EU(desc_hdr_template)
		              & priv->exec_units);

	return ret;
}

static int talitos_remove(struct of_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct talitos_private *priv = dev_get_drvdata(dev);
	struct talitos_crypto_alg *t_alg, *n;
	int i;

	list_for_each_entry_safe(t_alg, n, &priv->alg_list, entry) {
		switch (t_alg->algt.type) {
		case CRYPTO_ALG_TYPE_ABLKCIPHER:
		case CRYPTO_ALG_TYPE_AEAD:
			crypto_unregister_alg(&t_alg->algt.alg.crypto);
			break;
		case CRYPTO_ALG_TYPE_AHASH:
			crypto_unregister_ahash(&t_alg->algt.alg.hash);
			break;
		}
		list_del(&t_alg->entry);
		kfree(t_alg);
	}

	if (hw_supports(dev, DESC_HDR_SEL0_RNG))
		talitos_unregister_rng(dev);

	for (i = 0; i < priv->num_channels; i++)
		if (priv->chan[i].fifo)
			kfree(priv->chan[i].fifo);

	kfree(priv->chan);

	if (priv->irq[1] != NO_IRQ) {
		free_irq(priv->irq[1], priv);
		irq_dispose_mapping(priv->irq[1]);
	}

	if (priv->irq[0] != NO_IRQ) {
		free_irq(priv->irq[0], priv);
		irq_dispose_mapping(priv->irq[0]);
	}

	for_each_possible_cpu(i) {
		napi_disable(per_cpu_ptr(priv->done_task, i));
		netif_napi_del(per_cpu_ptr(priv->done_task, i));
	}
	free_percpu(priv->done_task); /* Alloc PER CPU structure */

	iounmap(priv->reg);
	if (priv->dma_dev_common.chancnt)
		talitos_unregister_async_xor(dev);

	dev_set_drvdata(dev, NULL);

	if (priv->netcrypto_cache != NULL)
		kmem_cache_destroy(priv->netcrypto_cache);
	kfree(priv);
#ifdef CONFIG_AS_FASTPATH
	pg_talitos_dev = NULL;
	pg_talitos_privdata = NULL;
#endif
	return 0;
}

static struct talitos_crypto_alg *talitos_alg_alloc(struct device *dev,
						    struct talitos_alg_template
						           *template)
{
	struct talitos_private *priv = dev_get_drvdata(dev);
	struct talitos_crypto_alg *t_alg;
	struct crypto_alg *alg;

	t_alg = kzalloc(sizeof(struct talitos_crypto_alg), GFP_KERNEL);
	if (!t_alg)
		return ERR_PTR(-ENOMEM);

	t_alg->algt = *template;

	switch (t_alg->algt.type) {
	case CRYPTO_ALG_TYPE_ABLKCIPHER:
		alg = &t_alg->algt.alg.crypto;
		alg->cra_init = talitos_cra_init;
		break;
	case CRYPTO_ALG_TYPE_AEAD:
		alg = &t_alg->algt.alg.crypto;
		alg->cra_init = talitos_cra_init_aead;
		break;
	case CRYPTO_ALG_TYPE_AHASH:
		alg = &t_alg->algt.alg.hash.halg.base;
		alg->cra_init = talitos_cra_init_ahash;
		if (!(priv->features & TALITOS_FTR_SHA224_HWINIT) &&
		    !strcmp(alg->cra_name, "sha224")) {
			t_alg->algt.alg.hash.init = ahash_init_sha224_swinit;
			t_alg->algt.desc_hdr_template =
					DESC_HDR_TYPE_COMMON_NONSNOOP_NO_AFEU |
					DESC_HDR_SEL0_MDEUA |
					DESC_HDR_MODE0_MDEU_SHA256;
		}
		break;
	}

	alg->cra_module = THIS_MODULE;
	alg->cra_priority = TALITOS_CRA_PRIORITY;
	alg->cra_alignmask = 0;
	alg->cra_ctxsize = sizeof(struct talitos_ctx);

	t_alg->dev = dev;

	return t_alg;
}
static void update_chanmap(struct talitos_private *priv, unsigned int map)
{
	u8 i = 0;
	for (i = 0; i < priv->num_channels; i++) {
		if (map & 0x1) {
			priv->core_chan_no[1][priv->core_num_chan[1]] =
							MAX_CHAN - 1 - i;
			priv->core_num_chan[1]++;
		} else {
			priv->core_chan_no[0][priv->core_num_chan[0]] =
							MAX_CHAN - 1 - i;
			priv->core_num_chan[0]++;
		}
		map = map >> 1;
	}
}
static int talitos_probe(struct of_device *ofdev,
			 const struct of_device_id *match)
{
	struct net_device *net_dev;
	struct device *dev = &ofdev->dev;
	struct device_node *np = ofdev->dev.of_node;
	struct talitos_private *priv;
	const unsigned int *prop;
	const char *name;
	int i, err;
	int grp_id;
	struct cpumask cpumask_irq;

	priv = kzalloc(sizeof(struct talitos_private), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	net_dev = alloc_percpu(struct net_device);
	for_each_possible_cpu(i) {
		(per_cpu_ptr(net_dev, i))->dev = *dev;
		INIT_LIST_HEAD(&per_cpu_ptr(net_dev, i)->napi_list);
	}

	dev_set_drvdata(dev, priv);

	priv->ofdev = ofdev;
	priv->dev = dev;
	/* Alloc PER CPU structure */
	priv->done_task = alloc_percpu(struct napi_struct);
	for_each_possible_cpu(i) {
		netif_napi_add(per_cpu_ptr(net_dev, i),
			per_cpu_ptr(priv->done_task, i),
			talitos_done, TALITOS_NAPI_WEIGHT);
		napi_enable(per_cpu_ptr(priv->done_task, i));
	}

	INIT_LIST_HEAD(&priv->alg_list);

	name = of_get_property(np, "fsl,multi-host-mode", NULL);
	if (!name || !strcmp(name, "primary")) {
		priv->dual = 0;
		priv->secondary = 0;
	} else if (!strcmp(name, "secondary")) {
		priv->dual = 0;
		priv->secondary = 1;
	} else if (!strcmp(name, "dual")) {
		if (nr_cpu_ids < 2) {
			/* add one cpu support to P1/P2 for both SMP and Non-SMP,
			 * use primary as the default mode. */
			priv->dual = 0;
			priv->secondary = 0;
		} else if (nr_cpu_ids == 2) {
			priv->dual = 1;
			priv->secondary = 0;
		} else {
			dev_err(dev, "can't work in dual host mode with CPU "
					"number not equal to 2 or 1\n");
			err = -EINVAL;
			goto err_out;
		}
	} else {
		dev_err(dev, "invalid multi-host-mode in device tree node\n");
		err = -EINVAL;
		goto err_out;
	}

	if (priv->secondary)
		priv->irq[0] = irq_of_parse_and_map(np, 1);
	else
		priv->irq[0] = irq_of_parse_and_map(np, 0);

	if (priv->irq[0] == NO_IRQ) {
		dev_err(dev, "failed to map irq[0]\n");
		err = -EINVAL;
		goto err_out;
	}

	/* get the irq line */
	err = request_irq(priv->irq[0], talitos_interrupt, 0,
			  dev_driver_string(dev), priv);
	if (err) {
		dev_err(dev, "failed to request irq[0] %d\n", priv->irq[0]);
		irq_dispose_mapping(priv->irq[0]);
		priv->irq[0] = NO_IRQ;
		goto err_out;
	}

	priv->reg = of_iomap(np, 0);
	if (!priv->reg) {
		dev_err(dev, "failed to of_iomap\n");
		err = -ENOMEM;
		goto err_out;
	}

	/* get SEC version capabilities from device tree */
	prop = of_get_property(np, "fsl,num-channels", NULL);
	if (prop)
		priv->num_channels = *prop;

	prop = of_get_property(np, "fsl,channel-fifo-len", NULL);
	if (prop)
		priv->chfifo_len = *prop;

	prop = of_get_property(np, "fsl,exec-units-mask", NULL);
	if (prop)
		priv->exec_units = *prop;

	prop = of_get_property(np, "fsl,descriptor-types-mask", NULL);
	if (prop)
		priv->desc_types = *prop;

	if (!is_power_of_2(priv->num_channels) || !priv->chfifo_len ||
	    !priv->exec_units || !priv->desc_types) {
		dev_err(dev, "invalid property data in device tree node\n");
		err = -EINVAL;
		goto err_out;
	}

	if (of_device_is_compatible(np, "fsl,sec3.0")) {
		priv->features |= TALITOS_FTR_SRC_LINK_TBL_LEN_INCLUDES_EXTENT;
		prop = of_get_property(np, "fsl,channel-remap", NULL);
		if (prop)
			priv->chan_remap = *prop;
		update_chanmap(priv, priv->chan_remap);
		if (priv->chan_remap && priv->dual) {
			priv->irq[1] = irq_of_parse_and_map(np, 1);

			if (priv->irq[1] == NO_IRQ) {
				dev_err(dev, "failed to map irq[1]\n");
				err = -EINVAL;
				goto err_out;
			}
			/* get the irq[1] line */
			err = request_irq(priv->irq[1], talitos_interrupt, 0,
					dev_driver_string(dev), priv);
			if (err) {
				dev_err(dev, "failed to request irq[1] %d\n",
					priv->irq[1]);
				irq_dispose_mapping(priv->irq[1]);
				priv->irq[1] = NO_IRQ;
				goto err_out;
			}

			/* set correct interrupt affinity */
			cpumask_clear(&cpumask_irq);
			cpumask_set_cpu(0, &cpumask_irq);
			irq_set_affinity(priv->irq[0], &cpumask_irq);

			cpumask_clear(&cpumask_irq);
			cpumask_set_cpu(1, &cpumask_irq);
			irq_set_affinity(priv->irq[1], &cpumask_irq);
		}
	}
	for (grp_id = 0; grp_id < MAX_GROUPS; grp_id++) {
		for (i = 0; i < priv->core_num_chan[grp_id]; i++) {
			priv->chan_isr[grp_id] +=
			MAP_ARRAY(priv->core_chan_no[grp_id][i]);
			priv->chan_imr[grp_id] +=
			MAP_ARRAY_DONE(priv->core_chan_no[grp_id][i]);
		}
	}
	if (of_device_is_compatible(np, "fsl,sec2.1"))
		priv->features |= TALITOS_FTR_HW_AUTH_CHECK |
				  TALITOS_FTR_SHA224_HWINIT;

	priv->chan = kzalloc(sizeof(struct talitos_channel) *
			     priv->num_channels, GFP_KERNEL);
	if (!priv->chan) {
		dev_err(dev, "failed to allocate channel management space\n");
		err = -ENOMEM;
		goto err_out;
	}

	for (i = 0; i < priv->num_channels; i++) {
		priv->chan[i].id = i;
		priv->chan[i].priv = priv;
		priv->chan[i].head = 0;
		priv->chan[i].tail = 0;
	}

	priv->fifo_len = roundup_pow_of_two(priv->chfifo_len);

	for (i = 0; i < priv->num_channels; i++) {
		/* save memory, remove if dynamic channel map is used */
		if (!is_channel_used(i, priv))
			continue;
		priv->chan[i].fifo = kzalloc(sizeof(struct talitos_request) *
					     priv->fifo_len, GFP_KERNEL);
		if (!priv->chan[i].fifo) {
			dev_err(dev, "failed to allocate request fifo %d\n", i);
			err = -ENOMEM;
			goto err_out;
		}
	}

	for (i = 0; i < priv->num_channels; i++)
		priv->chan[i].submit_count =
			   -(priv->chfifo_len - 1);

	dma_set_mask(dev, DMA_BIT_MASK(36));

	/* reset and initialize the h/w */
	if (!priv->secondary) {
		err = init_device(dev);
		if (err) {
			dev_err(dev, "failed to initialize device\n");
			goto err_out;
		}
	}

	/* register the RNG, if available */
	if (hw_supports(dev, DESC_HDR_SEL0_RNG)) {
		err = talitos_register_rng(dev);
		if (err) {
			dev_err(dev, "failed to register hwrng: %d\n", err);
			goto err_out;
		} else
			dev_info(dev, "hwrng\n");
	}

	priv->netcrypto_cache = kmem_cache_create("netcrypto_cache",
						MAX_DESC_LEN, 0,
					SLAB_HWCACHE_ALIGN, NULL);
	if (!priv->netcrypto_cache) {
		printk(KERN_ERR "%s: failed to create block cache\n",
				__func__);
		err = -ENOMEM;
		goto err_out;
	}

	/*
	 * register with async_tx xor, if capable
	 * SEC 2.x support up to 3 RAID sources,
	 * SEC 3.x support up to 6
	 */
	if (hw_supports(dev, DESC_HDR_SEL0_AESU | DESC_HDR_TYPE_RAID_XOR)) {
		int max_xor_srcs = 3;
		if (of_device_is_compatible(np, "fsl,sec3.0"))
			max_xor_srcs = 6;

		err = talitos_register_async_tx(dev, max_xor_srcs);
		if (err) {
			dev_err(dev, "failed to register async_tx xor: %d\n",
				err);
			goto err_out;
		}
		dev_info(dev, "max_xor_srcs %d\n", max_xor_srcs);
	}

	/* register crypto algorithms the device supports */
	for (i = 0; i < ARRAY_SIZE(driver_algs); i++) {
		if (hw_supports(dev, driver_algs[i].desc_hdr_template)) {
			struct talitos_crypto_alg *t_alg;
			char *name = NULL;

			t_alg = talitos_alg_alloc(dev, &driver_algs[i]);
			if (IS_ERR(t_alg)) {
				err = PTR_ERR(t_alg);
				goto err_out;
			}

			switch (t_alg->algt.type) {
			case CRYPTO_ALG_TYPE_ABLKCIPHER:
			case CRYPTO_ALG_TYPE_AEAD:
				err = crypto_register_alg(
						&t_alg->algt.alg.crypto);
				name = t_alg->algt.alg.crypto.cra_driver_name;
				break;
			case CRYPTO_ALG_TYPE_AHASH:
				err = crypto_register_ahash(
						&t_alg->algt.alg.hash);
				name =
				 t_alg->algt.alg.hash.halg.base.cra_driver_name;
				break;
			}
			if (err) {
				dev_err(dev, "%s alg registration failed\n",
					name);
				kfree(t_alg);
			} else {
				list_add_tail(&t_alg->entry, &priv->alg_list);
				dev_info(dev, "%s\n", name);
			}
		}
	}
#ifdef CONFIG_AS_FASTPATH
	pg_talitos_dev = dev;
	pg_talitos_privdata =  priv;
#endif
	return 0;

err_out:
	talitos_remove(ofdev);

	return err;
}

#ifdef CONFIG_AS_FASTPATH
struct device *talitos_getdevice(void)
{
	return pg_talitos_dev;
}
EXPORT_SYMBOL(talitos_getdevice);

dma_addr_t talitos_dma_map_single(void *data, unsigned int len, int dir)
{
	return dma_map_single(pg_talitos_dev, data, len, dir);
}
EXPORT_SYMBOL(talitos_dma_map_single);

void talitos_dma_unmap_single(void *data, unsigned int len, int dir)
{
	dma_unmap_single(pg_talitos_dev, data, len, dir);
}
EXPORT_SYMBOL(talitos_dma_unmap_single);
#endif

static const struct of_device_id talitos_match[] = {
	{
		.compatible = "fsl,sec2.0",
	},
	{},
};
MODULE_DEVICE_TABLE(of, talitos_match);

static struct of_platform_driver talitos_driver = {
	.driver = {
		.name = "talitos",
		.owner = THIS_MODULE,
		.of_match_table = talitos_match,
	},
	.probe = talitos_probe,
	.remove = talitos_remove,
};

static int __init talitos_init(void)
{
#ifdef CONFIG_AS_FASTPATH
	printk(KERN_INFO "SEC FASTPATH Enabled\r\n");
#endif
	return of_register_platform_driver(&talitos_driver);
}
module_init(talitos_init);

static void __exit talitos_exit(void)
{
	of_unregister_platform_driver(&talitos_driver);
}
module_exit(talitos_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kim Phillips <kim.phillips@freescale.com>");
MODULE_DESCRIPTION("Freescale integrated security engine (SEC) driver");

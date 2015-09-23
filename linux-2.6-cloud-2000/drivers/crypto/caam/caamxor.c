/*
 * caam - Freescale Integrated Security Engine (SEC) device driver
 * Support for off-loading XOR Parity Calculations to CAAM.
 *
 * Copyright 2011 Freescale Semiconductor, Inc
 */

#include <linux/slab.h>
#include <linux/string.h>

#include "regs.h"
#include "caamxor.h"
#include "desc.h"
#include "jq.h"
#include "error.h"
#include "dcl/dcl.h"

static void prepare_caam_xor_desc(struct device *dev,
				  struct caam_dma_async_tx_desc *desc,
				  dma_addr_t sh_desc_phys,
				  dma_addr_t dest, dma_addr_t *src,
				  u32 src_cnt, size_t len)
{
	int i;
	u32 *job_descptr = desc->job_desc;
	u32 *cmd_desc = desc->cmd_desc;
	const u32 move_jump_cmds[] = {
		0xB8A30000,  /* JUMP to First Source handler at index 35 */
		0x7C031C10,  /* MOVE class 1 context offset 0x20 to index 7 */
		0x7E031C10,  /* MOVE class 1 context offset 0x30 to index 7 */
		0x78031C10,  /* MOVE class 2 context offset 0x00 to index 7 */
		0x7A131C10,  /* MOVE class 2 context offset 0x10 to index 7 */
		0x7C131C10,  /* MOVE class 2 context offset 0x20 to index 7 */
		0x7E131C10,  /* MOVE class 2 context offset 0x30 to index 7 */
		0xB8B00000,  /* JUMP to Last handler at index 48 */
	};

	for (i = 0; i < (src_cnt - 1); i++) {
		*cmd_desc++ = src[i];
		*cmd_desc++ = len;
		*cmd_desc++ = move_jump_cmds[i];
		*cmd_desc++ = 0xA0000001;
	}

	*cmd_desc++ = src[i];
	*cmd_desc++ = len;
	*cmd_desc++ = move_jump_cmds[7];
	*cmd_desc++ = 0xA0000001;

	desc->cmd_desc_phys = dma_map_single(dev, desc->cmd_desc,
					     CMD_DESC_LEN * sizeof(u32),
					     DMA_TO_DEVICE);
	job_descptr = cmd_insert_hdr(job_descptr, SH_DESC_LEN, 7,
				     SHR_ALWAYS,
				     SHRNXT_SHARED /* has_shared */ ,
				     ORDER_REVERSE,
				     DESC_STD /*don't make trusted */);

	*job_descptr++ = sh_desc_phys;
	*job_descptr++ = 0xF8400000;
	*job_descptr++ = dest;
	*job_descptr++ = len;

	job_descptr = cmd_insert_seq_in_ptr(job_descptr, desc->cmd_desc_phys,
					    src_cnt * 16, PTR_DIRECT);
}

static void prepare_caam_xor_sh_desc(u32 *descptr, u32 src_cnt)
{
	const u32 ififo_infodata = 0x10F00080;
	const u32 chunk_size_32 = 32;
	const u32 chunk_size_64 = 64;
	const u32 chunk_size_128 = 128;

	descptr =
	    cmd_insert_shared_hdr(descptr, 0, SH_DESC_LEN, CTX_ERASE,
				  SHR_NEVER);
	descptr =
	    cmd_insert_seq_load(descptr, LDST_CLASS_1_CCB, 0,
				LDST_SRCDST_BYTE_CONTEXT, 0,
				((src_cnt > 4) ? 64 : (src_cnt * 16)));

	if (src_cnt > 4) {
		descptr = cmd_insert_seq_load(descptr, LDST_CLASS_2_CCB, 0,
					      LDST_SRCDST_BYTE_CONTEXT, 0,
					      (src_cnt - 4) * 16);
	} else {
		descptr = cmd_insert_jump(descptr, JUMP_TYPE_LOCAL, 0,
					  JUMP_TEST_ALL, 0, 1, 0);
	}

	descptr =
	    cmd_insert_load(descptr, (void *)&ififo_infodata, LDST_CLASS_DECO,
			    0, LDST_SRCDST_WORD_DECOCTRL, 8, 0, ITEM_INLINE);
	descptr =
	    cmd_insert_move(descptr, MOVE_WAITCOMP, MOVE_SRC_CLASS1CTX,
			    MOVE_DEST_DESCBUF, 28, 16);
	descptr = cmd_insert_shared_hdr(descptr, 6, 0, CTX_ERASE, SHR_NEVER);

	cmd_insert_seq_in_ptr(descptr, 0, 0x400000, PTR_DIRECT);
	descptr++;
	*descptr++ = 0x00000000;
	*descptr++ = 0x00000000;
	*descptr++ = 0x00000000;
	*descptr++ = 0x00000000;

	descptr = cmd_insert_seq_fifo_load(descptr, LDST_CLASS_IND_CCB, 1,
					   0, 0);
	descptr = cmd_insert_seq_fifo_load(descptr, LDST_CLASS_1_CCB, 0,
					   FIFOLD_TYPE_PK, 128);
	descptr = cmd_insert_math(descptr, MATH_FUN_ADD,
				  MATH_SRC0_VARSEQOUTLEN, MATH_SRC1_IMM,
				  MATH_DEST_VARSEQOUTLEN,
				  sizeof(chunk_size_128), 0, 0, 0,
				  (u32 *) &chunk_size_128);
	descptr =
	    cmd_insert_load(descptr, (void *)&ififo_infodata,
			    LDST_CLASS_IND_CCB, 0, LDST_SRCDST_WORD_INFO_FIFO,
			    0, 4, ITEM_INLINE);
	descptr =
	    cmd_insert_move(descptr, MOVE_WAITCOMP, MOVE_SRC_INFIFO,
			    MOVE_DEST_MATH0, 0, 32);
	descptr =
	    cmd_insert_math(descptr, MATH_FUN_XOR, MATH_SRC0_REG0,
			    MATH_SRC1_OUTFIFO, MATH_DEST_REG0, 8, 0, 0, 0, 0);
	descptr =
	    cmd_insert_math(descptr, MATH_FUN_XOR, MATH_SRC0_REG1,
			    MATH_SRC1_OUTFIFO, MATH_DEST_REG1, 8, 0, 0, 0, 0);
	descptr =
	    cmd_insert_math(descptr, MATH_FUN_XOR, MATH_SRC0_REG2,
			    MATH_SRC1_OUTFIFO, MATH_DEST_REG2, 8, 0, 0, 0, 0);
	descptr =
	    cmd_insert_math(descptr, MATH_FUN_XOR, MATH_SRC0_REG3,
			    MATH_SRC1_OUTFIFO, MATH_DEST_REG3, 8, 0, 0, 0, 0);
	descptr =
	    cmd_insert_move(descptr, MOVE_WAITCOMP, MOVE_SRC_MATH0,
			    MOVE_DEST_OUTFIFO, 0, 32);
	descptr =
	    cmd_insert_math(descptr, MATH_FUN_SUB, MATH_SRC0_VARSEQOUTLEN,
			    MATH_SRC1_IMM, MATH_DEST_VARSEQOUTLEN,
			    sizeof(chunk_size_32), 0, 0, 0,
			    (u32 *) &chunk_size_32);
	descptr =
	    cmd_insert_jump(descptr, JUMP_TYPE_LOCAL, 0, JUMP_TEST_ALL,
			    JUMP_COND_MATH_Z, 5, 0);
	descptr =
	    cmd_insert_math(descptr, MATH_FUN_SUB, MATH_SRC0_VARSEQOUTLEN,
			    MATH_SRC1_IMM, MATH_DEST_NONE,
			    sizeof(chunk_size_64), 0, 0, 0,
			    (u32 *) &chunk_size_64);
	descptr =
	    cmd_insert_jump(descptr, JUMP_TYPE_LOCAL, 0, JUMP_TEST_ALL,
			    JUMP_COND_MATH_Z, -22, 0);

	*descptr++ = 0xA00000F4;
	descptr = cmd_insert_seq_fifo_store(descptr, LDST_CLASS_IND_CCB, 0,
					    FIFOST_TYPE_MESSAGE_DATA,
					    chunk_size_128);
	descptr =
	    cmd_insert_math(descptr, MATH_FUN_SUB, MATH_SRC0_SEQOUTLEN,
			    MATH_SRC1_ONE, MATH_DEST_NONE, sizeof(int), 0, 0, 0,
			    0);
	descptr =
	    cmd_insert_jump(descptr, JUMP_TYPE_HALT_USER, 0, JUMP_TEST_ALL,
			    JUMP_COND_MATH_N, 0, 0);
	descptr =
	    cmd_insert_move(descptr, 0, MOVE_SRC_INFIFO, MOVE_DEST_OUTFIFO, 0,
			    128);
	descptr = cmd_insert_shared_hdr(descptr, 6, 0, CTX_ERASE, SHR_NEVER);
	descptr = cmd_insert_seq_fifo_load(descptr, LDST_CLASS_IND_CCB, 1,
					   0, 0);
	descptr = cmd_insert_math(descptr, MATH_FUN_SUB,
				  MATH_SRC0_SEQINLEN, MATH_SRC1_ONE,
				  MATH_DEST_NONE, sizeof(int), 0, 0, 0, 0);
	descptr = cmd_insert_jump(descptr, JUMP_TYPE_LOCAL, 0,
				  JUMP_TEST_ALL, JUMP_COND_MATH_N, -20, 0);
	descptr = cmd_insert_seq_fifo_load(descptr, LDST_CLASS_1_CCB, 0,
					   FIFOLD_TYPE_PK, 128);
	descptr =
	    cmd_insert_load(descptr, (void *)&ififo_infodata,
			    LDST_CLASS_IND_CCB, 0, LDST_SRCDST_WORD_INFO_FIFO,
			    0, 4, ITEM_INLINE);
	*descptr++ = 0x7A031C10;	/* cmd_insert_move */
	descptr = cmd_insert_math(descptr, MATH_FUN_SUB,
				  MATH_SRC0_VARSEQINLEN, MATH_SRC1_ONE,
				  MATH_DEST_NONE, sizeof(int), 0, 0, 0, 0);
	descptr = cmd_insert_jump(descptr, JUMP_TYPE_LOCAL, 0,
				  JUMP_TEST_INVALL, JUMP_COND_MATH_N, -26, 0);
	descptr = cmd_insert_move(descptr, 0, MOVE_SRC_INFIFO,
				  MOVE_DEST_OUTFIFO, 0, 128);
	descptr = cmd_insert_math(descptr, MATH_FUN_ADD,
				  MATH_SRC0_SEQINLEN, MATH_SRC1_IMM,
				  MATH_DEST_SEQOUTLEN, sizeof(chunk_size_128),
				  0, 0, 0, (u32 *) &chunk_size_128);
	descptr = cmd_insert_shared_hdr(descptr, 6, 0, CTX_ERASE, SHR_NEVER);
	descptr = cmd_insert_seq_fifo_load(descptr, LDST_CLASS_IND_CCB, 1,
					   0, 0);
	descptr = cmd_insert_math(descptr, MATH_FUN_ADD,
				  MATH_SRC0_VARSEQINLEN, MATH_SRC1_IMM,
				  MATH_DEST_VARSEQINLEN, sizeof(chunk_size_128),
				  0, 0, 0, (u32 *) &chunk_size_128);
	descptr = cmd_insert_move(descptr, 0, MOVE_SRC_CLASS1CTX,
				  MOVE_DEST_DESCBUF, 28, 16);
	descptr = cmd_insert_shared_hdr(descptr, 12, 0, CTX_ERASE, SHR_NEVER);
}

static enum dma_status caam_jq_tx_status(struct dma_chan *chan,
					 dma_cookie_t cookie,
					 struct dma_tx_state *txstate)
{
	struct caam_dma_jq *jq = NULL;
	dma_cookie_t last_used;
	dma_cookie_t last_complete;

	jq = container_of(chan, struct caam_dma_jq, chan);

	last_used = chan->cookie;
	last_complete = jq->completed_cookie;

	dma_set_tx_state(txstate, last_complete, last_used, 0);

	return dma_async_is_complete(cookie, last_complete, last_used);
}

static void caam_dma_xor_done(struct device *dev, u32 *hwdesc, u32 status,
			      void *auxarg)
{
	struct caam_dma_async_tx_desc *desc;
	struct caam_dma_jq *dma_jq;
	dma_async_tx_callback callback;
	void *callback_param;

	desc = (struct caam_dma_async_tx_desc *)auxarg;
	dma_jq = desc->dma_jq;

	if (status) {
		char tmp[256];
		dev_err(dev, "%s\n", caam_jq_strstatus(tmp, status));
	}

	spin_lock_bh(&dma_jq->desc_lock);

	if (dma_jq->completed_cookie < desc->async_tx.cookie) {
		dma_jq->completed_cookie = desc->async_tx.cookie;
		if (dma_jq->completed_cookie == DMA_MAX_COOKIE)
			dma_jq->completed_cookie = DMA_MIN_COOKIE;
	}

	callback = desc->async_tx.callback;
	callback_param = desc->async_tx.callback_param;

	dma_unmap_single(dma_jq->caam_hw_jq->parentdev,
			 desc->cmd_desc_phys, CMD_DESC_LEN * sizeof(u32),
			 DMA_TO_DEVICE);

	if (dma_jq->soft_desc->desc_cnt < MAX_INITIAL_DESCS) {
		list_add(&desc->node, &dma_jq->soft_desc->head);
		dma_jq->soft_desc->desc_cnt++;
	} else
		kfree(desc);

	spin_unlock_bh(&dma_jq->desc_lock);
	if (callback)
		callback(callback_param);
}

static void caam_jq_issue_pending(struct dma_chan *chan)
{
	struct caam_dma_jq *dma_jq = NULL;
	struct caam_dma_async_tx_desc *desc, *_desc;
	struct device *dev;
	int ret;

	dma_jq = container_of(chan, struct caam_dma_jq, chan);
	dev = dma_jq->dev;

	spin_lock_bh(&dma_jq->desc_lock);
	list_for_each_entry_safe(desc, _desc, &dma_jq->submit_q, node) {
		desc->dma_jq = dma_jq;
		ret = caam_jq_enqueue(dev, desc->job_desc,
				      caam_dma_xor_done, desc);
		if (ret) {
			if (ret == -EIO)
				pr_err("%s: I/O error while submitting"
				       "DMA-XOR request to CAAM block\n",
				       __func__);

			spin_unlock_bh(&dma_jq->desc_lock);

			return;
		}

		list_del_init(&desc->node);
	}

	spin_unlock_bh(&dma_jq->desc_lock);
}

static dma_cookie_t caam_jq_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct caam_dma_async_tx_desc *desc = NULL;
	struct caam_dma_jq *jq = NULL;
	dma_cookie_t cookie;

	desc = container_of(tx, struct caam_dma_async_tx_desc, async_tx);
	jq = container_of(tx->chan, struct caam_dma_jq, chan);

	spin_lock_bh(&jq->desc_lock);

	cookie = jq->chan.cookie + 1;
	if (cookie < DMA_MIN_COOKIE)
		cookie = DMA_MIN_COOKIE;

	desc->async_tx.cookie = cookie;
	jq->chan.cookie = desc->async_tx.cookie;
	list_add_tail(&desc->node, &jq->submit_q);

	spin_unlock_bh(&jq->desc_lock);

	return cookie;
}

struct dma_async_tx_descriptor *caam_jq_prep_dma_xor(struct dma_chan *chan,
						     dma_addr_t dest,
						     dma_addr_t *src,
						     unsigned int src_cnt,
						     size_t len,
						     unsigned long flags)
{
	struct caam_dma_jq *jq = NULL;
	struct caam_dma_async_tx_desc *desc = NULL;
	struct caam_drv_private *priv;

	jq = container_of(chan, struct caam_dma_jq, chan);

	if (src_cnt > MAX_XOR_SRCS) {
		dev_err(jq->dev, "%d XOR sources are greater then maximum"
			"supported %d XOR sources\n", src_cnt, MAX_XOR_SRCS);
		return NULL;
	}

	spin_lock_bh(&jq->desc_lock);
	if (jq->soft_desc->desc_cnt) {
		desc = container_of(jq->soft_desc->head.next,
				    struct caam_dma_async_tx_desc, node);
		jq->soft_desc->desc_cnt--;
		list_del_init(&desc->node);
	}
	spin_unlock_bh(&jq->desc_lock);

	if (!desc) {
		desc = kzalloc(sizeof(struct caam_dma_async_tx_desc),
			       GFP_KERNEL);
		if (!desc) {
			dev_err(jq->dev, "Faied to allocate memory for XOR"
				"async transaction\n");

			return ERR_PTR(-ENOMEM);
		}

		desc->async_tx.tx_submit = caam_jq_tx_submit;
	}

	dma_async_tx_descriptor_init(&desc->async_tx, &jq->chan);
	INIT_LIST_HEAD(&desc->node);

	priv = dev_get_drvdata(jq->caam_hw_jq->parentdev);
	prepare_caam_xor_desc(jq->caam_hw_jq->parentdev, desc,
			      priv->xor_sh_desc[src_cnt - 2].sh_desc_phys,
			      dest, src, src_cnt, len);

	desc->async_tx.parent = NULL;
	desc->async_tx.next = NULL;

	desc->async_tx.flags = flags;
	desc->async_tx.cookie = -EBUSY;
	async_tx_ack(&desc->async_tx);

	return &desc->async_tx;
}

static void caam_jq_free_chan_resources(struct dma_chan *chan)
{
	/* TBD */

	return;
}

static int caam_jq_alloc_chan_resources(struct dma_chan *chan)
{
	struct caam_dma_jq *jq = container_of(chan, struct caam_dma_jq, chan);
	struct caam_dma_async_tx_desc *desc;
	unsigned int i;

	jq->soft_desc = kzalloc(sizeof(struct caam_dma_desc_pool), GFP_KERNEL);
	if (!jq->soft_desc) {
		pr_err("%s: Failed to allocate resources for DMA channel\n",
		       __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&jq->soft_desc->head);
	for (i = 0; i < MAX_INITIAL_DESCS; i++) {
		desc = kzalloc(sizeof(struct caam_dma_async_tx_desc),
			       GFP_KERNEL);
		if (!desc)
			break;

		desc->async_tx.tx_submit = caam_jq_tx_submit;
		jq->soft_desc->desc_cnt++;
		list_add_tail(&desc->node, &jq->soft_desc->head);
	}

	return 0;
}

static int caam_jq_chan_bind(struct device *ctrldev, struct device *dev)
{
	struct caam_drv_private *priv = dev_get_drvdata(ctrldev);
	struct caam_drv_private_jq *jqpriv = dev_get_drvdata(dev);
	struct dma_device *dma_dev = &priv->dma_dev;
	struct caam_dma_jq *dma_jq;

	dma_jq = kzalloc(sizeof(struct caam_dma_jq), GFP_KERNEL);
	if (!dma_jq) {
		dev_err(dev, "Failed to allocate memory for caam job queue\n");
		return -ENOMEM;
	}

	dma_jq->chan.device = dma_dev;
	dma_jq->chan.private = dma_jq;

	INIT_LIST_HEAD(&dma_jq->submit_q);
	spin_lock_init(&dma_jq->desc_lock);
	list_add_tail(&dma_jq->chan.device_node, &dma_dev->channels);
	dma_dev->chancnt++;

	dma_jq->caam_hw_jq = jqpriv;
	dma_jq->dev = dev;

	return 0;
}

int caam_jq_dma_init(struct device *ctrldev)
{
	struct caam_drv_private *priv = dev_get_drvdata(ctrldev);
	struct dma_device *dma_dev = NULL;
	int i;

	priv->xor_sh_desc =
	    kzalloc(sizeof(struct caam_xor_sh_desc) * (MAX_XOR_SRCS - 2),
		    GFP_KERNEL);
	if (!priv->xor_sh_desc) {
		dev_err(ctrldev,
			"Failed to allocate memory for XOR Shared"
			"descriptor\n");
		return -ENOMEM;
	}

	for (i = 0; i < (MAX_XOR_SRCS - 2); i++) {
		prepare_caam_xor_sh_desc(priv->xor_sh_desc[i].desc, i + 2);
		priv->xor_sh_desc[i].sh_desc_phys =
		    dma_map_single(ctrldev, &priv->xor_sh_desc[i].desc,
				   SH_DESC_LEN * sizeof(u32), DMA_TO_DEVICE);
	}

	dma_dev = &priv->dma_dev;
	dma_dev->dev = ctrldev;
	INIT_LIST_HEAD(&dma_dev->channels);

	dma_dev->max_xor = MAX_XOR_SRCS;

	/*
	 * xor transaction must be 128 bytes aligned. For unaligned
	 * transaction, xor-parity calculations will not be off-loaded
	 * to caam
	 */
	dma_dev->xor_align = 8;
	dma_cap_set(DMA_XOR, dma_dev->cap_mask);

	dma_dev->device_alloc_chan_resources = caam_jq_alloc_chan_resources;
	dma_dev->device_tx_status = caam_jq_tx_status;
	dma_dev->device_issue_pending = caam_jq_issue_pending;
	dma_dev->device_prep_dma_xor = caam_jq_prep_dma_xor;
	dma_dev->device_free_chan_resources = caam_jq_free_chan_resources;

	for (i = 0; i < priv->total_jobqs; i++)
		caam_jq_chan_bind(ctrldev, priv->jqdev[i]);

	dma_async_device_register(dma_dev);

	return 0;
}

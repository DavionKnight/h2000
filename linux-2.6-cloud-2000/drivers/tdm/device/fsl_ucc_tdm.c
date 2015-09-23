/*
 * Freescale QUICC Engine TDM Device Driver
 *
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author:	Haiying Wang	<Haiying.Wang@freescale.com>
 *		Kai Jiang	<Kai.Jiang@freescale.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This driver adds support for TDM devices via Freescale's QUICC Engine.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/tdm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <sysdev/fsl_soc.h>
#include <sysdev/fsl_soc.h>
#include "fsl_ucc_tdm.h"

#define DRV_DESC "Freescale QE UCC TDM Driver"
#define DRV_NAME "ucc_tdm"

#undef DEBUG

static struct ucc_tdm_info utdm_primary_info = {
	.uf_info = {
		.tsa = 1,
		.cdp = 1,
		.cds = 1,
		.ctsp = 1,
		.ctss = 1,
		.revd = 0,
		.urfs = 256,
		.utfs = 256,
		.urfet = 128,
		.urfset = 192,
		.utfet = 128,
		.utftt = 0x40,
		.ufpt = 256,
		.ttx_trx = UCC_FAST_GUMR_TRANSPARENT_TTX_TRX_TRANSPARENT,
		.tenc = UCC_FAST_TX_ENCODING_NRZ,
		.renc = UCC_FAST_RX_ENCODING_NRZ,
		.tcrc = UCC_FAST_16_BIT_CRC,
		.synl = UCC_FAST_SYNC_LEN_NOT_USED,
	},

	.si_info = {
		.simr_rfsd = 1,		/* pq_mds_t1 card need 1 bit delay */
		.simr_tfsd = 0,
		.simr_crt = 0,
		.simr_sl = 0,
		.simr_ce = 1,
		.simr_fe = 1,
		.simr_gm = 0,
	},
};

static struct ucc_tdm_info utdm_info[MAX_TDM_NUM];

#ifdef DEBUG
static void dump_siram(struct ucc_tdm_private *priv)
{
	int i;
	u16 *siram = priv->siram;

	pr_info("Dump the SI RX RAM\n");
	for (i = 0; i < priv->num_of_ts; i++) {
		pr_info("%04x ", siram[priv->siram_entry_id * 32 + i]);
		if ((i + 1) % 4)
			pr_info("\n");
	}

	pr_info("Dump the SI TX RAM\n");
	for (i = 0; i < priv->num_of_ts; i++) {
		pr_info("%04x ", siram[priv->siram_entry_id * 32 + 0x200 + i]);
		if ((i + 1) % 4)
			pr_info("\n");
	}
}

static void mem_disp(u8 *addr, int size)
{
	u8 *i;
	int size16Aling = (size >> 4) << 4;
	int size4Aling = (size >> 2) << 2;
	int notAlign = 0;
	if (size % 16)
		notAlign = 1;

	for (i = addr; (u32) i < (u32) addr + size16Aling; i += 16)
		pr_info("0x%08x: %08x %08x %08x %08x\r\n",
			(u32) i,
			*((u32 *) (i)),
			*((u32 *) (i + 4)),
			*((u32 *) (i + 8)), *((u32 *) (i + 12)));
	if (notAlign == 1)
		pr_info("0x%08x: ", (u32) i);
	for (; (u32) i < (u32) addr + size4Aling; i += 4)
		pr_info("%08x ", *((u32 *) (i)));
	for (; (u32) i < (u32) addr + size; i++)
		pr_info("%02x", *((u8 *) (i)));
	if (notAlign == 1)
		pr_info("\r\n");
}

static void dump_ucc(struct ucc_tdm_private *priv)
{
	struct ucc_transparent_param *ucc_pram;
	ucc_pram = priv->ucc_pram;

	pr_info("Dumping UCC %d Registers\n", priv->ut_info->uf_info.ucc_num);
	ucc_fast_dump_regs(priv->uccf);
	pr_info("Dumping UCC %d Parameter RAM\n",
			priv->ut_info->uf_info.ucc_num);
	pr_info("rbase = 0x%x\n", in_be32(&ucc_pram->rbase));
	pr_info("rbptr = 0x%x\n", in_be32(&ucc_pram->rbptr));
	pr_info("mrblr = 0x%x\n", in_be16(&ucc_pram->mrblr));
	pr_info("rbdlen = 0x%x\n", in_be16(&ucc_pram->rbdlen));
	pr_info("rbdstat = 0x%x\n", in_be16(&ucc_pram->rbdstat));
	pr_info("rstate = 0x%x\n", in_be32(&ucc_pram->rstate));
	pr_info("rdptr = 0x%x\n", in_be32(&ucc_pram->rdptr));
	pr_info("riptr = 0x%x\n", in_be16(&ucc_pram->riptr));
	pr_info("tbase = 0x%x\n", in_be32(&ucc_pram->tbase));
	pr_info("tbptr = 0x%x\n", in_be32(&ucc_pram->tbptr));
	pr_info("tbdlen = 0x%x\n", in_be16(&ucc_pram->tbdlen));
	pr_info("tbdstat = 0x%x\n", in_be16(&ucc_pram->tbdstat));
	pr_info("tstate = 0x%x\n", in_be32(&ucc_pram->tstate));
	pr_info("tdptr = 0x%x\n", in_be32(&ucc_pram->tdptr));
	pr_info("tiptr = 0x%x\n", in_be16(&ucc_pram->tiptr));
	pr_info("rcrc = 0x%x\n", in_be32(&ucc_pram->rcrc));
	pr_info("tcrc = 0x%x\n", in_be32(&ucc_pram->tcrc));
	pr_info("c_mask = 0x%x\n", in_be32(&ucc_pram->c_mask));
	pr_info("c_pers = 0x%x\n", in_be32(&ucc_pram->c_pres));
	pr_info("disfc = 0x%x\n", in_be16(&ucc_pram->disfc));
	pr_info("crcec = 0x%x\n", in_be16(&ucc_pram->crcec));
}

static void dump_bds(struct ucc_tdm_private *priv)
{
	int length;

	if (priv->tx_bd) {
		length = sizeof(struct qe_bd) * NUM_OF_BUF;
		pr_info(" Dump tx BDs\n");
		mem_disp((u8 *)priv->tx_bd, length);
	}

	if (priv->rx_bd) {
		length = sizeof(struct qe_bd) * NUM_OF_BUF;
		pr_info(" Dump rx BDs\n");
		mem_disp((u8 *)priv->rx_bd, length);
	}

}

static void dump_priv(struct ucc_tdm_private *priv)
{
	pr_info("ut_info = 0x%x\n", (u32)priv->ut_info);
	pr_info("uccf = 0x%x\n", (u32)priv->uccf);
	pr_info("uf_regs = 0x%x\n", (u32)priv->uf_regs);
	pr_info("si_regs = 0x%x\n", (u32)priv->si_regs);
	pr_info("ucc_pram = 0x%x\n", (u32)priv->ucc_pram);
	pr_info("tdm_port = 0x%x\n", (u32)priv->tdm_port);
	pr_info("siram_entry_id = 0x%x\n", priv->siram_entry_id);
	pr_info("siram = 0x%x\n", (u32)priv->siram);
	pr_info("tdm_mode = 0x%x\n", (u32)priv->tdm_mode);
	pr_info("tdm_framer_type; = 0x%x\n", (u32)priv->tdm_framer_type);
	pr_info("rx_buffer; = 0x%x\n", (u32)priv->rx_buffer);
	pr_info("tx_buffer; = 0x%x\n", (u32)priv->tx_buffer);
	pr_info("dma_rx_addr; = 0x%x\n", (u32)priv->dma_rx_addr);
	pr_info("dma_tx_addr; = 0x%x\n", (u32)priv->dma_tx_addr);
	pr_info("tx_bd; = 0x%x\n", (u32)priv->tx_bd);
	pr_info("rx_bd; = 0x%x\n", (u32)priv->rx_bd);
	pr_info("phase_rx = 0x%x\n", (u32)priv->phase_rx);
	pr_info("phase_tx = 0x%x\n", (u32)priv->phase_tx);
	pr_info("ucc_pram_offset = 0x%x\n", priv->ucc_pram_offset);
	pr_info("tx_bd_offset = 0x%x\n", priv->tx_bd_offset);
	pr_info("rx_bd_offset = 0x%x\n", priv->rx_bd_offset);

}

#endif /* DEBUG */

static void init_si(struct ucc_tdm_private *priv)
{
	struct si1 __iomem *si_regs = NULL;
	u16 __iomem *siram = NULL;
	u16 siram_entry_valid = 0;
	u16 siram_entry_closed = 0;
	u16 ucc_num;
	u8 csel;
	u16 sixmr;
	u16 tdm_port;
	u32 siram_entry_id;
	u32 mask;
	int i;

	si_regs = priv->si_regs;
	siram = priv->siram;
	ucc_num = priv->ut_info->uf_info.ucc_num;
	tdm_port = priv->tdm_port;
	siram_entry_id = priv->siram_entry_id;

	/* set siram table */
	if (ucc_num < 4)
		csel = ucc_num + 9;
	else
		csel = ucc_num - 3;

	siram_entry_valid = SIR_CSEL(csel) | SIR_BYTE | SIR_CNT(0);
	siram_entry_closed = SIR_IDLE | SIR_BYTE | SIR_CNT(0);

	for (i = 0; i < priv->num_of_ts; i++) {
		mask = 0x01 << i;

		if (priv->tx_ts_mask & mask)
			out_be16(&siram[siram_entry_id * 32 + i],
					siram_entry_valid);
		else
			out_be16(&siram[siram_entry_id * 32 + i],
					siram_entry_closed);

		if (priv->rx_ts_mask & mask)
			out_be16(&siram[siram_entry_id * 32 + 0x200 +  i],
					siram_entry_valid);
		else
			out_be16(&siram[siram_entry_id * 32 + 0x200 +  i],
					siram_entry_closed);
	}

	setbits16(&siram[(siram_entry_id * 32) + (priv->num_of_ts - 1)],
			SIR_LAST);
	setbits16(&siram[(siram_entry_id * 32) + 0x200 + (priv->num_of_ts - 1)],
			SIR_LAST);

	/* Set SIxMR register */
	sixmr = SIMR_SAD(siram_entry_id);

	sixmr &= ~SIMR_SDM_MASK;

	if (priv->tdm_mode == TDM_INTERNAL_LOOPBACK)
		sixmr |= SIMR_SDM_INTERNAL_LOOPBACK;
	else
		sixmr |= SIMR_SDM_NORMAL;

	sixmr |= SIMR_RFSD(priv->ut_info->si_info.simr_rfsd) |
			SIMR_TFSD(priv->ut_info->si_info.simr_tfsd);

	if (priv->ut_info->si_info.simr_crt)
		sixmr |= SIMR_CRT;
	if (priv->ut_info->si_info.simr_sl)
		sixmr |= SIMR_SL;
	if (priv->ut_info->si_info.simr_ce)
		sixmr |= SIMR_CE;
	if (priv->ut_info->si_info.simr_fe)
		sixmr |= SIMR_FE;
	if (priv->ut_info->si_info.simr_gm)
		sixmr |= SIMR_GM;

	switch (tdm_port) {
	case 0:
		out_be16(&si_regs->siamr1, sixmr);
		break;
	case 1:
		out_be16(&si_regs->sibmr1, sixmr);
		break;
	case 2:
		out_be16(&si_regs->sicmr1, sixmr);
		break;
	case 3:
		out_be16(&si_regs->sidmr1, sixmr);
		break;
	default:
		printk(KERN_ERR"can not find tdm sixmr reg\n");
		break;
	}

#ifdef DEBUG
	dump_siram(priv);
#endif

}
static int utdm_init(struct ucc_tdm_private *priv)
{
	struct ucc_tdm_info *ut_info;
	struct ucc_fast_info *uf_info;
	u32 cecr_subblock;
	u32 bd_status;
	int ret, i;
	void *bd_buffer;
	dma_addr_t bd_dma_addr;
	u32 riptr;
	u32 tiptr;

	ut_info = priv->ut_info;
	uf_info = &ut_info->uf_info;

	if (priv->tdm_framer_type == TDM_FRAMER_T1)
		priv->num_of_ts = 24;
	if (priv->tdm_framer_type == TDM_FRAMER_E1)
		priv->num_of_ts = 32;

	uf_info->uccm_mask = (u32) (UCC_TRANS_UCCE_RXB << 16);

	if (ucc_fast_init(uf_info, &priv->uccf)) {
		printk(KERN_ERR "%s: Failed to init uccf.", __func__);
		return -ENOMEM;
}

	priv->uf_regs = priv->uccf->uf_regs;
	ucc_fast_disable(priv->uccf, COMM_DIR_RX | COMM_DIR_TX);

	/* Initialize SI */
	init_si(priv);

	/* Write to QE CECR, UCCx channel to Stop Transmission */
	cecr_subblock = ucc_fast_get_qe_cr_subblock(uf_info->ucc_num);
	ret = qe_issue_cmd(QE_STOP_TX, cecr_subblock,
		(u8) QE_CR_PROTOCOL_UNSPECIFIED, 0);

	/* Set UPSMR */
	out_be32(&priv->uf_regs->upsmr, 0);

	/* Alloc Rx BD */
	priv->rx_bd_offset = qe_muram_alloc(NUM_OF_BUF * sizeof(struct qe_bd),
			QE_ALIGNMENT_OF_BD);
	if (IS_ERR_VALUE(priv->rx_bd_offset)) {
		printk(KERN_ERR "%s: Cannot allocate MURAM memory for RxBDs\n",
				__func__);
		ret = -ENOMEM;
		goto rxbd_alloc_error;
	}

	/* Alloc Tx BD */
	priv->tx_bd_offset = qe_muram_alloc(NUM_OF_BUF * sizeof(struct qe_bd),
				QE_ALIGNMENT_OF_BD);
	if (IS_ERR_VALUE(priv->tx_bd_offset)) {
		printk(KERN_ERR "%s: Cannot allocate MURAM memory for TxBDs\n",
				__func__);
		ret = -ENOMEM;
		goto txbd_alloc_error;
	}

	priv->tx_bd = qe_muram_addr(priv->tx_bd_offset);
	priv->rx_bd = qe_muram_addr(priv->rx_bd_offset);

	/* Alloc parameter ram for ucc transparent */
	priv->ucc_pram_offset = qe_muram_alloc(sizeof(priv->ucc_pram),
				ALIGNMENT_OF_UCC_TRANS_PRAM);

	if (IS_ERR_VALUE(priv->ucc_pram_offset)) {
		printk(KERN_ERR "%s: Can not allocate MURAM for hdlc prameter.\n",
				__func__);
		return -ENOMEM;
		goto pram_alloc_error;
	}

	/* init parameter base */
	cecr_subblock = ucc_fast_get_qe_cr_subblock(uf_info->ucc_num);
	ret = qe_issue_cmd(QE_ASSIGN_PAGE_TO_DEVICE, cecr_subblock,
			QE_CR_PROTOCOL_UNSPECIFIED, priv->ucc_pram_offset);

	priv->ucc_pram = (struct ucc_transparent_param __iomem *)
					qe_muram_addr(priv->ucc_pram_offset);

	/* Zero out parameter ram */
	memset_io(priv->ucc_pram, 0, sizeof(struct ucc_transparent_param));

	/* Alloc riptr, tiptr */
	riptr = qe_muram_alloc(32, 32);
	if (IS_ERR_VALUE(riptr)) {
		printk(KERN_ERR "%s: Cannot allocate MURAM mem for Receive"
			" internal temp data pointer\n", __func__);
		ret = -ENOMEM;
		goto riptr_alloc_error;
	}

	tiptr = qe_muram_alloc(32, 32);
	if (IS_ERR_VALUE(tiptr)) {
		printk(KERN_ERR "%s: Cannot allocate MURAM mem for transmit"
			" internal temp data pointer\n", __func__);
		ret = -ENOMEM;
		goto tiptr_alloc_error;
	}

	/* Set RIPTR, TIPTR */
	out_be16(&priv->ucc_pram->riptr, (u16)riptr);
	out_be16(&priv->ucc_pram->tiptr, (u16)tiptr);

	/* Set MRBLR */
	out_be16(&priv->ucc_pram->mrblr, (u16)MAX_RX_BUF_LENGTH);

	/* Set RBASE, TBASE */
	out_be32(&priv->ucc_pram->rbase, (u32)cpm_muram_dma(priv->rx_bd));
	out_be32(&priv->ucc_pram->tbase, (u32)cpm_muram_dma(priv->tx_bd));

	/* Set RSTATE, TSTATE */
	out_be32(&priv->ucc_pram->rstate, 0x30000000);
	out_be32(&priv->ucc_pram->tstate, 0x30000000);

	/* Set C_MASK, C_PRES for 16bit CRC */
	out_be32(&priv->ucc_pram->c_mask, 0x0000F0B8);
	out_be32(&priv->ucc_pram->c_pres, 0x0000FFFF);

	out_be16(&priv->ucc_pram->res0, 0);
	for (i = 0; i < 4; i++)
		out_be32(&priv->ucc_pram->res4[i], 0x0);

	/* Get BD buffer */
	bd_buffer = dma_alloc_coherent(NULL, 2 * NUM_OF_BUF * MAX_RX_BUF_LENGTH,
				 &bd_dma_addr, GFP_KERNEL);

	if (!bd_buffer) {
		printk("Could not allocate buffer descriptors\n");
		return -ENOMEM;
	}

	memset(bd_buffer, 0, 2 * NUM_OF_BUF * MAX_RX_BUF_LENGTH);

	priv->rx_buffer = bd_buffer;
	priv->tx_buffer = bd_buffer + NUM_OF_BUF * MAX_RX_BUF_LENGTH;

	priv->dma_rx_addr = bd_dma_addr;
	priv->dma_tx_addr = bd_dma_addr + NUM_OF_BUF * MAX_RX_BUF_LENGTH;

	for (i = 0; i < NUM_OF_BUF; i++) {
		if (i < (NUM_OF_BUF - 1))
			bd_status = R_E | R_I | R_CM;
		else
			bd_status = R_E | R_I | R_W | R_CM;

		out_be32((u32 *)(priv->rx_bd + i), bd_status);
		out_be32(&priv->rx_bd[i].buf, priv->dma_rx_addr
				+ i * MAX_RX_BUF_LENGTH);

		if (i < (NUM_OF_BUF - 1))
			bd_status =  T_I;
		else
			bd_status =  T_I | T_W;

		out_be32((u32 *)(priv->tx_bd + i), bd_status);
		out_be32(&priv->tx_bd[i].buf, priv->dma_tx_addr
				+ i * MAX_RX_BUF_LENGTH);
	}

	priv->phase_rx = 0;
	priv->phase_tx = 0;

	return 0;

tiptr_alloc_error:
	qe_muram_free(riptr);
riptr_alloc_error:
	qe_muram_free(priv->ucc_pram_offset);
pram_alloc_error:
	qe_muram_free(priv->tx_bd_offset);
txbd_alloc_error:
	qe_muram_free(priv->rx_bd_offset);
rxbd_alloc_error:
	ucc_fast_free(priv->uccf);

	return ret;
}

static int ucc_tdm_read(struct tdm_adapter *adap, u8 *tdm_buffer, u32 len)
{

	struct ucc_tdm_private *priv = tdm_get_adapdata(adap);
	u8 phase_rx;
	u32 i;
	u32 byte_copy;
	u8 *recv_buf;

	wait_event_interruptible(priv->tdm_queue, priv->tdm_queue_flag != 0);
	priv->tdm_queue_flag = 0;

	if (priv->phase_rx == 0)
		phase_rx = NUM_OF_BUF - 1;
	else
		phase_rx = priv->phase_rx - 1;

	recv_buf = priv->rx_buffer + phase_rx * MAX_RX_BUF_LENGTH;

	if (len > MAX_RX_BUF_LENGTH)
		byte_copy = MAX_RX_BUF_LENGTH;
	else
		byte_copy = len;

	for (i = 0; i < byte_copy; i++)
		*(tdm_buffer +  i) = recv_buf[i];

	return byte_copy;

}


static int ucc_tdm_write(struct tdm_adapter *adap, u8 *write_buf,
		 unsigned int len)
{
	struct ucc_tdm_private *priv = tdm_get_adapdata(adap);
	struct qe_bd __iomem *bd;
	u32 bd_status;
	u8 *tdm_send_buf;
	u32 copy_len;
	int i, j;
	u32 buf_num;

	buf_num = len / MAX_RX_BUF_LENGTH;
	if (len % MAX_RX_BUF_LENGTH)
		buf_num += 1;

	if (buf_num > NUM_OF_BUF)
		return -EINVAL;

	for (i = 0; i < buf_num; i++) {

		if (priv->phase_tx == NUM_OF_BUF)
			priv->phase_tx = 0;

		bd = (priv->tx_bd + priv->phase_tx);
		bd_status = in_be32((u32 __iomem *)bd);
		tdm_send_buf = priv->tx_buffer +
				priv->phase_tx * MAX_RX_BUF_LENGTH;

		/* the last buf to copy */
		if (i == (buf_num - 1))
			copy_len = len - i * MAX_RX_BUF_LENGTH;
		else
			copy_len = MAX_RX_BUF_LENGTH;

		while ((bd_status = in_be32((u32 __iomem *)bd)) & T_R)
			msleep(30);

		memset(tdm_send_buf, 0xff, MAX_RX_BUF_LENGTH);

		for (j = 0; j < copy_len; j++)
			tdm_send_buf[j] = *((u8 *)write_buf + j);

		bd_status &= ~(T_L | BD_LEN_MASK);
		if (i == (buf_num - 1))
			out_be32((u32 *)(bd),
				bd_status | T_R | T_L | T_I | copy_len);
		else
			out_be32((u32 *)(bd), bd_status | T_R | T_I | copy_len);

		priv->phase_tx++;
	}

	return 0;
}

static irqreturn_t ucc_tdm_irq_handler(int irq, void *dev_id)
{
	struct ucc_tdm_private *priv = (struct ucc_tdm_private *)dev_id;
	struct ucc_fast_private *uccf;
	struct ucc_tdm_info *ut_info;
	register u32 ucce;
	register u32 uccm;

	ut_info = priv->ut_info;
	uccf = priv->uccf;

	ucce = (u32) in_be32(uccf->p_ucce);
	uccm = (u32) in_be32(uccf->p_uccm);

	if ((ucce >> 16) & UCC_TRANS_UCCE_RXB) {
		if (priv->phase_rx  == NUM_OF_BUF - 1)
			priv->phase_rx = 0;
		else
			priv->phase_rx++;

		priv->tdm_queue_flag = 1;
		wake_up_interruptible(&priv->tdm_queue);

	}

	out_be32(uccf->p_ucce, ucce);

	return IRQ_HANDLED;

}

static int utdm_start(struct tdm_adapter *adap)
{
	u32 cecr_subblock;
	struct ucc_tdm_private *priv = tdm_get_adapdata(adap);

	if (priv->tdm_busy != 1) {
		if (request_irq(priv->ut_info->uf_info.irq, ucc_tdm_irq_handler,
					0, "tdm", (void *)priv)) {
			printk(KERN_ERR "%s: request_irq for ucc tdm failed\n",
							__func__);
			return -ENODEV;
		}
		cecr_subblock =
		ucc_fast_get_qe_cr_subblock(priv->ut_info->uf_info.ucc_num);

		qe_issue_cmd(QE_INIT_TX_RX, cecr_subblock,
			(u8) QE_CR_PROTOCOL_UNSPECIFIED, 0);

		ucc_fast_enable(priv->uccf, COMM_DIR_RX | COMM_DIR_TX);

		/* Enable the TDM port */
		priv->si_regs->siglmr1_h |= (0x1 << priv->tdm_port);
		priv->phase_rx = 0;
		priv->phase_tx = 0;
		priv->tdm_busy = 1;
		msleep(100);
	} else
		printk(KERN_ERR"TDM IS RUNNING!\n");

#ifdef DEBUG
	dump_priv(priv);
	dump_ucc(priv);
	dump_bds(priv);
#endif

	return 0;
}

static void utdm_memclean(struct ucc_tdm_private *priv)
{
	qe_muram_free(priv->ucc_pram->riptr);
	qe_muram_free(priv->ucc_pram->tiptr);

	if (priv->rx_bd) {
		qe_muram_free(priv->rx_bd_offset);
		priv->rx_bd = NULL;
		priv->rx_bd_offset = 0;
	}

	if (priv->tx_bd) {
		qe_muram_free(priv->tx_bd_offset);
		priv->tx_bd = NULL;
		priv->tx_bd_offset = 0;
	}

	if (priv->ucc_pram) {
		qe_muram_free(priv->ucc_pram_offset);
		priv->ucc_pram = NULL;
		priv->ucc_pram_offset = 0;
	 }

	if (priv->uf_regs) {
		iounmap(priv->uf_regs);
		priv->uf_regs = NULL;
	}

	if (priv->uccf) {
		ucc_fast_free(priv->uccf);
		priv->uccf = NULL;
	}

	if (priv->rx_buffer) {
		dma_free_coherent(NULL, 2 * NUM_OF_BUF * MAX_RX_BUF_LENGTH,
			priv->rx_buffer, priv->dma_rx_addr);
		priv->rx_buffer = NULL;
		priv->dma_rx_addr = 0;
	}
}

static int utdm_stop(struct tdm_adapter *adap)
{
	struct ucc_tdm_private *priv = tdm_get_adapdata(adap);
	u32 cecr_subblock;

	cecr_subblock = ucc_fast_get_qe_cr_subblock(
			priv->ut_info->uf_info.ucc_num);

	qe_issue_cmd(QE_GRACEFUL_STOP_TX, cecr_subblock,
			(u8) QE_CR_PROTOCOL_UNSPECIFIED, 0);
	qe_issue_cmd(QE_CLOSE_RX_BD, cecr_subblock,
			(u8) QE_CR_PROTOCOL_UNSPECIFIED, 0);

	priv->si_regs->siglmr1_h &= ~(0x1 << priv->tdm_port);
	ucc_fast_disable(priv->uccf, COMM_DIR_RX | COMM_DIR_TX);

	free_irq(priv->ut_info->uf_info.irq, priv);
	priv->tdm_busy = 0;

	return 0;
}

static const struct tdm_algorithm tdm_algo = {
	.tdm_read_simple = ucc_tdm_read,
	.tdm_write_simple = ucc_tdm_write,
	.tdm_enable = utdm_start,
	.tdm_disable = utdm_stop,
	.functionality = NULL,
};

static struct tdm_adapter ucc_tdm_ops = {
	.owner = THIS_MODULE,
	.algo = &tdm_algo,
};

static enum tdm_mode_t set_tdm_mode(const char *tdm_mode_type)
{
	if (strcasecmp(tdm_mode_type, "internal-loopback") == 0)
		return TDM_INTERNAL_LOOPBACK;
	else if (strcasecmp(tdm_mode_type, "normal") == 0)
		return TDM_NORMAL;
	else
		return TDM_NORMAL;
}


static enum tdm_framer_t set_tdm_framer(const char *tdm_framer_type)
{
	if (strcasecmp(tdm_framer_type, "t1") == 0)
		return TDM_FRAMER_T1;
	else if (strcasecmp(tdm_framer_type, "e1") == 0)
		return TDM_FRAMER_E1;
	else
		return TDM_FRAMER_T1;
}

static int set_si_param(struct ucc_tdm_private *priv)
{
	struct si_mode_info *si_info = &priv->ut_info->si_info;

	if (priv->tdm_mode == TDM_INTERNAL_LOOPBACK) {
		si_info->simr_crt = 1;
		si_info->simr_rfsd = 0;
	}

	return 0;
}

static int __devinit ucc_tdm_probe(struct of_device *ofdev,
				const struct of_device_id *match)
{
	struct device_node *np = ofdev->dev.of_node;
	struct ucc_tdm_private *utdm_priv = NULL;
	struct ucc_tdm_info *ut_info;
	struct resource res;
	int ucc_num;
	const unsigned int *prop;
	const char *sprop;
	struct device_node *dnp;
	int ret;
	static int si_flag;

	prop = of_get_property(np, "cell-index", NULL);
	if (!prop) {
		printk(KERN_ERR "%s: Invalid ucc property\n", __func__);
		return -ENODEV;
	}

	ucc_num = *prop - 1;
	if ((ucc_num > 7) && (ucc_num < 0)) {
		printk(KERN_ERR "%s: Invalid UCC num\n", __func__);
		return -EINVAL;
	}

	ut_info = &utdm_info[ucc_num];
	ut_info->uf_info.ucc_num = ucc_num;

	sprop = of_get_property(np, "rx-clock-name", NULL);
	if (sprop) {
		ut_info->uf_info.rx_clock = qe_clock_source(sprop);
		if ((ut_info->uf_info.rx_clock < QE_CLK_NONE) ||
			(ut_info->uf_info.rx_clock > QE_CLK24)) {
			printk(KERN_ERR	"%s: Invalid rx-clock-name property\n",
					__func__);
			return -EINVAL;
		}
	} else {
		printk(KERN_ERR
			"%s: Invalid rx-clock-name property\n", __func__);
		return -EINVAL;
	}

	sprop = of_get_property(np, "tx-clock-name", NULL);
	if (sprop) {
		ut_info->uf_info.tx_clock = qe_clock_source(sprop);
		if ((ut_info->uf_info.tx_clock < QE_CLK_NONE) ||
			(ut_info->uf_info.tx_clock > QE_CLK24)) {
			printk(KERN_ERR
			"%s: Invalid tx-clock-name property\n", __func__);
		return -EINVAL;
		}
	} else {
		printk(KERN_ERR
			"%s: Invalid tx-clock-name property\n", __func__);
		return -EINVAL;
	}

	sprop = of_get_property(np, "rx-sync-clock", NULL);
	if (sprop) {
		ut_info->uf_info.tx_sync = qe_clock_source(sprop);
		if ((ut_info->uf_info.tx_sync < QE_CLK_NONE) ||
			(ut_info->uf_info.tx_sync > QE_TSYNC_PIN)) {
			printk(KERN_ERR
			"%s: Invalid rx-sync-clock property\n", __func__);
		return -EINVAL;
		}
	} else {
		printk(KERN_ERR
			"%s: Invalid rx-sync-clock property\n", __func__);
		return -EINVAL;
	}

	sprop = of_get_property(np, "tx-sync-clock", NULL);
	if (sprop) {
		ut_info->uf_info.tx_sync = qe_clock_source(sprop);
		if ((ut_info->uf_info.tx_sync < QE_CLK_NONE) ||
			(ut_info->uf_info.tx_sync > QE_TSYNC_PIN)) {
			printk(KERN_ERR
			"%s: Invalid tx-sync-clock property\n", __func__);
		return -EINVAL;
		}
	} else {
		printk(KERN_ERR
			"%s: Invalid tx-sync-clock property\n", __func__);
		return -EINVAL;
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret)
		return -EINVAL;

	ut_info->uf_info.regs = res.start;
	ut_info->uf_info.irq = irq_of_parse_and_map(np, 0);

	utdm_priv = kzalloc(sizeof(struct ucc_tdm_private), GFP_KERNEL);
	if (!utdm_priv) {
		ret = -ENOMEM;
		printk(KERN_ERR
			"%s: No mem to alloc tdm private data\n", __func__);
		goto err_alloc_priv;
	}

	dev_set_drvdata(&ofdev->dev, utdm_priv);
	utdm_priv->dev = &ofdev->dev;

	prop = of_get_property(np, "tx-timeslot", NULL);
	if (!prop) {
		ret = -EINVAL;
		printk(KERN_ERR
			"%s: Invalid tx-timeslot property\n", __func__);
		goto err_miss_property;
	}
	utdm_priv->tx_ts_mask = *prop;

	prop = of_get_property(np, "rx-timeslot", NULL);
	if (!prop) {
		ret = -EINVAL;
		printk(KERN_ERR
			"%s: Invalid rx-timeslot property\n", __func__);
		goto err_miss_property;
	}
	utdm_priv->rx_ts_mask = *prop;

	prop = of_get_property(np, "fsl,tdm-id", NULL);
	if (!prop) {
		ret = -EINVAL;
		printk(KERN_ERR
			"%s: No fsl,tdm-id property for this UCC\n", __func__);
		goto err_miss_property;
	}
	utdm_priv->tdm_port = *prop;
	ut_info->uf_info.tdm_num = utdm_priv->tdm_port ;

	prop = of_get_property(np, "tdm-mode", NULL);
	if (!prop) {
		ret = -EINVAL;
		printk(KERN_ERR
			"%s: No tdm-mode property for UCC\n", __func__);
		goto err_miss_property;
	}
	utdm_priv->tdm_mode = set_tdm_mode((const char *)prop);

	prop = of_get_property(np, "tdm-framer-type", NULL);
	if (!prop) {
		ret = -EINVAL;
		printk(KERN_ERR
			"%s: No tdm-framer-type property for UCC\n", __func__);
		goto err_miss_property;
	}
	utdm_priv->tdm_framer_type = set_tdm_framer((const char *)prop);

	prop = of_get_property(np, "fsl,siram-entry-id", NULL);
	if (!prop) {
		ret = -EINVAL;
		printk(KERN_ERR"No siram entry id for UCC\n");
		goto err_miss_property;
	}
	utdm_priv->siram_entry_id = *(const u32 *)prop;

	dnp = of_find_node_by_name(NULL, "si");
	if (!dnp) {
		ret = -EINVAL;
		printk(KERN_ERR "%s: No si property\n", __func__);
		goto err_miss_property;
	}
	of_address_to_resource(dnp, 0, &res);
	utdm_priv->si_regs = ioremap(res.start, res.end - res.start + 1);
	of_node_put(dnp);


	dnp = of_find_node_by_name(NULL, "siram");
	if (!dnp) {
		ret = -EINVAL;
		printk(KERN_ERR "%s: No siramproperty\n", __func__);
		goto err_miss_si_property;
	}
	of_address_to_resource(dnp, 0 , &res);
	utdm_priv->siram = ioremap(res.start, res.end - res.start + 1);
	of_node_put(dnp);

	if (si_flag == 0) {
		memset(utdm_priv->siram, 0,  res.end - res.start + 1);
		si_flag = 1;
	}

	utdm_priv->ut_info = ut_info;
	set_si_param(utdm_priv);

	utdm_priv->adap = kzalloc(sizeof(struct tdm_adapter), GFP_KERNEL);
	if (!utdm_priv->adap) {
		ret = -ENOMEM;
		printk(KERN_ERR "%s: No mem to alloc for adapter\n", __func__);
		goto err_adap_alloc;
	}
	sprintf(ucc_tdm_ops.name, "%s%d", "tdm_ucc_", ucc_num + 1);
	memcpy(utdm_priv->adap, &ucc_tdm_ops, sizeof(struct tdm_adapter));

	tdm_set_adapdata(utdm_priv->adap, utdm_priv);
	utdm_priv->adap->parent = &ofdev->dev;

	init_waitqueue_head(&utdm_priv->tdm_queue);
	utdm_priv->tdm_queue_flag = 0;

	ret = utdm_init(utdm_priv);
	if (ret) {
		printk(KERN_ERR "%s: Failed to init utdm\n", __func__);
		goto err_utdm_init;
	}

	ret = tdm_add_adapter(utdm_priv->adap);
	if (ret < 0) {
		printk(KERN_ERR "%s: Failed to add adapter\n", __func__);
		goto err_utdm_init;
	}

	spin_lock_init(&utdm_priv->tdmlock);

	#ifdef DEBUG
		dump_priv(utdm_priv);
		dump_ucc(utdm_priv);
		dump_bds(utdm_priv);
		mem_disp((u8 *)utdm_priv->si_regs, 0x20);
	#endif

	return 0;

err_utdm_init:
	kfree(utdm_priv->adap);
err_adap_alloc:
	iounmap(utdm_priv->siram);
err_miss_si_property:
	iounmap(utdm_priv->si_regs);
err_miss_property:
	kfree(utdm_priv);
err_alloc_priv:
	return ret;

}

static int __devexit ucc_tdm_remove(struct of_device *ofdev)
{
	struct ucc_tdm_private *priv = dev_get_drvdata(&ofdev->dev);

	utdm_stop(priv->adap);
	utdm_memclean(priv);

	if (priv->si_regs) {
		iounmap(priv->si_regs);
		priv->si_regs = NULL;
	}

	if (priv->siram) {
		iounmap(priv->siram);
		priv->siram = NULL;
	}
	kfree(priv->adap);
	kfree(priv);

	pr_info("%s UCC based tdm module removed\n", __func__);

	return 0;
}

static const struct of_device_id fsl_ucc_tdm_of_match[] = {
	{
	.compatible = "fsl,ucc-tdm",
	},
	{},
};

MODULE_DEVICE_TABLE(of, fsl_ucc_tdm_of_match);

static struct of_platform_driver ucc_tdm_driver = {
	.probe          = ucc_tdm_probe,
	.remove         = __devexit_p(ucc_tdm_remove),
	.driver         = {
		.owner  = THIS_MODULE,
		.name   = DRV_NAME,
		.of_match_table    = fsl_ucc_tdm_of_match,
	},
};

static int __init ucc_tdm_init(void)
{
	int i, ret;

	pr_info(DRV_NAME ": " DRV_DESC ":Init\n");

	for (i = 0; i < MAX_TDM_NUM; i++)
		memcpy(&(utdm_info[i]), &utdm_primary_info,
			sizeof(utdm_primary_info));

	ret = of_register_platform_driver(&ucc_tdm_driver);
	if (ret)
		pr_err(DRV_NAME
			"ucc_tdm_driver failed (%i)\n", ret);
	return ret;
}

static void __exit ucc_tdm_exit(void)
{
	pr_info(DRV_NAME ": " DRV_DESC ":Exit\n");
	of_unregister_platform_driver(&ucc_tdm_driver);
}

module_init(ucc_tdm_init);
module_exit(ucc_tdm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Freescale Semiconductor Inc.");
MODULE_DESCRIPTION("Driver For Freescale QE UCC TDM controller");
MODULE_VERSION("1.0");

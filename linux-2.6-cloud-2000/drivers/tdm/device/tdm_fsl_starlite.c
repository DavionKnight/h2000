/*
 * drivers/misc/tdm_fsl_starlite.c
 *
 * Copyright (C) 2007-2010 Freescale Semiconductor, Inc, All rights reserved.
 *
 * TDM driver for Freescale Starlite TDM controller.
 * This driver can interface with SLIC device to run VOIP kind of
 * applications.
 *
 * Author: P. V. Suresh <pala@freescale.com>
 * 	Hemant Agrawal <hemant@freescale.com>
 * 	Rajesh Gumasta <rajesh.gumasta@freescale.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

 /* Note that this is a complete rewrite of P.V. Suresh's starlite driver code.
    But we have used so much of his original code and ideas that it seems
    only fair to recognize him as co-author -- Rajesh & Hemant */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/tdm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <sysdev/fsl_soc.h>

#include "tdm_fsl_starlite.h"

#define DRV_DESC "Freescale Starlite TDM Driver Adapter"
#define DRV_NAME "tdm_fsl_starlite"

static int tdmen = 1;

module_param(tdmen, int, S_IRUSR);
MODULE_PARM_DESC(tdmen, "Enable TDM: Enable=1, Disable=0(default)");

/* Initialize the Tx Transmit Control Discriptor parameters*/
static void tx_tcd_init(struct tdm_priv *priv)
{
	int i;
	u32 iter;
	u32 offset;
	dma_addr_t physaddr;
	int bytes_in_fifo_per_frame =
	    ALIGN_SIZE(priv->cfg.num_ch * priv->cfg.ch_width, 8);

	iter = bytes_in_fifo_per_frame / 8 * priv->cfg.num_frames;

	for (i = 0; i < NUM_OF_TDM_BUF; i++) {
		offset = i * priv->cfg.num_frames * bytes_in_fifo_per_frame;
		/* saddr */
		priv->dma_tx_tcd[i]->tcd[0] = priv->dma_output_paddr + offset;

		/* ssize=dsize=64bit, soff=8, smod=dmod=0 */
		priv->dma_tx_tcd[i]->tcd[1] =
		    DMA_TCD1_SOFF(0x08) | DMA_TCD1_SSIZE(SSIZE_64BITS) |
		    DMA_TCD1_DSIZE(SSIZE_64BITS);

		/* number of bytes for minor loop, wide fifo 8bytes for dma */
		priv->dma_tx_tcd[i]->tcd[2] = 0x08;

		/* slast = 0 */
		priv->dma_tx_tcd[i]->tcd[3] = 0x0;

		/* dadr = TX FIFO */
		priv->dma_tx_tcd[i]->tcd[4] = TDM_TDR_OFFSET + priv->ptdm_base;

		/* channel to channel linking is disabled ,
		 * destination offset is inc destination adr by 8,
		 * current iteration(citer) = number of transfers for frame
		 */
		priv->dma_tx_tcd[i]->tcd[5] = DMA_TCD5_CITER_DISABLE_LINK(iter);

		/* enable scater gather, interrupt on 1 Frame, */
		priv->dma_tx_tcd[i]->tcd[7] =
		    DMA_TCD7_BITER_DISABLE_LINK(iter) | DMA_TCD7_E_SG;
		priv->dma_tx_tcd[i]->tcd[6] = 0;
	}

	/* Next TCD for SG operation */
	physaddr = priv->dma_tx_tcd_paddr;
	priv->dma_tx_tcd[2]->tcd[6] =
	    ALIGN_SIZE(physaddr, ALIGNED_32_BYTES);
	physaddr += TCD_BUFFER_SIZE;
	priv->dma_tx_tcd[0]->tcd[6] =
	    ALIGN_SIZE(physaddr, ALIGNED_32_BYTES);
	physaddr += TCD_BUFFER_SIZE;
	priv->dma_tx_tcd[1]->tcd[6] =
	    ALIGN_SIZE(physaddr, ALIGNED_32_BYTES);
}

/* Initialize the Rx Transmit Control Discriptor parameters*/
static void rx_tcd_init(struct tdm_priv *priv)
{
	int i;
	u32 iter;
	u32 offset;
	dma_addr_t physaddr;
	int bytes_in_fifo_per_frame =
	    ALIGN_SIZE(priv->cfg.num_ch * priv->cfg.ch_width, 8);

	iter = bytes_in_fifo_per_frame / 8 * priv->cfg.num_frames;

	for (i = 0; i < NUM_OF_TDM_BUF; i++) {
		/* TDM RX fifo address */
		priv->dma_rx_tcd[i]->tcd[0] = TDM_RDR_OFFSET + priv->ptdm_base;

		/* ssize=dsize=64bit, soff=smod=dmod=0 */
		priv->dma_rx_tcd[i]->tcd[1] =
		    DMA_TCD1_SSIZE(SSIZE_64BITS) | DMA_TCD1_DSIZE(SSIZE_64BITS);

		/* number of bytes for minor loop, wide fifo 8bytes for dma */
		priv->dma_rx_tcd[i]->tcd[2] = 8;

		/* slast = 0 */
		priv->dma_rx_tcd[i]->tcd[3] = 0;

		offset = i * priv->cfg.num_frames * bytes_in_fifo_per_frame;

		/* dadr = rx buffer address */
		priv->dma_rx_tcd[i]->tcd[4] = priv->dma_input_paddr + offset;

		/* channel to channel linking is disabled ,
		 * destination offset is inc destination adr by 8,
		 * current iteration(citer) = number of transfers for frame
		 */
		priv->dma_rx_tcd[i]->tcd[5] =
		    DMA_TCD5_DOFF(0x08) | DMA_TCD5_CITER_DISABLE_LINK(iter);

		/* enable scater gather, interrupt on 1 Frame, */
		priv->dma_rx_tcd[i]->tcd[7] =
		    DMA_TCD7_BITER_DISABLE_LINK(iter) | DMA_TCD7_E_SG |
		    DMA_TCD7_INT_MAJ;
		priv->dma_rx_tcd[i]->tcd[6] = 0;
	}

	/* Next TCD for SG operation */
	physaddr = priv->dma_rx_tcd_paddr;
	priv->dma_rx_tcd[2]->tcd[6] =
	    ALIGN_SIZE(physaddr, ALIGNED_32_BYTES);
	physaddr += TCD_BUFFER_SIZE;
	priv->dma_rx_tcd[0]->tcd[6] =
	    ALIGN_SIZE(physaddr, ALIGNED_32_BYTES);
	physaddr += TCD_BUFFER_SIZE;
	priv->dma_rx_tcd[1]->tcd[6] =
	    ALIGN_SIZE(physaddr, ALIGNED_32_BYTES);
}

static irqreturn_t tdm_err_isr(int irq, void *p)
{
	int ret = IRQ_NONE;
	u32 status, mask, val;
	u32 dmac_err;
	struct tdm_priv *priv = p;
	u8 ch;

	/* transmit errors */
	status = in_be32(&priv->tdm_regs->ter);
	mask = in_be32(&priv->tdm_regs->tier);
	val = status & mask;
	out_be32(&priv->tdm_regs->ter, val);

	/* Transmit under Run error */
	if (val & TIER_TUEE)
		dev_err(priv->device, "TDM::Transmit Under Run error \n");

	/* Transmit Sync Error */
	if (val & TIER_TSEEE)
		dev_err(priv->device, "TDM::Transmit Sync error \n");

	if (val)
		ret = IRQ_HANDLED;

	/* receive errors */
	status = in_be32(&priv->tdm_regs->rer);
	mask = in_be32(&priv->tdm_regs->rier);
	val = status & mask;
	out_be32(&priv->tdm_regs->rer, val);

	/* Receiver Over run error */
	if (val & RIER_ROEE)
		dev_err(priv->device, "TDM::Receive  Over Run error \n");

	/* Receive Sync Error  */
	if (val & RIER_RSEEE)
		dev_err(priv->device, "TDM::Receive Sync error \n");

	if (val)
		ret = IRQ_HANDLED;

	/* Handling of DMA Errors */
	dmac_err = in_be32(&priv->dmac_regs->dmaes);
	if (!(dmac_err & DMAES_VLD))
		return ret;

	ch = DMAES_ERRCHN(dmac_err);

	if (dmac_err & DMAES_CPE)
		dev_err(priv->device, "TDM::Channel priority error\n");
	if (dmac_err & DMAES_GPE)
		dev_err(priv->device, "TDM::Group priority error\n");
	if (dmac_err & DMAES_SAE)
		dev_err(priv->device, "TDM::Source address error\n");
	if (dmac_err & DMAES_SOE)
		dev_err(priv->device, "TDM::Source offset error\n");
	if (dmac_err & DMAES_DAE)
		dev_err(priv->device, "TDM::Destination address error\n");
	if (dmac_err & DMAES_DOE)
		dev_err(priv->device, "TDM::Destination offset error\n");
	if (dmac_err & DMAES_NCE)
		dev_err(priv->device, "TDM::Nbytes citer error\n");
	if (dmac_err & DMAES_SGE)
		dev_err(priv->device, "TDM::Scatter gather error\n");
	if (dmac_err & DMAES_DBE)
		dev_err(priv->device, "TDM::Destination bus error \n");
	if (dmac_err & DMAES_SBE)
		dev_err(priv->device, "TDM::Source bus error \n");

	/* Clear the error */
	out_8(&priv->dmac_regs->dmacerr, ch);
	return IRQ_HANDLED;
}

static irqreturn_t dmac_done_isr(int irq, void *p)
{
	u32 ch;
	int ret = IRQ_NONE;
	struct tdm_priv *priv = p;

	ch = in_be32(&priv->dmac_regs->dmaintl);

	/* clear interrupt */
	if (ch & DMAC_RX_INT) {
		out_8(&priv->dmac_regs->dmacint, TDMRX_DMA_CH);
		ret = IRQ_HANDLED;
		/* track phases for Rx/Tx */
		priv->phase_rx += 1;
		if (priv->phase_rx == NUM_OF_TDM_BUF)
			priv->phase_rx = 0;
	}
	if (ch & DMAC_TX_INT) {
		out_8(&priv->dmac_regs->dmacint, TDMTX_DMA_CH);
		ret = IRQ_HANDLED;
	}

	if (ret == IRQ_HANDLED) {
		/* set the flag and wake up the thread */
		priv->adap->tdm_rx_flag = 1;

		/* schedule the tasklet */
		if (priv->adap->tasklet_conf)
			tasklet_schedule(&priv->adap->tdm_data_tasklet);
	}
	return ret;
}

static int init_tdm(struct tdm_priv *priv)
{
	u8 *buf;
	int i;
	int buf_size;
	dma_addr_t physaddr = 0;
	int ret = 0;

	/*
	   Allocate memory for Rx/Tx buffer according to active time slots
	   BufferSize = NUM_OF_TDM_BUF*NUM_OF_FRAMES*Active_CH
	 */
	buf_size = TDM_BUF_SIZE(priv->cfg.num_ch, priv->cfg.ch_width,
			 priv->cfg.num_frames);
	buf = dma_alloc_coherent(priv->device, buf_size, &physaddr, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto err_alloc_ip;
	}
	priv->dma_input_paddr = physaddr;
	priv->dma_input_vaddr = buf;
	priv->tdm_input_data = ALIGN_ADDRESS(buf, ALIGNED_8_BYTES);

	buf = dma_alloc_coherent(priv->device, buf_size, &physaddr, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto err_alloc_op;
	}
	priv->dma_output_paddr = physaddr;
	priv->dma_output_vaddr = buf;
	priv->tdm_output_data = ALIGN_ADDRESS(buf, ALIGNED_8_BYTES);

	/* allocate memory for TCD buffer discriptors */
	buf = dma_alloc_coherent(priv->device, NUM_OF_TDM_BUF * TCD_BUFFER_SIZE,
		&physaddr, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto err_alloc_rx;
	}

	memset(buf, 0, NUM_OF_TDM_BUF * TCD_BUFFER_SIZE);
	priv->dma_rx_tcd_paddr = physaddr;
	priv->dma_rx_tcd_vaddr = buf;
	for (i = 0; i < NUM_OF_TDM_BUF; i++) {
		priv->dma_rx_tcd[i] = ALIGN_ADDRESS(buf, ALIGNED_32_BYTES);
		buf += TCD_BUFFER_SIZE;
	}

	buf = dma_alloc_coherent(priv->device, 3 * TCD_BUFFER_SIZE, &physaddr,
			       GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto err_alloc_tx;
	}
	memset(buf, 0, NUM_OF_TDM_BUF * TCD_BUFFER_SIZE);
	priv->dma_tx_tcd_paddr = physaddr;
	priv->dma_tx_tcd_vaddr = buf;
	for (i = 0; i < NUM_OF_TDM_BUF; i++) {
		priv->dma_tx_tcd[i] = ALIGN_ADDRESS(buf, ALIGNED_32_BYTES);
		buf += TCD_BUFFER_SIZE;
	}

	priv->phase_rx = 0;
	priv->phase_tx = 0;
	return 0;

err_alloc_tx:
	dma_free_coherent(priv->device, NUM_OF_TDM_BUF * TCD_BUFFER_SIZE,
		priv->dma_rx_tcd_vaddr, priv->dma_rx_tcd_paddr);
err_alloc_rx:
	dma_free_coherent(priv->device, buf_size, priv->dma_output_vaddr,
			  priv->dma_output_paddr);
err_alloc_op:
	dma_free_coherent(priv->device, buf_size, priv->dma_input_vaddr,
			  priv->dma_input_paddr);
err_alloc_ip:
	return ret;
}

/* TDM register programming */
static int tdm_fsl_starlite_reg_init(struct tdm_priv *priv)
{
	int i;
	phys_addr_t base = get_immrbase();
	__be32 __iomem *pmuxcr;

	pmuxcr = ioremap(base + PMUXCR_OFFSET, 4);
	if (!pmuxcr)
		return -1;

	out_be32(pmuxcr, in_be32(pmuxcr) | PMUXCR_TDM_ENABLE);
	iounmap(pmuxcr);

	/* channel/group round robin */
	out_be32(&priv->dmac_regs->dmacr, DMACR_ERGA | DMACR_ERCA);
	/* Enable error Interrupts for TDM Rx &Tx */
	out_8(&priv->dmac_regs->dmaseei, TDMTX_DMA_CH);
	out_8(&priv->dmac_regs->dmaseei, TDMRX_DMA_CH);
	out_be32(&priv->dmac_regs->dmagpor, DMAGPOR_SNOOP);

	tx_tcd_init(priv);
	rx_tcd_init(priv);

	/* TDM RD->TD loopback, Share T/R Fsync,Clock */
	if (priv->cfg.loopback)
		out_be32(&priv->tdm_regs->gir, GIR_LPBK | GIR_RTS);
	else
		out_be32(&priv->tdm_regs->gir, GIR_RTS);

	/*
	   Rx Water mark 0,  FIFO enable,  Wide fifo, DMA enable for RX,
	   Receive Sync out, syncwidth = ch width, Rx clk out,zero sync,
	   falling edge , data order
	 */

	out_be32(&priv->tdm_regs->rir,
		 RIR_RFWM(0) | RIR_RFEN | RIR_RWEN | RIR_RDMA | RIR_RSL |
		 RIR_RSO | RIR_RCOE | RIR_RRDO | RIR_RFSD(0x01));
	out_be32(&priv->tdm_regs->tir,
		 TIR_TFWM(0) | TIR_TFEN | TIR_TWEN | TIR_TDMA | TIR_TSL |
		 TIR_TSO | TIR_TRDO | TIR_TFSD(0x01));

	/* no of channels ,Channel size-coading */
	out_be32(&priv->tdm_regs->rfp,
		 RFP_RNCF(priv->cfg.num_ch) | RFP_RCS(priv->cfg.ch_type));
	out_be32(&priv->tdm_regs->tfp,
		 TFP_TNCF(priv->cfg.num_ch) | TFP_TCS(priv->cfg.ch_type));

	out_be32(&priv->tdm_regs->rier, 0);
	out_be32(&priv->tdm_regs->tier, 0);

	/* clear all receive and transmit chs */
	for (i = 0; i < 4; i++) {
		out_be32(&priv->tdm_regs->tcma[i], 0);
		out_be32(&priv->tdm_regs->tcen[i], 0);
		out_be32(&priv->tdm_regs->rcen[i], 0);
	}

	return 0;

}

static void tdm_fsl_starlite_stop(struct tdm_priv *priv)
{
	/* stop the Tx & Rx */
	out_be32(&priv->tdm_regs->tcr, 0);
	out_be32(&priv->tdm_regs->rcr, 0);

	/* Clear DMA error Enable Request DMAEEIH/L */
	out_8(&priv->dmac_regs->dmaceei, TDMTX_DMA_CH);
	out_8(&priv->dmac_regs->dmaceei, TDMRX_DMA_CH);
	out_8(&priv->dmac_regs->dmacint, TDMRX_DMA_CH);
	out_8(&priv->dmac_regs->dmacint, TDMTX_DMA_CH);

	/* disable the dma request */
	out_8(&priv->dmac_regs->dmacerq, TDMRX_DMA_CH);
	out_8(&priv->dmac_regs->dmacerq, TDMTX_DMA_CH);
}

static int tdm_fsl_starlite_disable(struct tdm_adapter *adap)
{
	struct tdm_priv *priv = tdm_get_adapdata(adap);
	if (priv->tdm_active == 0) {
		dev_warn(priv->device, "already Disabled");
		return 0;
	}

	priv->tdm_active = 0;

	return 0;
}

static int tdm_fsl_starlite_enable(struct tdm_adapter *adap)
{
	int i;
	u32 ch_enab[4];
	unsigned long timeout;
	struct tdm_priv *priv = tdm_get_adapdata(adap);
	u8 ph = priv->phase_tx;

	if (priv->tdm_active == 1) {
		dev_warn(priv->device, "already Enabled");
		return 0;
	}

	/* enable the Channels required 0 to number of ch -1 */
	for (i = 0; i < 4; i++)
		ch_enab[i] = 0;

	for (i = 0; i < priv->cfg.num_ch; i++)
		ch_enab[i / 32] |= (1 << (i & 0x1F));

	for (i = 0; i < 4; i++) {
		out_be32(&priv->tdm_regs->rcen[i], ch_enab[i]);
		out_be32(&priv->tdm_regs->tcen[i], ch_enab[i]);
	}

	/* Clear the DONE bit */
	out_8(&priv->dmac_regs->dmacdne, TDMRX_DMA_CH);
	out_8(&priv->dmac_regs->dmacdne, TDMTX_DMA_CH);

	/* Load the Tx  transfer control descriptors */
	for (i = 0; i < DMA_MAX_TCD; i++)
		out_be32(&priv->dmac_regs->tcd[TDMTX_DMA_CH].tcd[i],
			 priv->dma_tx_tcd[ph]->tcd[i]);

	/* Load the Rx  transfer control descriptors */
	for (i = 0; i < DMA_MAX_TCD; i++)
		out_be32(&priv->dmac_regs->tcd[TDMRX_DMA_CH].tcd[i],
			 priv->dma_rx_tcd[ph]->tcd[i]);

	/* enable the dma request */
	out_8(&priv->dmac_regs->dmaserq, TDMRX_DMA_CH);
	out_8(&priv->dmac_regs->dmaserq, TDMTX_DMA_CH);

	/* Enable Receiver, transmitter */
	timeout = jiffies + TDM_ENABLE_TIMEOUT;
	out_be32(&priv->tdm_regs->tcr, TCR_TEN);
	while (!(in_be32(&priv->tdm_regs->tsr) & TSR_TENS)) {
		if (time_after(jiffies, timeout)) {
			dev_err(priv->device, "timeout to enable TDM Tx\n");
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

	timeout = jiffies + TDM_ENABLE_TIMEOUT;
	out_be32(&priv->tdm_regs->rcr, RCR_REN);
	while (!(in_be32(&priv->tdm_regs->rsr) & RSR_RENS)) {
		if (time_after(jiffies, timeout)) {
			dev_err(priv->device, "timeout to enable TDM Rx\n");
			return -ETIMEDOUT;
		}
		cpu_relax();

	}

	priv->tdm_active = 1;
	return 1;
}
static u32 tdm_fsl_starlite_read(struct tdm_adapter *adap,
		u16 **input_tdm_buffer)
{
	struct tdm_priv *priv = tdm_get_adapdata(adap);
	u8 phase_rx;
	u32 buf_addr, buf_size;
	/* point to where to start for the current phase data processing */
	int bytes_in_fifo_per_frame =
	    ALIGN_SIZE(priv->cfg.num_ch * priv->cfg.ch_width, 8);

	if (priv->tdm_active == 0) {
		dev_warn(priv->device, "TDM is not ready");
		return 0;
	}

	if (priv->phase_rx == 0)
		phase_rx = NUM_OF_TDM_BUF - 1;
	else
		phase_rx = priv->phase_rx - 1;

	buf_size = bytes_in_fifo_per_frame * priv->cfg.num_frames;
	buf_addr = buf_size * phase_rx;
	*input_tdm_buffer = (u16 *)(priv->tdm_input_data + buf_addr);

	return buf_size;
}

static u32 tdm_fsl_starlite_get_write_buf(struct tdm_adapter *adap,
		u16 **output_tdm_buffer)
{
	struct tdm_priv *priv = tdm_get_adapdata(adap);
	u32 tmp;
	u8 phase_tx;
	u32 buf_addr, buf_size;
	/* point to where to start for the current phase data processing */
	int bytes_in_fifo_per_frame =
	    ALIGN_SIZE(priv->cfg.num_ch * priv->cfg.ch_width, 8);

	if (priv->tdm_active == 0) {
		dev_warn(priv->device, "TDM is not ready");
		return 0;
	}

	tmp = in_be32(&priv->dmac_regs->tcd[TDMTX_DMA_CH].tcd[0]);

	tmp -= priv->dma_tx_tcd[0]->tcd[0];

	priv->phase_tx = tmp / (bytes_in_fifo_per_frame * priv->cfg.num_frames);

	if (priv->phase_tx == 0)
		phase_tx = NUM_OF_TDM_BUF - 1;
	else
		phase_tx = priv->phase_tx - 1;

	buf_size = bytes_in_fifo_per_frame * priv->cfg.num_frames;
	buf_addr = buf_size * phase_tx;
	*output_tdm_buffer = (u16 *)(priv->tdm_output_data + buf_addr);

	return buf_size;
}

static const struct tdm_algorithm tdm_algo = {
	.tdm_read = tdm_fsl_starlite_read,
	.tdm_get_write_buf = tdm_fsl_starlite_get_write_buf,
	.tdm_enable = tdm_fsl_starlite_enable,
	.tdm_disable = tdm_fsl_starlite_disable,
	.functionality = NULL,
};

static struct tdm_adapter tdm_fsl_starlite_ops = {
	.owner = THIS_MODULE,
	.name = "fsl_starlite",
	.algo = &tdm_algo,
};

static int __devinit tdm_fsl_starlite_probe(struct of_device *ofdev,
			      const struct of_device_id *match)
{
	int ret = 0;
	struct tdm_priv *priv;
	struct resource res;

	priv = kmalloc(sizeof(struct tdm_priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	dev_set_drvdata(&ofdev->dev, priv);
	priv->device = &ofdev->dev;

	ret = of_address_to_resource(ofdev->dev.of_node, 0, &res);
	if (ret) {
		ret = -EINVAL;
		goto err_resource;
	}
	priv->ptdm_base = res.start;

	priv->tdm_regs = of_iomap(ofdev->dev.of_node, 0);
	if (!priv->tdm_regs) {
		ret = -ENOMEM;
		goto err_tdmregs;
	}

	priv->dmac_regs = of_iomap(ofdev->dev.of_node, 1);
	if (!priv->dmac_regs) {
		ret = -ENOMEM;
		goto err_dmacreg;
	}

	/* tdmrd tmdtd at immrbar+0x16100 */
	priv->data_regs =
	    (struct tdm_data *)(TDM_DATAREG_OFFSET + (u8 *)priv->tdm_regs);
	/* TDMCLK_DIV_VAL_RX/TX at TDMBASE+0x180 */
	priv->clk_regs =
	    (struct tdm_clock *)(TDM_CLKREG_OFFSET + (u8 *)priv->tdm_regs);

	/* irqs mapping for tdm err/dmac err, dmac done */
	priv->tdm_err_intr = irq_of_parse_and_map(ofdev->dev.of_node, 0);
	if (priv->tdm_err_intr == NO_IRQ) {
		ret = -EINVAL;
		goto err_tdmerr_irqmap;
	}

	priv->dmac_done_intr = irq_of_parse_and_map(ofdev->dev.of_node, 1);
	if (priv->dmac_done_intr == NO_IRQ) {
		ret = -EINVAL;
		goto err_dmacdone_irqmap;
	}
	ret =
	    request_irq(priv->tdm_err_intr, tdm_err_isr, 0, "tdm_err_isr",
			priv);
	if (ret)
		goto err_tdmerrisr;

	ret =
	    request_irq(priv->dmac_done_intr, dmac_done_isr, 0, "dmac_done_isr",
			priv);
	if (ret)
		goto err_dmacdoneisr;

	priv->cfg.loopback = e_TDM_PROCESS_NORMAL;
	priv->cfg.num_ch = TDM_ACTIVE_CHANNELS;
	priv->cfg.ch_type = TDM_CHANNEL_TYPE;
	priv->cfg.ch_width = TDM_SLOT_WIDTH;
	priv->cfg.num_frames = NUM_OF_FRAMES;

	priv->adap = &tdm_fsl_starlite_ops;

	/* Wait q initilization */
	priv->adap->tdm_rx_flag = 0;
	/* todo - these should be configured by dts or init time */
	priv->adap->adap_mode = e_TDM_ADAPTER_MODE_NONE;
	priv->adap->tdm_mode = priv->cfg.loopback;

	priv->adap->max_num_ports = priv->cfg.num_ch;

	tdm_set_adapdata(priv->adap, priv);
	priv->adap->parent = &ofdev->dev;

	ret = 0;
	ret = tdm_add_adapter(priv->adap);
	if (ret < 0) {
		dev_err(priv->device, "failed to add adapter\n");
		goto fail_adapter;
	}

	ret = init_tdm(priv);
	if (ret)
		goto err_tdminit;

	ret = tdm_fsl_starlite_reg_init(priv);
	if (ret)
		goto err_tdminit;

	spin_lock_init(&priv->tdmlock);
	spin_lock(&priv->tdmlock);
	priv->tdm_active = 0;
	spin_unlock(&priv->tdmlock);

	if (tdmen) {
		ret = tdm_fsl_starlite_enable(priv->adap);
		if (!ret)
			goto err_tdminit;
	}

	return 0;

err_tdminit:
fail_adapter:
	free_irq(priv->dmac_done_intr, priv);
err_dmacdoneisr:
	free_irq(priv->tdm_err_intr, priv);
err_tdmerrisr:
	irq_dispose_mapping(priv->dmac_done_intr);
err_dmacdone_irqmap:
	irq_dispose_mapping(priv->tdm_err_intr);
err_tdmerr_irqmap:
	iounmap(priv->dmac_regs);
err_dmacreg:
	iounmap(priv->tdm_regs);
err_tdmregs:
err_resource:
	dev_set_drvdata(&ofdev->dev, NULL);
	kfree(priv);
err_alloc:
	return ret;
}

static int __devexit tdm_fsl_starlite_remove(struct of_device *ofdev)
{
	struct tdm_priv *priv = dev_get_drvdata(&ofdev->dev);
	int buf_size;

	tdm_fsl_starlite_disable(priv->adap);

	tdm_fsl_starlite_stop(priv);

	tdm_del_adapter(priv->adap);
	dev_set_drvdata(&ofdev->dev, NULL);

	/* free the irqs and dispose their mapping */
	free_irq(priv->tdm_err_intr, priv);
	free_irq(priv->dmac_done_intr, priv);
	irq_dispose_mapping(priv->tdm_err_intr);
	irq_dispose_mapping(priv->dmac_done_intr);
	iounmap(priv->tdm_regs);
	iounmap(priv->dmac_regs);

	/* free the buffers */
	buf_size =
	    TDM_BUF_SIZE(priv->cfg.num_ch, priv->cfg.ch_width,
			 priv->cfg.num_frames);
	dma_free_coherent(priv->device, buf_size, priv->dma_input_vaddr,
			  priv->dma_input_paddr);
	dma_free_coherent(priv->device, buf_size, priv->dma_output_vaddr,
			  priv->dma_output_paddr);

	/* free the TCDs */
	dma_free_coherent(priv->device, NUM_OF_TDM_BUF * TCD_BUFFER_SIZE,
		priv->dma_rx_tcd_vaddr,  priv->dma_rx_tcd_paddr);
	dma_free_coherent(priv->device, NUM_OF_TDM_BUF * TCD_BUFFER_SIZE,
		priv->dma_tx_tcd_vaddr,  priv->dma_tx_tcd_paddr);
	dev_set_drvdata(&ofdev->dev, NULL);
	kfree(priv);
	return 0;
}

static const struct of_device_id fsl_tdm_of_match[] = {
	{
	 .compatible = "fsl,starlite-tdm",
	 },
	{},
};

MODULE_DEVICE_TABLE(of, fsl_tdm_of_match);

static struct of_platform_driver tdm_fsl_starlite_driver = {
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= DRV_NAME,
		.of_match_table	= fsl_tdm_of_match,
	},
	.probe		= tdm_fsl_starlite_probe,
	.remove		= __devexit_p(tdm_fsl_starlite_remove),
};

static int __init tdm_fsl_starlite_init(void)
{
	int ret;
	pr_info(DRV_NAME ": " DRV_DESC ":Init\n");
	ret = of_register_platform_driver(&tdm_fsl_starlite_driver);
	if (ret)
		pr_err(DRV_NAME
			"of_register_platform_driver failed (%i)\n", ret);
	return ret;
}

static void __exit tdm_fsl_starlite_exit(void)
{
	pr_info(DRV_NAME ": " DRV_DESC ":Exit\n");
	of_unregister_platform_driver(&tdm_fsl_starlite_driver);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("P.V.Suresh, Freescale Semiconductor");
MODULE_DESCRIPTION("Driver For Freescale Starlite TDM controller");
MODULE_VERSION("1.1");

module_init(tdm_fsl_starlite_init);
module_exit(tdm_fsl_starlite_exit);

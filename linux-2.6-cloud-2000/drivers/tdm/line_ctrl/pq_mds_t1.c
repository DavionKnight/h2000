/*
 * drivers/tdm/line/pq_mds_t1.c
 *
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 * T1/E1 PHY(DS26528) Control Module for Freescale PQ-MDS-T1 card.
 *
 * Author: Kai Jiang <Kai.Jiang@freescale.com>
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
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/param.h>
#include <linux/delay.h>
#include <linux/of.h>


#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include <linux/io.h>

#include <linux/of_platform.h>
#include <linux/of_device.h>

#include "pq_mds_t1.h"
#define DRV_DESC "Freescale Developed PQ_MDS_T1CARD Driver"
#define DRV_NAME "PQ_MDS_T1"
#define DRV_VERSION "1.0"

static int pq_mds_t1_connect(struct pq_mds_t1 *t1_info)
{
	struct pld_mem *pld_base = t1_info->pld_base;

	if ((in_8(&pld_base->brdrev) == PQ_MDS_8E1T1_BRD_REV) &&
		(in_8(&pld_base->pldrev) == PQ_MDS_8E1T1_PLD_REV))
		return 0;
	else {
		printk(KERN_WARNING"No PQ-MDS-T1 Card Connected\n");
		return -ENODEV;
	}
}

static int pq_mds_t1_clock_set(struct pq_mds_t1 *pq_mds_t1_info)
{
	struct ds26528_mem *ds26528 = pq_mds_t1_info->ds26528_base;
	struct pld_mem *pld = pq_mds_t1_info->pld_base;

	if (pq_mds_t1_info->card_support == ZARLINK_LE71HR8820G) {
		/* General clock configuration
		   Altera register setting: Framer DIGIOEN and TXEN active */
		out_8(&pld->pinset, 0x03);
		/* Drive MCLK & REFCLKIO with 2.048MHz */
		out_8(&pld->csr, 0x41);
		/* Drive TSYSCLK & RSYSCLK with 2.048MHz */
		out_8(&pld->sysclk_tr, 0x41);
		out_8(&pld->synctss, 0x00);
		/* Drive TCLK1..4 with 2.048MHz*/
		out_8(&pld->tcsr1, 0x55);
		/* Drive TCLK5..8 with 2.048MHz*/
		out_8(&pld->tcsr2, 0x55);
		out_8(&pld->tsyncs1, 0x00);
		out_8(&pld->ds3set, 0x00);
		/* Connect tdm port A to LM card */
		out_8(&pld->gcr, 0x42);
		/* Framer setting
		    Select MCLK - 2.048MHz, REFCLKIO - 2.048MHz (GTCCR) */
		out_8(&ds26528->link[0].gbl.gtccr, 0xa0);
		/* Wait 1msec */
		msleep(1);

		/* Set TSSYNCIO -> OUTPUT (GTCR2) */
		out_8(&ds26528->link[0].gbl.gtcr2, 0x02);
		/* Select BPCLK=2.048MHz (GFCR) */
		out_8(&ds26528->link[0].gbl.gfcr, 0x00);
	} else {
		if (pq_mds_t1_info->line_rate == LINE_RATE_T1) {
			/* General clock configuration
			   Altera register setting:
			   Framer DIGIOEN and TXEN active */
			out_8(&pld->pinset, 0x03);
			/* Drive MCLK & REFCLKIO with 1.544MHz */
			out_8(&pld->csr, 0x00);
			/* Drive TSYSCLK & RSYSCLK with 1.544MHz */
			out_8(&pld->sysclk_tr, 0x00);

			/* Framer setting
			   Select MCLK - 1.544MHz,
			   REFCLKIO - 1.544MHz (GTCCR) */
			out_8(&ds26528->link[0].gbl.gtccr, 0xac);
			/* Wait 1msec */
			msleep(1);

			/* Set TSSYNCIO -> OUTPUT (GTCR2) */
			out_8(&ds26528->link[0].gbl.gtcr2, 0x02);
			/* Select BPCLK=2.048MHz (GFCR) */
			out_8(&ds26528->link[0].gbl.gfcr, 0x00);
		} else {
			/* General clock configuration
			    Altera register setting:
			    Framer DIGIOEN and TXEN active */
			out_8(&pld->pinset, 0x03);
			/* Drive MCLK & REFCLKIO with 2.048MHz */
			out_8(&pld->csr, 0x41);
			/* Drive TSYSCLK & RSYSCLK with 2.048MHz */
			out_8(&pld->sysclk_tr, 0x41);

			/* Framer setting
			    Select MCLK - 2.048MHz,
			    REFCLKIO - 2.048MHz (GTCCR) */
			out_8(&ds26528->link[0].gbl.gtccr, 0xa0);
			/* Wait 1msec */
			msleep(1);

			/* Set TSSYNCIO -> OUTPUT (GTCR2) */
			out_8(&ds26528->link[0].gbl.gtcr2, 0x02);
			/* Select BPCLK=2.048MHz (GFCR) */
			out_8(&ds26528->link[0].gbl.gfcr, 0x00);
		}
	}

	return 0;
}

static int ds26528_bulk_write_set(struct pq_mds_t1 *pq_mds_t1_info)
{
	/* Select Bulk write for Framer Register Init (GTCR1) */
	out_8(&(pq_mds_t1_info->ds26528_base->link[0].gbl.gtcr1), 0x04);
	return 0;
}

static int ds26528_bulk_write_unset(struct pq_mds_t1 *pq_mds_t1_info)
{
	/* Unselect Bulk write for Framer Register Init (GTCR1) */
	out_8(&(pq_mds_t1_info->ds26528_base->link[0].gbl.gtcr1), 0x00);
	return 0;
}

static int ds26528_gbl_sreset(struct pq_mds_t1 *pq_mds_t1_info)
{
	struct ds26528_mem *ds26528 = pq_mds_t1_info->ds26528_base;

	/* Global LIU Software Reset Register (GLSRR) */
	out_8(&ds26528->link[0].gbl.glsrr, 0xff);
	/* Global Framer and BERT Software Reset Register (GFSRR) */
	out_8(&ds26528->link[0].gbl.gfsrr, 0xff);
	/* Wait 10msec*/
	msleep(10);

	out_8(&ds26528->link[0].gbl.glsrr, 0x00);
	out_8(&ds26528->link[0].gbl.gfsrr, 0x00);

	return 0;
}

static int card_pld_t1_clk_set(struct pq_mds_t1 *pq_mds_t1_info)
{
	struct pld_mem *pld = pq_mds_t1_info->pld_base;
	/* Drive TCLK1..4 with 1.544MHz*/
	out_8(&pld->tcsr1, 0x00);
	/* Drive TCLK5..8 with 1.544MHz*/
	out_8(&pld->tcsr2, 0x00);
	msleep(1);

	return 0;
}

static int card_pld_e1_clk_set(struct pq_mds_t1 *pq_mds_t1_info)
{
	struct pld_mem *pld = pq_mds_t1_info->pld_base;
	/* Drive TCLK1..4 with 2.048MHz*/
	out_8(&pld->tcsr1, 0x55);
	/* Drive TCLK5..8 with 2.048MHz*/
	out_8(&pld->tcsr2, 0x55);
	msleep(1);

	return 0;
}

static int ds26528_rx_tx_sreset(struct pq_mds_t1 *pq_mds_t1_info, u8 phy_id)
{
	struct ds26528_mem *ds26528 = pq_mds_t1_info->ds26528_base;

	if (phy_id > MAX_NUM_OF_CHANNELS)
		return -ENODEV;

	/* Perform RX/TX SRESET,Reset receiver (RMMR) */
	out_8(&ds26528->link[phy_id].rx.rmmr, 0x02);
	/* Reset tranceiver (TMMR) */
	out_8(&ds26528->link[phy_id].tx.tmmr, 0x02);
	/* Wait 10msec */
	msleep(10);

	return 0;
}

static int ds26528_frame_regs_clear(struct pq_mds_t1 *pq_mds_t1_info, u8 phy_id)
{
	struct ds26528_mem *ds26528 = pq_mds_t1_info->ds26528_base;
	/* Zero all Framer Registers */
	memset(&ds26528->link[phy_id].rx, 0, sizeof(struct rx_frame));
	memset(&ds26528->link[phy_id].tx, 0, sizeof(struct tx_frame));
	memset(&ds26528->liu[phy_id], 0, sizeof(struct liu_reg));
	memset(&ds26528->bert[phy_id], 0, sizeof(struct bert_reg));

	return 0;
}

static int ds26528_trans_mode_set(struct pq_mds_t1 *pq_mds_t1_info, u8 phy_id)
{
	struct ds26528_mem *ds26528 = pq_mds_t1_info->ds26528_base;

	switch (pq_mds_t1_info->trans_mode) {
	case FRAMER_LB:
		out_8(&ds26528->link[phy_id].rx.rcr3, 0x01);
		break;
	case NORMAL:
		break;
	default:
		printk(KERN_WARNING
			"function: %s No such transfer mode\n", __func__);
		break;
	}

	return 0;

}

static int ds26528_t1_spec_config(struct pq_mds_t1 *pq_mds_t1_info, u8 phy_id)
{
	struct ds26528_mem *ds26528 = pq_mds_t1_info->ds26528_base;
	/* Receive T1 Mode (Receive Master Mode Register - RMMR)
	   Framer Disabled */
	out_8(&ds26528->link[phy_id].rx.rmmr, 0x00);
	/* Transmit T1 Mode (Transmit Master Mode Register - TMMR)
	   Framer Disabled */
	out_8(&ds26528->link[phy_id].tx.tmmr, 0x00);
	/* Receive T1 Mode Framer Enable (RMMR - Framer Enabled/T1) */
	out_8(&ds26528->link[phy_id].rx.rmmr, 0x80);
	/* Transmit T1 Mode Framer Enable (TMMR - Framer Enabled/T1) */
	out_8(&ds26528->link[phy_id].tx.tmmr, 0x80);
	/* RCR1, receive T1 B8zs & ESF (Receive Control Register 1 - T1 MODE) */
	out_8(&ds26528->link[phy_id].rx.rcr1, 0xc8);
	/* RIOCR  (RSYSCLK=1.544MHz, RSYNC-Output) */
	out_8(&ds26528->link[phy_id].rx.riocr, 0x00);
	/* TCR1 Transmit T1 b8zs*/
	out_8(&ds26528->link[phy_id].tx.tcr[0], 0x04);
	/* TIOCR (TSYSCLK=1.544MHz, TSYNC-Output) */
	out_8(&ds26528->link[phy_id].tx.tiocr, 0x04);
	/* Receive T1 Mode Framer Enable & init Done */
	out_8(&ds26528->link[phy_id].rx.rmmr, 0xc0);
	/* Transmit T1 Mode Framer Enable & init Done */
	out_8(&ds26528->link[phy_id].tx.tmmr, 0xc0);
	/* Configure LIU (LIU Transmit Receive Control Register
	   - LTRCR. T1 mode) */
	out_8(&ds26528->liu[phy_id].ltrcr, 0x02);
	/* T1 Mode default 100 ohm 0db CSU (LIU Transmit Impedance and
	   Pulse Shape Selection Register - LTITSR)*/
	out_8(&ds26528->liu[phy_id].ltitsr, 0x10);
	/* T1 Mode default 100 ohm Long haul (LIU Receive Impedance and
	   Sensitivity Monitor Register - LRISMR)*/
	out_8(&ds26528->liu[phy_id].lrismr, 0x13);
	/* Enable Transmit output (LIU Maintenance Control Register - LMCR) */
	out_8(&ds26528->liu[phy_id].lmcr, 0x01);
	return 0;
}
static int ds26528_e1_spec_config(struct pq_mds_t1 *pq_mds_t1_info, u8 phy_id)
{
	struct ds26528_mem *ds26528 = pq_mds_t1_info->ds26528_base;

	/* Receive E1 Mode (Receive Master Mode Register - RMMR)
	   Framer Disabled */
	out_8(&ds26528->link[phy_id].rx.rmmr, 0x01);
	/* Transmit E1 Mode (Transmit Master Mode Register - TMMR)
	   Framer Disabled*/
	out_8(&ds26528->link[phy_id].tx.tmmr, 0x01);
	/* Receive E1 Mode Framer Enable (RMMR - Framer Enabled/E1)*/
	out_8(&ds26528->link[phy_id].rx.rmmr, 0x81);
	/* Transmit E1 Mode Framer Enable (TMMR - Framer Enabled/E1)*/
	out_8(&ds26528->link[phy_id].tx.tmmr, 0x81);
	/* RCR1, receive E1 B8zs & ESF (Receive Control Register 1 - E1 MODE)*/
	out_8(&ds26528->link[phy_id].rx.rcr1, 0x60);
	/* RIOCR (RSYSCLK=2.048MHz, RSYNC-Output) */
	out_8(&ds26528->link[phy_id].rx.riocr, 0x10);
	/* TCR1 Transmit E1 b8zs */
	out_8(&ds26528->link[phy_id].tx.tcr[0], 0x04);
	/* TIOCR (TSYSCLK=2.048MHz, TSYNC-Output) */
	out_8(&ds26528->link[phy_id].tx.tiocr, 0x14);
	/* Set E1TAF (Transmit Align Frame Register regsiter) */
	out_8(&ds26528->link[phy_id].tx.e1taf, 0x1b);
	/* Set E1TNAF register (Transmit Non-Align Frame Register) */
	out_8(&ds26528->link[phy_id].tx.e1tnaf, 0x40);
	/* Receive E1 Mode Framer Enable & init Done (RMMR) */
	out_8(&ds26528->link[phy_id].rx.rmmr, 0xc1);
	/* Transmit E1 Mode Framer Enable & init Done (TMMR) */
	out_8(&ds26528->link[phy_id].tx.tmmr, 0xc1);
	/* Configure LIU (LIU Transmit Receive Control Register
	   - LTRCR. E1 mode) */
	out_8(&ds26528->liu[phy_id].ltrcr, 0x00);
	/* E1 Mode default 75 ohm w/Transmit Impedance Matlinking
	 (LIU Transmit Impedance and Pulse Shape Selection Register - LTITSR)*/
	out_8(&ds26528->liu[phy_id].ltitsr, 0x00);
	/* E1 Mode default 75 ohm Long Haul w/Receive Impedance Matlinking
	  (LIU Receive Impedance and Sensitivity Monitor Register - LRISMR)*/
	out_8(&ds26528->liu[phy_id].lrismr, 0x03);
	/* Enable Transmit output (LIU Maintenance Control Register - LMCR) */
	out_8(&ds26528->liu[phy_id].lmcr, 0x01);

	return 0;
}

int ds26528_t1_e1_config(struct pq_mds_t1 *pq_mds_t1_info, u8 phy_id)
{
	struct pq_mds_t1 *card_info = pq_mds_t1_info;

	ds26528_bulk_write_set(card_info);
	ds26528_gbl_sreset(card_info);
	switch (card_info->line_rate) {
	case LINE_RATE_T1:
		card_pld_t1_clk_set(card_info);
		break;
	case LINE_RATE_E1:
		card_pld_e1_clk_set(card_info);
		break;
	default:
		printk(KERN_WARNING"Support E1/T1. Frame mode not support.\n");
		return -ENODEV;
		break;
	}
	ds26528_rx_tx_sreset(card_info, phy_id);
	ds26528_frame_regs_clear(card_info, phy_id);
	ds26528_trans_mode_set(card_info, phy_id);

	switch (card_info->line_rate) {
	case LINE_RATE_T1:
		ds26528_t1_spec_config(card_info, phy_id);
		break;
	case LINE_RATE_E1:
		ds26528_e1_spec_config(card_info, phy_id);
		break;
	default:
		printk(KERN_WARNING"Support E1/T1. Frame mode not support\n");
		return -ENODEV;
		break;
	}
	ds26528_bulk_write_unset(card_info);

	return 0;
}

static enum line_rate_t set_phy_line_rate(const char *line_rate)
{
	if (strcasecmp(line_rate, "t1") == 0)
		return LINE_RATE_T1;
	else if (strcasecmp(line_rate, "e1") == 0)
		return LINE_RATE_E1;
	else
		return LINE_RATE_T1;
}

static enum tdm_trans_mode_t set_phy_trans_mode(const char *trans_mode)
{
	if (strcasecmp(trans_mode, "normal") == 0)
		return NORMAL;
	else if (strcasecmp(trans_mode, "framer-loopback") == 0)
		return FRAMER_LB;
	else
		return NORMAL;
}

static enum card_support_type set_card_support_type(const char *card_type)
{
	if (strcasecmp(card_type, "zarlink,le71hr8820g") == 0)
		return ZARLINK_LE71HR8820G;
	else if (strcasecmp(card_type, "dallas,ds26528") == 0)
		return DS26528_CARD;
	else
		return DS26528_CARD;
}

static int __devinit pq_mds_t1_probe(struct of_device *ofdev,
				const struct of_device_id *match)
{
	int err = 0;
	struct device_node *np;
	struct resource res;
	struct pq_mds_t1 *t1_info = NULL;
	unsigned char *prop = NULL;

	t1_info = kzalloc(sizeof(struct pq_mds_t1), GFP_KERNEL);
	if (!t1_info) {
		printk(KERN_ERR"%s: No memory to alloc\n", __func__);
		return -ENOMEM;
	}
	dev_set_drvdata(&ofdev->dev, t1_info);

	np = of_find_compatible_node(NULL, NULL, "fsl,pq-mds-t1-pld");
	if (!np)  {
		printk(KERN_ERR
			"%s: Invalid fsl,pq-mds-t1-pld property\n", __func__);
		err =  -ENODEV;
		goto err_miss_pld_property;
	}

	err = of_address_to_resource(np, 0, &res);
	if (err) {
		err = -ENODEV;
		goto err_miss_pld_property;
	}
	t1_info->pld_base = ioremap(res.start, res.end - res.start + 1);

	prop = (unsigned char *)of_get_property(np, "fsl,card-support", NULL);
	if (!prop) {
		err = -ENODEV;
		printk(KERN_ERR"%s: Invalid card-support property\n", __func__);
		goto err_card_support;
	}
	t1_info->card_support = set_card_support_type(prop);
	of_node_put(np);

	err = pq_mds_t1_connect(t1_info);
	if (err) {
		err = -ENODEV;
		printk(KERN_ERR"%s: No PQ_MDS_T1 CARD\n", __func__);
		goto err_card_support;
	}

	np = of_find_compatible_node(NULL, NULL, "dallas,ds26528");
	if (!np) {
		printk(KERN_ERR
			"%s: Invalid dallas,ds26528 property\n", __func__);
		err = -ENODEV;
		goto err_card_support;

	}
	err = of_address_to_resource(np, 0, &res);
	if (err) {
		err = -ENODEV;
		goto err_card_support;
	}
	t1_info->ds26528_base = ioremap(res.start, res.end - res.start + 1);

	prop = (unsigned char *)of_get_property(np, "line-rate", NULL);
	if (!prop) {
		err = -ENODEV;
		printk(KERN_ERR"%s: Invalid line-rate property\n", __func__);
		goto err_line_rate;
	}

	t1_info->line_rate = set_phy_line_rate(prop);

	prop = (unsigned char *)of_get_property(np, "trans-mode", NULL);
	if (!prop) {
		err = -ENODEV;
		printk(KERN_ERR"%s: Invalid trans-mode property\n", __func__);
		goto err_line_rate;
	}
	t1_info->trans_mode = set_phy_trans_mode(prop);
	of_node_put(np);

	pq_mds_t1_clock_set(t1_info);
	if (t1_info->card_support == DS26528_CARD)
		ds26528_t1_e1_config(t1_info, 0);

	return 0;

err_line_rate:
	iounmap(t1_info->ds26528_base);
err_card_support:
	iounmap(t1_info->pld_base);
err_miss_pld_property:
	kfree(t1_info);

	return err;
}

static int pq_mds_t1_remove(struct of_device *ofdev)
{
	struct pq_mds_t1 *t1_info = dev_get_drvdata(&ofdev->dev);

	iounmap(t1_info->pld_base);
	iounmap(t1_info->ds26528_base);
	kfree(t1_info);

	return 0;
}

static struct of_device_id pq_mds_t1_of_match[] = {
	{
	.compatible = "fsl,pq-mds-t1",
	},
	{},
};

static struct of_platform_driver pq_mds_t1_driver = {
	.probe = pq_mds_t1_probe,
	.remove = __devexit_p(pq_mds_t1_remove),
	.driver = {
		.name = "pq_mds_t1",
		.owner = THIS_MODULE,
		.of_match_table = pq_mds_t1_of_match,
	},
};



static int __init pq_mds_t1_init(void)
{
	int ret;

	printk(KERN_INFO "PQ-MDS-T1: " DRV_DESC "\n");

	ret = of_register_platform_driver(&pq_mds_t1_driver);
	if (ret)
		printk(KERN_WARNING "PQ_MDS_T1 Card failed to register\n");

	return ret;
}


static void __exit pq_mds_t1_exit(void)
{
	of_unregister_platform_driver(&pq_mds_t1_driver);
}


MODULE_AUTHOR("Freescale Semiconductor, Inc");
MODULE_DESCRIPTION(DRV_DESC);
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");


module_init(pq_mds_t1_init);
module_exit(pq_mds_t1_exit);

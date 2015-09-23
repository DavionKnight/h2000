/*
 * drivers/tdm/line/slic_zarlink.c
 *
 * Copyright (C) 2008-2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 * SLIC Line Control Module for Zarlink SLICs.
 * This  is a slic control and initialization module.
 *
 * Author:Poonam Aggrwal<poonam.aggrwal@freescale.com>
 * Hemant Agrawal <hemant@freescale.com>
 * Rajesh Gumasta <rajesh.gumasta@freescale.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This driver was created solely by Freescale, without the assistance,
 * support or intellectual property of Zarlink Semiconductor.  No maintenance
 * or support will be provided by Zarlink Semiconductor regarding this driver
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

 /* Note that this is a complete rewrite of Poonam's slic code.
    But we have used so much of her original code and ideas that it seems
    only fair to recognize her as co-author -- Rajesh & Hemant */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/spi/spi.h>
#include <linux/wait.h>
#include <linux/param.h>
#include <linux/delay.h>
#include <linux/of.h>
#include "slic_zarlink.h"

#define DRV_DESC "FREESCALE DEVELOPED ZARLINK SLIC DRIVER"
#define DRV_NAME "legerity"

#define MAX_NUM_OF_SLICS 10

#define TESTING_PRODUCT_CODE

static struct spi_device *g_spi;
struct spi_transfer t;

struct slic_channel {
	unsigned int ch1_rx_slot, ch1_tx_slot, ch2_rx_slot, ch2_tx_slot;
};
struct slic_channel slic_ch[MAX_NUM_OF_SLICS];
static int num_slics;
static enum {
	ZARLINK_LE88266,
	ZARLINK_LE88506
} slic_chip_type;

/** \brief      Driver's license
 ** \details    GPL
 ** \ingroup    Linux_module
 **/
MODULE_LICENSE("GPL");
/** \brief      Module author
 ** \ingroup    Linux_module
 **/
MODULE_AUTHOR("Poonam Aggrwal<poonam.aggrwal@freescale.com>");
/** \brief      Module description
 ** \ingroup    Linux_module
 **/
MODULE_DESCRIPTION(DRV_DESC);

static char spi_transaction(char *temp_tx, char *temp_rx)
{
	struct spi_message m;

	spi_message_init(&m);
	t.tx_buf = temp_tx;
	t.rx_buf = temp_rx;
	t.len = 1;
	spi_message_add_tail(&t, &m);
	if (spi_sync(g_spi, &m) < 0)
		printk(KERN_ERR "error in spi operation\n");
	return 0;
}

static void
slic_cmd(unsigned char deviceId, unsigned char channel, unsigned char cmd,
		unsigned char len, unsigned char *cmdData)
{
	/* Port Enable Command */
	unsigned char ecCmd = WRITE_CHANNEL_ENABLE;
	unsigned char cmdLen;
	unsigned char temp;

/* Write EC command */
	temp = 0x00;
	spi_transaction(&ecCmd, &temp);

/* write EC value */
	temp = 0x00;
	spi_transaction(&channel, &temp);

/* write command */
	temp = 0x00;
	spi_transaction(&cmd, &temp);

/* If read command or write command */
	if (cmd & 0x01) {
		unsigned char temp = 0x00;
		for (cmdLen = 0; cmdLen < len; cmdLen++)
			spi_transaction(&temp, &cmdData[cmdLen]);
		}
	else {
		temp = 0x00;
		for (cmdLen = 0; cmdLen < len; cmdLen++)
			spi_transaction(&cmdData[cmdLen], &temp);
	}
}

static void get_slic_product_code()
{
	unsigned char temp_tx, temp_rx;

	/* Read Product Code */
	temp_tx = READ_PRODUCT_CODE;
	temp_rx = 0x00;
	spi_transaction(&temp_tx, &temp_rx);
	printk(KERN_INFO "The data read 1 is %x\n", temp_rx);

	temp_tx = WRITE_CHANNEL_ENABLE;
	spi_transaction(&temp_tx, &temp_rx);
	printk(KERN_INFO "The data read 2 is %x\n", temp_rx);
	temp_tx = 0x00;
	spi_transaction(&temp_tx, &temp_rx);
	printk(KERN_INFO "The product code read is %x\n", temp_rx);

	/* Reading the device configuration register */
	temp_tx = READ_DEVICE_CONFIGURATION;
	spi_transaction(&temp_tx, &temp_rx);
	printk(KERN_INFO "The device configuration reg 1 is  %x\n", temp_rx);

	temp_tx = 0x00;
	spi_transaction(&temp_tx, &temp_rx);
	printk(KERN_INFO "The device configuration reg 2 is  %x\n", temp_rx);
}

static int slic_init_configure(unsigned char device_handle,
		struct spi_device *spi, int slic_id)
{
	char temp1 = 0;
	char temp2[2];
	char temp3[3];
	unsigned char cad[4];
	unsigned char len;
	unsigned int channel_id;

	temp3[0] = 0x04;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, 0x04, len, &temp3[0]);

	temp3[0] = 0x0;
	temp3[1] = 0x0;
	len = 0x02;
	slic_cmd(device_handle, CHANNEL1, READ_PRODUCT_CODE, len, &temp3[0]);
	if ((temp3[0] == 0x04) && (temp3[1] == 0xb3))
		slic_chip_type = ZARLINK_LE88266;
	else if ((temp3[0] == 0x04) && (temp3[1] == 0xbc))
		slic_chip_type = ZARLINK_LE88506;

#ifdef TESTING_PRODUCT_CODE
	get_slic_product_code();
#endif
	temp3[0] = 0x82;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_DEVICE_CFG, len, &temp3[0]);

	temp3[0] = 0x7f;
	temp3[1] = 0xff;
	len = 0x02;
	slic_cmd(device_handle, CHANNEL1, WRITE_INT_MASK, len, &temp3[0]);

	temp3[0] = 0xff;
	temp3[1] = 0xff;
	len = 0x02;
	slic_cmd(device_handle, CHANNEL1, WRITE_INT_MASK, len, &temp3[0]);

	temp3[0] = 0x40;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_TXRXCLKSLOT_TXCLKEDGE, len,
			&temp3[0]);

	temp3[0] = 0x0;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_SYSTEM_STATE, len, &temp3[0]);

	temp3[0] = 0x0;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL2, WRITE_SYSTEM_STATE, len, &temp3[0]);

	/* Put the Switching regulators in disabled mode */
	temp3[0] = 0x0;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_SWITCH_REGULATOR_CTRL, len,
			&temp3[0]);

	temp3[0] = 0x0;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_SWITCH_REGULATOR_CTRL, len,
			&temp3[0]);

	temp3[0] = 0x3;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_SYSTEM_STATE_CFG, len,
			&temp3[0]);

	temp3[0] = 0x0;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_SYSTEM_STATE, len, &temp3[0]);

	temp3[0] = 0x3;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL2, WRITE_SYSTEM_STATE_CFG, len,
			&temp3[0]);

	temp3[0] = 0x0;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL2, WRITE_SYSTEM_STATE, len, &temp3[0]);

	if (slic_chip_type == ZARLINK_LE88266)
		temp3[0] = 0x2b;
	else if (slic_chip_type == ZARLINK_LE88506)
		temp3[0] = 0x23;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_SYSTEM_STATE, len, &temp3[0]);

	temp3[0] = 0x80;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_OPERATING_FUNCTION, len,
			&temp3[0]);

	temp3[0] = 0xe0;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_OPERATING_CONDITIONS, len,
			&temp3[0]);

	temp3[0] = 0x1;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_CONVERTER_CFG, len, &temp3[0]);

	if (slic_chip_type == ZARLINK_LE88266) {
		/* Set Switching Paramenters as for Le88266
		 * 1. BSI[1:0] = 00b (sense pin VBL is SWVSY, VBH is SWVSZ)
		 * 2. SWFS[1:0] = 00b (setting frequency as 384kHz in
		 *    high power mode)
		 * 3. SWYV[4:0] = 00101b (setting to -25V)
		 * 4. SWZV[4:0] = 00000b (setting to 0V)
		 */
		temp3[0] = 0x00;
		temp3[1] = 0x05;
		temp3[2] = 0x00;
	} else if (slic_chip_type == ZARLINK_LE88506) {
		temp3[0] = 0x00;
		temp3[1] = 0x84;
		temp3[2] = 0x80;
	}
	len = 0x03;
	slic_cmd(device_handle, CHANNEL1, WRITE_SWITCH_REGULATOR_PARAMS, len,
			&temp3[0]);

	/* Put the Switching regulators in
	 * 1. Regulator Y & Z in low power state
	 * 2. Over voltage protection enabled
	 */
	temp3[0] = 0x15;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_SWITCH_REGULATOR_CTRL, len,
			&temp3[0]);

	/* Wait 20ms before switching from low power to high power */
	mdelay(20);

	temp3[0] = 0x9;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_CONVERTER_CFG, len, &temp3[0]);

	temp3[0] = 0xb;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_CONVERTER_CFG, len, &temp3[0]);

	temp3[0] = 0xb;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_CONVERTER_CFG, len, &temp3[0]);

	temp3[0] = 0x1;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_CONVERTER_CFG, len, &temp3[0]);

	temp3[0] = 0x1;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL2, WRITE_CONVERTER_CFG, len, &temp3[0]);

	temp3[0] = 0x2;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_CONVERTER_CFG, len, &temp3[0]);

	temp3[0] = 0x2;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_CONVERTER_CFG, len, &temp3[0]);

	if (slic_chip_type == ZARLINK_LE88266) {
		temp3[0] = 0x3;
		len = 0x01;
		slic_cmd(device_handle, CHANNEL1, WRITE_CONVERTER_CFG, len,
				&temp3[0]);
		temp3[0] = 0x3;
		len = 0x01;
		slic_cmd(device_handle, CHANNEL1, WRITE_CONVERTER_CFG, len,
				&temp3[0]);
	}
	if (slic_chip_type == ZARLINK_LE88266) {
		/* Set Switching Paramenters as for Le88266
		 * 1. BSI[1:0] = 00b (sense pin VBL is SWVSY, VBH is SWVSZ)
		 * 2. SWFS[1:0] = 00b (setting frequency as 384kHz in high
		 *    power mode)
		 * 3. SWYV[4:0] = 00101b (setting to -25V)
		 * 4. SWZV[4:0] = 00000b (setting to 0V)
		 */
		temp3[0] = 0x00;
		temp3[1] = 0x05;
		temp3[2] = 0x00;
	} else if (slic_chip_type == ZARLINK_LE88506) {
		temp3[0] = 0x00;
		temp3[1] = 0x84;
		temp3[2] = 0x80;
	}
	len = 0x03;
	slic_cmd(device_handle, CHANNEL1, WRITE_SWITCH_REGULATOR_PARAMS, len,
			&temp3[0]);

	/* Put the Switching regulators in
	 * 1. Regulator Y & Z in high power state
	 * 2. Over voltage protection enabled
	 */
	temp3[0] = 0x1f;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_SWITCH_REGULATOR_CTRL, len,
			&temp3[0]);

	/* Setting the channel specific parameters */
	for (channel_id = CHANNEL1; channel_id <= CHANNEL2; channel_id++) {

		/* Set the IO direction to Output - to energise the fxo
		 * relay */
		temp3[0] = 0x1;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_IO_DIRECTION, len,
				&temp3[0]);

		temp3[0] = 0x0;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_IO_DATA, len,
				&temp3[0]);

		temp3[0] = 0x0;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_OPERATING_CONDITIONS,
				len, &temp3[0]);

		len = sizeof(dataset_cadenceTimer) / sizeof(unsigned char);
		slic_cmd(device_handle, channel_id, WRITE_CADENCE_TIMER, len,
				&dataset_cadenceTimer[0]);

		temp3[0] = 0x2;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_SYSTEM_STATE_CFG,
				len, &temp3[0]);

		temp3[0] = 0xc0;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_OPERATING_CONDITIONS,
				len, &temp3[0]);

		if (slic_chip_type == ZARLINK_LE88266) {
			temp3[0] = 0x0;
			temp3[1] = 0x2;
		} else if (slic_chip_type == ZARLINK_LE88506) {
			temp3[0] = 0x0;
			temp3[1] = 0x0;
		}
		len = 0x02;
		slic_cmd(device_handle, channel_id, WRITE_DC_CALIBRATION, len,
			       &temp3[0]);

		temp3[0] = 0x0;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_SYSTEM_STATE, len,
				&temp3[0]);

		len = sizeof(dataset_writeLoopParams) / sizeof(unsigned char);
		slic_cmd(device_handle, channel_id,
				WRITE_LOOP_SUPERVISION_PARAMS, len,
				&dataset_writeLoopParams[0]);

		temp3[0] = 0x13;
		temp3[1] = 0x8;
		len = 0x02;
		slic_cmd(device_handle, channel_id, WRITE_DC_FEED_PARAMS, len,
				&temp3[0]);

		len = sizeof(dataset1_for_nooperation) / sizeof(unsigned char);
		slic_cmd(device_handle, channel_id, WRITE_NO_OPERATION, len,
				&dataset1_for_nooperation[0]);

		temp3[0] = 0x3f;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_OPERATING_FUNCTION,
				len, &temp3[0]);

		len = sizeof(dataset2_for_nooperation) / sizeof(unsigned char);
		slic_cmd(device_handle, channel_id, WRITE_NO_OPERATION, len,
				&dataset2_for_nooperation[0]);

		temp3[0] = 0x2;
		len = 0x1;
		slic_cmd(device_handle, channel_id, WRITE_SYSTEM_STATE_CFG,
				len, &temp3[0]);

		len = sizeof(dataset_internalCfgReg3) / sizeof(unsigned char);
		slic_cmd(device_handle, channel_id, WRITE_INTERNAL_CFG_REG3 ,
				len, &dataset_internalCfgReg3[0]);

		len = sizeof(dataset3_for_nooperation) / sizeof(unsigned char);
		slic_cmd(device_handle, channel_id, WRITE_NO_OPERATION, len,
				&dataset3_for_nooperation[0]);

		temp3[0] = 0xbf;
		len = 0x1;
		slic_cmd(device_handle, channel_id, WRITE_OPERATING_FUNCTION,
				len, &temp3[0]);

		temp3[0] = 0xc0;
		len = 0x1;
		slic_cmd(device_handle, channel_id, WRITE_OPERATING_CONDITIONS,
				len, &temp3[0]);

		temp3[0] = 0x6;
		len = 0x1;
		slic_cmd(device_handle, channel_id, WRITE_SYSTEM_STATE_CFG,
				len, &temp3[0]);

		temp3[0] = 0x6;
		len = 0x1;
		slic_cmd(device_handle, channel_id, WRITE_SYSTEM_STATE_CFG,
				len, &temp3[0]);

		temp3[0] = 0xc0;
		len = 0x1;
		slic_cmd(device_handle, channel_id, WRITE_OPERATING_CONDITIONS,
				len, &temp3[0]);

		temp3[0] = 0x16;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_SYSTEM_STATE_CFG,
				len, &temp3[0]);

		temp3[0] = 0xc0;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_OPERATING_CONDITIONS,
				len, &temp3[0]);

		temp3[0] = 0x16;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_SYSTEM_STATE_CFG,
				len, &temp3[0]);

		temp3[0] = 0x3f;
		if (channel_id == CHANNEL1)
			temp3[1] = 0xff;
		else
			temp3[1] = 0xbf;
		len = 0x02;
		slic_cmd(device_handle, channel_id, WRITE_INT_MASK, len,
				&temp3[0]);

		temp3[0] = 0x16;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_SYSTEM_STATE_CFG,
				len, &temp3[0]);

		temp3[0] = 0xc0;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_OPERATING_CONDITIONS,
				len, &temp3[0]);

		temp3[0] = 0x0;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_OPERATING_CONDITIONS,
				len, &temp3[0]);

		if (slic_chip_type == ZARLINK_LE88266) {
			temp3[0] = 0x0;
			temp3[1] = 0x2;
		} else if (slic_chip_type == ZARLINK_LE88506) {
			temp3[0] = 0x0;
			temp3[1] = 0x0;
		}

		len = 0x02;
		slic_cmd(device_handle, channel_id, WRITE_DC_CALIBRATION, len,
				&temp3[0]);

		if (slic_chip_type == ZARLINK_LE88266)
			temp3[0] = 0x2b;
		else if (slic_chip_type == ZARLINK_LE88506)
			temp3[0] = 0x23;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_SYSTEM_STATE, len,
				&temp3[0]);

	}
	/* Reading the Device Configuration register */
	len = 0x1;
	slic_cmd(device_handle, CHANNEL1, READ_DEVICE_CONFIGURATION, len,
			&temp1);
	printk(KERN_INFO "DEV reg is %x\n", temp1);

	/* Enabling interrupt by writing into Device Configuration Register */
	temp1 &= 0x7F;
	len = 0x01;
	slic_cmd(device_handle, CHANNEL1, WRITE_DEVICE_CFG, len, &temp1);

	/*  Reading the Device Configuration register */
	len = 0x1;
	slic_cmd(device_handle, CHANNEL1, READ_DEVICE_CONFIGURATION, len,
			&temp1);
	printk(KERN_INFO "DEV reg after is %x\n", temp1);

	/*  Reading the Mask register */
	len = 0x2;
	slic_cmd(device_handle, CHANNEL1, READ_INT_MASK, len, &temp2[0]);
	printk(KERN_INFO "Mask reg before setting is %x %x\n",
					 temp2[0], temp2[1]);

	/*  Writing into the mask register */
	temp2[0] = 0xFF;
	temp2[1] = 0xFF;
	len = 0x2;
	slic_cmd(device_handle, CHANNEL1, WRITE_INT_MASK, len, &temp2[0]);

	temp2[0] = 0xFF;
	temp2[1] = 0xFF;
	len = 0x2;
	slic_cmd(device_handle, CHANNEL2, WRITE_INT_MASK, len, &temp2[0]);

	/*  Reading the Mask register */
	len = 0x2;
	slic_cmd(device_handle, CHANNEL1, READ_INT_MASK, len, &temp2[0]);
	printk(KERN_INFO "Mask reg after setting is %x %x\n",
					 temp2[0], temp2[1]);

	temp1 = slic_ch[slic_id].ch1_tx_slot;
	len = 0x1;
	slic_cmd(device_handle, CHANNEL1, WRITE_TX_TIME_SLOT, len, &temp1);

	len = 0x1;
	slic_cmd(device_handle, CHANNEL1, READ_TX_TIME_SLOT, len, &temp1);
	printk(KERN_INFO "Read Tx Timeslot for CH1 is %x\n", temp1);

	temp1 = slic_ch[slic_id].ch2_tx_slot;
	len = 0x1;
	slic_cmd(device_handle, CHANNEL2, WRITE_TX_TIME_SLOT, len, &temp1);

	len = 0x1;
	slic_cmd(device_handle, CHANNEL2, READ_TX_TIME_SLOT, len, &temp1);
	printk(KERN_INFO "Read Tx Timeslot for CH2 is %x\n", temp1);

	temp1 = slic_ch[slic_id].ch1_rx_slot;
	len = 0x1;
	slic_cmd(device_handle, CHANNEL1, WRITE_RX_TIME_SLOT, len, &temp1);

	len = 0x1;
	slic_cmd(device_handle, CHANNEL1, READ_RX_TIME_SLOT, len, &temp1);
	printk(KERN_INFO "Read Rx Timeslot for CH1 is %x\n", temp1);

	temp1 = slic_ch[slic_id].ch2_rx_slot;
	len = 0x1;
	slic_cmd(device_handle, CHANNEL2, WRITE_RX_TIME_SLOT, len, &temp1);

	len = 0x1;
	slic_cmd(device_handle, CHANNEL2, READ_RX_TIME_SLOT, len, &temp1);
	printk(KERN_INFO "Read Rx Timeslot for CH2 is %x\n", temp1);

	for (channel_id = CHANNEL1; channel_id <= CHANNEL2; channel_id++) {

		temp1 &= 0xBF;
		temp1 |= 0x80;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_OPERATING_FUNCTION,
				len, &temp1);

		len = 0x01;
		slic_cmd(device_handle, channel_id, READ_OPERATING_FUNCTION,
				len, &temp1);
		printk(KERN_INFO "Operating Fun for channel %d is %x\n",
				channel_id, temp1);

		/* Install Timers */
		len = 0x04;
		slic_cmd(device_handle, channel_id, READ_CADENCE_TIMER,	len,
				&cad[0]);
		printk(KERN_INFO "Cadence Timer Reg for CH%d before is %x %x"
				"%x %x\n", channel_id, cad[0], cad[1], cad[2],
			       cad[3]);

		len = sizeof(set_cadenceTimer) / sizeof(unsigned char);
		slic_cmd(device_handle, channel_id, WRITE_CADENCE_TIMER, len,
				&set_cadenceTimer[0]);

		len = 0x04;
		slic_cmd(device_handle, channel_id, READ_CADENCE_TIMER , len,
				&cad[0]);
		printk(KERN_INFO "Cadence Timer Reg for CH%d after is %x %x"
				"%x %x\n", channel_id, cad[0], cad[1], cad[2],
				cad[3]);
		if (slic_chip_type == ZARLINK_LE88266)
			temp1 = 0x20;
		 else if (slic_chip_type == ZARLINK_LE88506)
			temp1 = 0x21;
		len = 0x01;
		slic_cmd(device_handle, channel_id, WRITE_SYSTEM_STATE_CFG,
				len, &temp1);

		slic_cmd(device_handle, channel_id, READ_SYSTEM_STATE_CFG,
				len, &temp1);
		printk(KERN_INFO "Switching control for channel %d is %x\n",
				channel_id, temp1);
	}
	return 0;
}

void configure_spi_pdata(struct spi_device *spi)
{
	struct slic_platform_data *spi_slic_pdata;
	const u32 *iprop;
	struct device_node *np = spi->dev.of_node;

	spi_slic_pdata = kzalloc(sizeof(*spi_slic_pdata), GFP_KERNEL);
	if (spi_slic_pdata == NULL)
		return;

	spi->dev.platform_data = spi_slic_pdata;

	iprop = of_get_property(np, "ch1-rx-slot", NULL);
	spi_slic_pdata->ch1_rx_slot = *iprop;
	iprop = of_get_property(np, "ch1-tx-slot", NULL);
	spi_slic_pdata->ch1_tx_slot = *iprop;
	iprop = of_get_property(np, "ch2-rx-slot", NULL);
	spi_slic_pdata->ch2_rx_slot = *iprop;
	iprop = of_get_property(np, "ch2-tx-slot", NULL);
	spi_slic_pdata->ch2_tx_slot = *iprop;
	pr_info("SLIC config success\n");

}
static int slic_remove(struct spi_device *spi)
{

	printk(KERN_INFO "SLIC module uninstalled\n");
	return 0;
}

static int slic_probe(struct spi_device *spi)
{
	int ret = 0;
	unsigned char *device_handle;
	struct slic_platform_data *data;

	g_spi = spi;
	spi->bits_per_word = 8;
	spi->mode = 0;
	spi->mode |= SPI_CPHA | SPI_CPOL;

	if (num_slics >= MAX_NUM_OF_SLICS) {
		printk(KERN_ERR "Exceeded the max number of slics\n");
		return ret;
	}

	/* Initialize the SLIC */
	configure_spi_pdata(spi);
	data = spi->dev.platform_data;
	slic_ch[num_slics].ch1_tx_slot = data->ch1_tx_slot;
	slic_ch[num_slics].ch1_rx_slot = data->ch1_rx_slot;
	slic_ch[num_slics].ch2_tx_slot = data->ch2_tx_slot;
	slic_ch[num_slics].ch2_rx_slot = data->ch2_rx_slot;

	device_handle = 0x0;
	ret = slic_init_configure(device_handle, spi, num_slics);
	if (ret == 0) {
		num_slics++;
		printk(KERN_INFO "SLIC %d configuration success\n",
				num_slics);
	} else {
		printk(KERN_ERR "%s slic configuration failed\n", __func__);
		return ret;
	}

	return ret;
}

static struct spi_driver slic_driver = {
	.driver = {
		   .name = DRV_NAME,
		   .bus = &spi_bus_type,
		   .owner = THIS_MODULE,
		   },
	.probe = slic_probe,
	.remove = __devexit_p(slic_remove),

};

static int __init slic_init(void)
{
	int ret;
	printk(KERN_INFO "SLIC: " DRV_DESC "\n");
	printk(KERN_INFO  "####################################################"
			"\n# This driver was created solely by Freescale,     #"
			"\n# without the assistance, support or intellectual  #"
			"\n# property of Zarlink Semiconductor. No            #"
			"\n# maintenance or support will be provided by       #"
			"\n# Zarlink  Semiconductor regarding this driver.    #"
			"\n####################################################"
		"\n");

	ret = spi_register_driver(&slic_driver);
	if (ret != 0)
		printk(KERN_ERR "%s spi_register_driver failed\n",
							__func__);
	return ret;
}

static void __exit slic_exit(void)
{
	spi_unregister_driver(&slic_driver);
}

module_init(slic_init);
module_exit(slic_exit);

/*
 * drivers/net/gianfar_1588.c
 *
 * Copyright 2008-2011 Freescale Semiconductor, Inc.
 * Copyright 2009 IXXAT Automation, GmbH
 *
 * Author: Anup Gangwar <anup.gangwar@freescale.com>
 *	   Yashpal Dutta <yashpal.dutta@freescale.com>
 *
 * Gianfar Ethernet Driver -- IEEE 1588 interface functionality
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 */

#include <linux/vmalloc.h>
#include <linux/of.h>
#include "gianfar.h"
#include <linux/of_platform.h>

#if defined(CONFIG_1588_MUX_eTSEC1) || defined(CONFIG_1588_MUX_eTSEC2)
#define MPC85XX_PMUXCR_OFFS 0x60
#if defined(CONFIG_1588_MUX_eTSEC1)
#define PMUXCR_eTSEC1_PPS 0x10000000
#endif
#if defined(CONFIG_1588_MUX_eTSEC2)
#define PMUXCR_eTSEC2_PPS 0x08000000
#endif
#endif

static int gfar_ptp_init_circ(struct gfar_ptp_circular_t *buf);
static int gfar_ptp_is_empty(struct gfar_ptp_circular_t *buf);
static int gfar_ptp_nelems(struct gfar_ptp_circular_t *buf);
static int gfar_ptp_is_full(struct gfar_ptp_circular_t *buf);
static int gfar_ptp_insert(struct gfar_ptp_circular_t *buf,
			struct gfar_ptp_data_t *data);
static int gfar_ptp_find_and_remove(struct gfar_ptp_circular_t *buf,
			int key, struct gfar_ptp_data_t *data);
static u32 nominal_frequency(u32 sysclock_freq);
static int gfar_ptp_cal_attr(struct gfar_ptp_attr_t *ptp_attr);

static DECLARE_WAIT_QUEUE_HEAD(ptp_rx_ts_wait);
#define PTP_GET_RX_TIMEOUT	(HZ/10)

static u32 freq_compensation;

static struct proc_dir_entry *gfar_1588_proc_file;
static struct gfar_node_info_t gfar_node;
static struct gfar_1588_data_t gfar_1588_data;

/*64 bites add and return the result*/
static u64 add64_oper(u64 addend, u64 augend)
{
	u64 result = 0;
	u32 addendh, addendl, augendl, augendh;

	addendh = (u32)(addend >> 32);
	addendl = (u32)addend;

	augendh = (u32)(augend >> 32);
	augendl = (u32)augend;

	asm("addc %0,%1,%2" : "=r" (addendl) : "r" (addendl), "r" (augendl));
	asm("adde %0,%1,%2" : "=r" (addendh) : "r" (addendh), "r" (augendh));

	result = (((u64)addendh << 32) | (u64)addendl);

	return result;
}

/*64 bits multiplication and return the result*/
static u64 multi64_oper(u32 multiplier, u32 multiplicand)
{
	u64 result = 0;
	u64 tmp_ret = 0;
	u32 tmp_multi = multiplicand;
	int i;

	for (i = 0; i < 32; i++) {
		if (tmp_multi & 0x1) {
			tmp_ret = ((u64)multiplier << i);
			result = add64_oper(result, tmp_ret);
		}
		tmp_multi = (tmp_multi >> 1);
	}
	return result;
}

static u32 div64_oper(u64 dividend, u32 divisor, u32 *quotient)
{
	u32 time_h, time_l;
	u32 result;
	u64 tmp_dividend;
	int i;

	*quotient = 0;

	time_h = (u32)(dividend >> 32);
	time_l = (u32)dividend;
	time_h = time_h % divisor;
	for (i = 1; i <= 32; i++) {
		tmp_dividend = (((u64)time_h << 32) | (u64)time_l);
		tmp_dividend = (tmp_dividend << 1);
		time_h = (u32)(tmp_dividend >> 32);
		time_l = (u32)tmp_dividend;
		result = time_h / divisor;
		time_h = time_h % divisor;
		*quotient += (result << (32 - i));
	}
	return time_h;
}

/*
 * Resource required for accessing 1588 Timer Registers. There are few 1588
 * modules registers which are present in eTSEC1 memory space only. The second
 * reg entry there in denotes the 1588 regs.
 */
int gfar_ptp_init(struct gfar_private *priv)
{
	priv->ptimer = ioremap(priv->timer_resource.start,
			sizeof(struct gfar_regs_1588));
	if ((priv->ptimer == NULL) ||
			gfar_ptp_init_circ(&(priv->rx_time_sync)) ||
			gfar_ptp_init_circ(&(priv->rx_time_del_req)) ||
			gfar_ptp_init_circ(&(priv->rx_time_pdel_req)) ||
			gfar_ptp_init_circ(&(priv->rx_time_pdel_resp)))
		return 1;
	return 0;
}

void gfar_ptp_cleanup(struct gfar_private *priv)
{
	if (priv->ptimer != NULL)
		iounmap(priv->ptimer);

	if (priv->rx_time_sync.data_buf)
		vfree(priv->rx_time_sync.data_buf);

	if (priv->rx_time_del_req.data_buf)
		vfree(priv->rx_time_del_req.data_buf);
	if (priv->rx_time_pdel_req.data_buf)
		vfree(priv->rx_time_pdel_req.data_buf);
	if (priv->rx_time_pdel_resp.data_buf)
		vfree(priv->rx_time_pdel_resp.data_buf);
}

int gfar_ptp_do_txstamp(struct sk_buff *skb)
{
	u16  *udp_port;
	char *pkt_type;

	if (skb->len > 44) {
		pkt_type = (char *)(skb->data + GFAR_PTP_PKT_TYPE_OFFS);
		udp_port = (u16 *)(skb->data + GFAR_PTP_PORT_OFFS);

		/* Check if port is 319 for PTP Event, and check for UDP */
		if ((*udp_port == 0x013F) &&
				(*pkt_type == GFAR_PACKET_TYPE_UDP))
			return 1;
	}

	return 0;
}

void gfar_ptp_store_rxstamp(struct net_device *dev, struct sk_buff *skb)
{
	int msg_type, seq_id, control;
	struct gfar_ptp_data_t tmp_rx_time;
	struct gfar_private *priv = netdev_priv(dev);
	u16 udp_port;
	char pkt_type;

	pkt_type = *(((char *)skb->data) + GFAR_PTP_PKT_TYPE_OFFS);
	udp_port = *((u16 *)(skb->data + GFAR_PTP_PORT_OFFS));
	seq_id = *((u16 *)(skb->data + GFAR_PTP_SEQ_ID_OFFS));
	control = *((u8 *)(skb->data + GFAR_PTP_CTRL_OFFS));

	/* Check if port is 319 for PTP Event, and check for UDP */
	if ((udp_port == 0x13F) && (pkt_type == GFAR_PACKET_TYPE_UDP)) {
		tmp_rx_time.key = seq_id;
		tmp_rx_time.item.high = *((u32 *)skb->data);
		tmp_rx_time.item.low = *(((u32 *)skb->data) + 1);

		switch (control) {

		case GFAR_PTP_CTRL_SYNC:
			gfar_ptp_insert(&(priv->rx_time_sync), &tmp_rx_time);
			break;

		case GFAR_PTP_CTRL_DEL_REQ:
			gfar_ptp_insert(&(priv->rx_time_del_req), &tmp_rx_time);
			break;

		/* clear transportSpecific field*/
		case GFAR_PTP_CTRL_ALL_OTHER:
			msg_type = (*((u8 *)(skb->data +
					GFAR_PTP_MSG_TYPE_OFFS))) & 0x0F;
			switch (msg_type) {
			case GFAR_PTP_MSG_TYPE_PDREQ:
				gfar_ptp_insert(&(priv->rx_time_pdel_req),
							&tmp_rx_time);
				break;
			case GFAR_PTP_MSG_TYPE_PDRESP:
				gfar_ptp_insert(&(priv->rx_time_pdel_resp),
						&tmp_rx_time);
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}

		wake_up_interruptible(&ptp_rx_ts_wait);
	}
}

int gfar_ptp_init_circ(struct gfar_ptp_circular_t *buf)
{
	buf->data_buf = (struct gfar_ptp_data_t *)
				vmalloc((DEFAULT_PTP_RX_BUF_SZ+1) *
					sizeof(struct gfar_ptp_data_t));

	if (!buf->data_buf)
		return 1;
	buf->front = 0;
	buf->end = 0;
	buf->size = (DEFAULT_PTP_RX_BUF_SZ + 1);

	return 0;
}

static inline int gfar_ptp_calc_index(int size, int curr_index, int offset)
{
	return (curr_index + offset) % size;
}

int gfar_ptp_is_empty(struct gfar_ptp_circular_t *buf)
{
	return (buf->front == buf->end);
}

int gfar_ptp_nelems(struct gfar_ptp_circular_t *buf)
{
	const int front = buf->front;
	const int end = buf->end;
	const int size = buf->size;
	int n_items;

	if (end > front)
		n_items = end - front;
	else if (end < front)
		n_items = size - (front - end);
	else
		n_items = 0;

	return n_items;
}

int gfar_ptp_is_full(struct gfar_ptp_circular_t *buf)
{
	if (gfar_ptp_nelems(buf) == (buf->size - 1))
		return 1;
	else
		return 0;
}

int gfar_ptp_insert(struct gfar_ptp_circular_t *buf,
				struct gfar_ptp_data_t *data)
{
	struct gfar_ptp_data_t *tmp;

	if (gfar_ptp_is_full(buf))
		return 1;

	tmp = (buf->data_buf + buf->end);

	tmp->key = data->key;
	tmp->item.high = data->item.high;
	tmp->item.low = data->item.low;

	buf->end = gfar_ptp_calc_index(buf->size, buf->end, 1);

	return 0;
}

int gfar_ptp_find_and_remove(struct gfar_ptp_circular_t *buf,
			int key, struct gfar_ptp_data_t *data)
{
	int i;
	int size = buf->size, end = buf->end;

	if (gfar_ptp_is_empty(buf))
		return 1;

	i = buf->front;
	while (i != end) {
		if ((buf->data_buf + i)->key == key)
			break;
		i = gfar_ptp_calc_index(size, i, 1);
	}

	if (i == end) {
		buf->front = buf->end;
		return 1;
	}

	data->item.high = (buf->data_buf + i)->item.high;
	data->item.low = (buf->data_buf + i)->item.low;

	buf->front = gfar_ptp_calc_index(size, i, 1);

	return 0;
}

/* Set the 1588 timer counter registers */
static void gfar_set_1588cnt(struct net_device *dev,
			struct gfar_ptp_time *gfar_time)
{
	struct gfar_private *priv = netdev_priv(dev);
	u32 tempval;
	u64 alarm_value = 0, temp_alarm_val;
	struct gfar_ptp_attr_t ptp_attr;

	memset(&ptp_attr, 0, sizeof(struct gfar_ptp_attr_t));
	temp_alarm_val = add64_oper((u64)gfar_time->low,
		((u64)gfar_time->high)<<32);
	div64_oper(temp_alarm_val, TMR_SEC, &tempval);
	alarm_value = multi64_oper(tempval, TMR_SEC);
	temp_alarm_val = add64_oper((u64)TMR_ALARM1_L,
			((u64)TMR_ALARM1_H)<<32);
	alarm_value = add64_oper(alarm_value, temp_alarm_val);
	/* We must write the tmr_cnt_l register first */
	tempval = (u32)gfar_time->low;
	gfar_write(&priv->ptimer->tmr_cnt_l, tempval);
	tempval = (u32)gfar_time->high;
	gfar_write(&priv->ptimer->tmr_cnt_h, tempval);
	tempval = (u32)alarm_value;
	gfar_write(&(priv->ptimer->tmr_alarm1_l), tempval);
	tempval = (u32)(alarm_value>>32);
	gfar_write(&(priv->ptimer->tmr_alarm1_h), tempval);
	if (gfar_ptp_cal_attr(&ptp_attr))
		return;
	gfar_write(&(priv->ptimer->tmr_fiper1), ptp_attr.tmr_fiper1);
}

/* Get both the time-stamps and use the larger one */
static void gfar_get_tx_timestamp(struct gfar __iomem *regs,
			struct gfar_ptp_time *tx_time)
{
	struct gfar_ptp_time tx_set_1, tx_set_2;
	u32 tmp;

	/* Read the low register first */
	tx_set_1.low = gfar_read(&regs->tmr_txts1_l);
	tx_set_1.high = gfar_read(&regs->tmr_txts1_h);

	tx_set_2.low = gfar_read(&regs->tmr_txts2_l);
	tx_set_2.high = gfar_read(&regs->tmr_txts2_h);

	tmp = 0;

	if (tx_set_2.high > tx_set_1.high)
		tmp = 1;
	else if (tx_set_2.high == tx_set_1.high)
		if (tx_set_2.low > tx_set_1.low)
			tmp = 1;

	if (tmp == 0) {
		tx_time->low = tx_set_1.low;
		tx_time->high = tx_set_1.high;
	} else {
		tx_time->low = tx_set_2.low;
		tx_time->high = tx_set_2.high;
	}
}

static uint8_t gfar_get_rx_time(struct gfar_private *priv, struct ifreq *ifr,
		struct gfar_ptp_time *rx_time, int mode)
{
	struct gfar_ptp_data_t tmp;
	int key, flag;

	key = *((int *)ifr->ifr_data);
	switch (mode) {
	case PTP_GET_RX_TIMESTAMP_SYNC:
		flag = gfar_ptp_find_and_remove(&(priv->rx_time_sync),
						key, &tmp);
		break;
	case PTP_GET_RX_TIMESTAMP_DEL_REQ:
		flag = gfar_ptp_find_and_remove(&(priv->rx_time_del_req),
						key, &tmp);
		break;

	case PTP_GET_RX_TIMESTAMP_PDELAY_REQ:
		flag = gfar_ptp_find_and_remove(&(priv->rx_time_pdel_req),
						key, &tmp);
		break;
	case PTP_GET_RX_TIMESTAMP_PDELAY_RESP:
		flag = gfar_ptp_find_and_remove(&(priv->rx_time_pdel_resp),
						key, &tmp);
		break;

	default:
		flag = 1;
		printk(KERN_ERR "ERROR\n");
		break;
	}

	if (!flag) {
		rx_time->high = tmp.item.high;
		rx_time->low = tmp.item.low;
		return 0;
	} else {
		wait_event_interruptible_timeout(ptp_rx_ts_wait, 0,
					PTP_GET_RX_TIMEOUT);

		switch (mode) {
		case PTP_GET_RX_TIMESTAMP_SYNC:
			flag = gfar_ptp_find_and_remove(&(priv->rx_time_sync),
				key, &tmp);
			break;
		case PTP_GET_RX_TIMESTAMP_DEL_REQ:
			flag = gfar_ptp_find_and_remove(
				&(priv->rx_time_del_req), key, &tmp);
			break;
		case PTP_GET_RX_TIMESTAMP_PDELAY_REQ:
			flag = gfar_ptp_find_and_remove(
				&(priv->rx_time_pdel_req), key, &tmp);
			break;
		case PTP_GET_RX_TIMESTAMP_PDELAY_RESP:
			flag = gfar_ptp_find_and_remove(
				&(priv->rx_time_pdel_resp), key, &tmp);
			break;
		}

		if (flag == 0) {
			rx_time->high = tmp.item.high;
			rx_time->low = tmp.item.low;
			return 0;
		}

		return -1;
	}
}

static void gfar_get_curr_cnt(struct gfar_regs_1588 __iomem *ptimer,
			struct gfar_ptp_time *curr_time)
{
	curr_time->low = gfar_read(&ptimer->tmr_cnt_l);
	curr_time->high = gfar_read(&ptimer->tmr_cnt_h);
}

/*set Fiper Trigger Alarm */
void gfar_set_fiper_alarm(struct net_device *dev, struct gfar_ptp_time *alarm)
{
	struct gfar_private *priv = netdev_priv(dev);
	struct gfar_ptp_attr_t ptp_attr;

	memset(&ptp_attr, 0, sizeof(struct gfar_ptp_attr_t));
	gfar_write(&(priv->ptimer->tmr_alarm1_l), alarm->low);
	gfar_write(&(priv->ptimer->tmr_alarm1_h), alarm->high);
	if (gfar_ptp_cal_attr(&ptp_attr))
		return;
	gfar_write(&(priv->ptimer->tmr_fiper1), ptp_attr.tmr_fiper1);
}

int gfar_ioctl_1588(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct gfar_private *priv = netdev_priv(dev);
	struct gfar __iomem *regs = priv->gfargrp[0].regs;
	struct gfar_ptp_time *cnt, *alarm;
	signed long *p_addend;
	struct gfar_ptp_time rx_time, tx_time, curr_time;
	int retval = 0;

	switch (cmd) {
	case PTP_GET_RX_TIMESTAMP_SYNC:
	case PTP_GET_RX_TIMESTAMP_DEL_REQ:
	case PTP_GET_RX_TIMESTAMP_PDELAY_REQ:
	case PTP_GET_RX_TIMESTAMP_PDELAY_RESP:
		retval = gfar_get_rx_time(priv, ifr, &rx_time, cmd);
		if (retval == 0)
			copy_to_user(ifr->ifr_data, &rx_time, sizeof(rx_time));
		break;
	case PTP_GET_TX_TIMESTAMP:
		gfar_get_tx_timestamp(regs, &tx_time);
		copy_to_user(ifr->ifr_data, &tx_time, sizeof(tx_time));
		break;
	case PTP_GET_CNT:
		gfar_get_curr_cnt(priv->ptimer, &curr_time);
		copy_to_user(ifr->ifr_data, &curr_time, sizeof(curr_time));
		break;
	case PTP_SET_CNT:
		cnt = (struct gfar_ptp_time *)ifr->ifr_data;
		gfar_set_1588cnt(dev, cnt);
		break;
	case PTP_SET_FIPER_ALARM:
		alarm = (struct gfar_ptp_time *)ifr->ifr_data;
		gfar_set_fiper_alarm(dev, alarm);
		break;
	case PTP_ADJ_ADDEND:
		p_addend = (signed long *)ifr->ifr_data;
		/* assign new value directly */
		gfar_write(&priv->ptimer->tmr_add, *p_addend);
		break;
	case PTP_GET_ADDEND:
		/* return initial timer add value
		 * to calculate drift correction */
		copy_to_user(ifr->ifr_data, &freq_compensation,
				sizeof(freq_compensation));
		break;
	case PTP_CLEANUP_TIMESTAMP_BUFFERS:
		/* reset sync buffer */
		priv->rx_time_sync.front = 0;
		priv->rx_time_sync.end = 0;
		priv->rx_time_sync.size = (DEFAULT_PTP_RX_BUF_SZ + 1);
		/* reset delay_req buffer */
		priv->rx_time_del_req.front = 0;
		priv->rx_time_del_req.end = 0;
		priv->rx_time_del_req.size = (DEFAULT_PTP_RX_BUF_SZ + 1);
		/* reset pdelay_req buffer */
		priv->rx_time_pdel_req.front = 0;
		priv->rx_time_pdel_req.end = 0;
		priv->rx_time_pdel_req.size = (DEFAULT_PTP_RX_BUF_SZ + 1);
		/* reset pdelay_resp buffer */
		priv->rx_time_pdel_resp.front = 0;
		priv->rx_time_pdel_resp.end = 0;
		priv->rx_time_pdel_resp.size = (DEFAULT_PTP_RX_BUF_SZ + 1);
		break;
	default:
		return -EINVAL;
	}
	return retval;
}

/* 1588 Module intialization and filer table populating routine*/
void gfar_1588_start(struct net_device *dev)
{
	struct gfar_private *priv = netdev_priv(dev);
	struct gfar_ptp_attr_t ptp_attr;
	u32 *tmr_prsc, *cksel;
	struct device_node *np;

	memset(&ptp_attr, 0, sizeof(struct gfar_ptp_attr_t));
	np = of_find_compatible_node(NULL, NULL, "fsl,gianfar-ptp-timer");
	if (np == NULL)	{
		printk(KERN_ERR "1588: Cannot find gianfar-ptp-timer node \r\n");
		return;
	}

	if (gfar_ptp_cal_attr(&ptp_attr))
		return;

	tmr_prsc = (u32 *)of_get_property(np, "tmr-prsc", NULL);
	if (tmr_prsc == NULL)	{
		printk(KERN_ERR "1588: Cannot find tmr-prsc property \r\n");
		return;
	}

	gfar_write(&(priv->ptimer->tmr_prsc), *tmr_prsc);
	gfar_write(&(priv->ptimer->tmr_fiper1), ptp_attr.tmr_fiper1);
	gfar_write(&(priv->ptimer->tmr_alarm1_l), TMR_ALARM1_L);
	gfar_write(&(priv->ptimer->tmr_alarm1_h), TMR_ALARM1_H);

	/* Need to mask the TCLK bits as they are initialized with 1 */
	gfar_write(&(priv->ptimer->tmr_ctrl),
		(gfar_read(&(priv->ptimer->tmr_ctrl))
			 & ~TMR_CTRL_TCLK_MASK) | (ptp_attr.tclk_period));

	/* initialize TMR_ADD with the initial frequency compensation value:
	 * freq_compensation = (2^32 / frequency ratio)
	 */
	div64_oper((((u64)2 << 31)*100),
		ptp_attr.freq_div_ratio, &freq_compensation);
	gfar_write(&(priv->ptimer->tmr_add), freq_compensation);

	cksel = (u32 *)of_get_property(np, "cksel", NULL);
	if (cksel == NULL)	{
		printk(KERN_ERR "1588: Cannot find cksel property \r\n");
		return;
	}

	gfar_write(&(priv->ptimer->tmr_ctrl),
		gfar_read(&(priv->ptimer->tmr_ctrl)) |
		TMR_CTRL_ENABLE | *cksel | TMR_CTRL_FIPER_START);
}

/* Cleanup routine for 1588 module.
 * When PTP is disabled this routing is called */
void gfar_1588_stop(struct net_device *dev)
{
	struct gfar_private *priv = netdev_priv(dev);

	gfar_write(&priv->ptimer->tmr_ctrl,
		gfar_read(&priv->ptimer->tmr_ctrl)
		& ~TMR_CTRL_ENABLE);
}

void pmuxcr_guts_write(void)
{
#if defined(CONFIG_1588_MUX_eTSEC1) || defined(CONFIG_1588_MUX_eTSEC2)
	struct device_node *np = NULL;
	void __iomem *immap = NULL;
	u32 pmuxcr;

	np =  of_find_compatible_node(NULL, NULL, "fsl,p2020-guts");
	immap = of_iomap(np, 0);
	if (immap) {
		pmuxcr = in_be32(immap + MPC85XX_PMUXCR_OFFS);

#if defined(CONFIG_1588_MUX_eTSEC1)
		pmuxcr |= PMUXCR_eTSEC1_PPS;
#endif

#if defined(CONFIG_1588_MUX_eTSEC2)
		pmuxcr |= PMUXCR_eTSEC2_PPS;
#endif
		out_be32(immap + MPC85XX_PMUXCR_OFFS, pmuxcr);
		iounmap(immap);
	} else {
		printk(KERN_INFO "1588 muxing could not be done,"
				" mapping failed\n");
	}
#endif
}

/**
 * nominal_frequency - This function calculates the nominal frequency.
 * nominal frequency is the desired clock frequency.
 * @sysclock_freq: Timer Oscillator Frequency
 *
 * Description:
 *  Returns the nominal frequency which is calculated on the following
 *  basis.
 *  nominal frequency should be less than the Timer Oscillator frequency.
 *  nominal frequency should be a factor of 1000.
 *
 *  Eg If Timer Oscillator frequency is 400.
 *     then nominal frequency can be 250.
 *
 *     If Timer Oscillator frequency is 600.
 *     then nominal frequency can be 500.
 *
 *     If Timer Oscillator frequency is 333.
 *     then nominal frequency can be 250.
 */
u32 nominal_frequency(u32 sysclock_freq)
{
	u32 remainder = 0;

	remainder = sysclock_freq % 50;
	if (remainder != 0) {
		sysclock_freq = sysclock_freq - remainder;
		sysclock_freq += 50;
	}

	while ((10000 % (sysclock_freq -= 50)) != 0)
		continue;
	return sysclock_freq;
}

int gfar_ptp_cal_attr(struct gfar_ptp_attr_t *ptp_attr)
{
	u32 *sysclock_freq, nominal_freq, tclk_period;
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "fsl,gianfar-ptp-timer");
	if (np == NULL)	{
		printk(KERN_ERR "1588: Cannot find gianfar-ptp-timer node \r\n");
		return 1;
	}
	sysclock_freq = (u32 *)of_get_property(np, "timer-frequency", NULL);
	if (sysclock_freq == NULL) {
		printk(KERN_DEBUG "1588: Cannot find timer-frequency property \r\n");
		return 1;
	}
	printk(KERN_DEBUG "1588 is running at system-clock"
			" frequency (%u) \r\n", *sysclock_freq);

	nominal_freq = nominal_frequency(DIV_ROUND(*sysclock_freq, 1000) \
							/ 1000);

	/* TCLK_PERIOD = 10^9/Nominal_Frequency in MHZ */
	tclk_period = 1000/nominal_freq;

	/* FIPER = (10^9 / (Required PPS * TCLK_PERIOD)) - TCLK_PERIOD*/
	ptp_attr->tmr_fiper1 = (ONE_GIGA / (PPS_1588 * tclk_period)) \
						- tclk_period;
	tclk_period <<= 16;
	ptp_attr->tclk_period = tclk_period;
	ptp_attr->nominal_freq = nominal_freq;
	ptp_attr->sysclock_freq = DIV_ROUND(*sysclock_freq, 1000) / 1000;

	/*
	 * FreqDivRatio = Timer Oscillator Freq / Nominal Freq
	 * and Timer Oscillator Freq = System Clock Freq
	 */
	ptp_attr->freq_div_ratio = (ptp_attr->sysclock_freq *
					100) / ptp_attr->nominal_freq;
	return 0;
}

static int gfar_1588_proc_read(char *buffer,
		char **buffer_location,
		off_t offset, int buffer_length, int *eof, void *data)
{
	int len;
	struct gfar_1588_data_t *gfar_1588_info = \
				(struct gfar_1588_data_t *)data;

	len = sprintf(buffer, "%s = '%s'\n",
			gfar_1588_info->name, gfar_1588_info->value);
	return len;
}

static int gfar_1588_proc_write(struct file *file, const char *buffer,
			unsigned long count, void *data)
{
	int i, cnt;
	struct gfar_private *priv;
	struct device_node *np = NULL;
	struct of_device *ofdev;
	struct gfar_1588_data_t *gfar_1588_info = \
				(struct gfar_1588_data_t *)data;
	struct gfar __iomem *regs;

	np = of_find_compatible_node(NULL, NULL, "fsl,gianfar-ptp-timer");
	if (np == NULL)	{
		printk(KERN_ERR "1588: Cannot find ptp_timer node. Check"
				" ptp_timer node in dts \r\n");
		return -EINVAL;
	}

	if (count > GFAR_1588_PROCFS_MAX_SIZE)
		count = GFAR_1588_PROCFS_MAX_SIZE;

	if (copy_from_user(gfar_1588_info->value, buffer, count))
		return -EFAULT;
	gfar_1588_info->value[count - 1] = '\0';
	printk(KERN_DEBUG "\r\n  buffer (%s) \r\n", gfar_1588_info->value);
	cnt = gfar_node.match_cnt / sizeof(struct of_device_id);

	/*
	 * Here we are getting the valid index ie "i" for which we have
	 * valid compatible string in the dts file.
	 */
	for (i = 0; i < (cnt - 1); i++) {
		printk(KERN_DEBUG "\r\n gfar-node is (%s)\r\n", \
				gfar_node.gfar_node_match[i].compatible);
		if (!of_find_compatible_node(NULL, NULL, \
				gfar_node.gfar_node_match[i].compatible))
			continue;
		else
			break;
	}

	/*
	 * After getting the valid index "i" from above for loop
	 * we are Enabling/Disabling the 1588 functionality on the
	 * eTSEC controllers.
	 */
	while (1) {
		np = of_find_compatible_node(np, NULL, \
				gfar_node.gfar_node_match[i].compatible);
		if (np == NULL) {
			printk(KERN_DEBUG "\r\nUnale to find gfar-node\n");
			break;
		} else {
			ofdev = of_find_device_by_node(np);
			priv = dev_get_drvdata(&ofdev->dev);
			regs = priv->gfargrp[0].regs;
			if (!strcmp(gfar_1588_info->value, "0")) {
				printk(KERN_DEBUG "\r\n PTPD OFF \r\n");
				priv->ptimer_present = 0;
				gfar_write(&priv->ptimer->tmr_ctrl,
					gfar_read(&priv->ptimer->tmr_ctrl)
							& ~TMR_CTRL_ENABLE);
				gfar_write(&regs->rctrl,
					gfar_read(&regs->rctrl)
							& ~RCTRL_TS_ENABLE);
			} else if (!strcmp(gfar_1588_info->value, "1")) {
				printk(KERN_DEBUG "\r\n PTPD ON \r\n");
				priv->ptimer_present = 1;
				gfar_write(&priv->ptimer->tmr_ctrl,
					gfar_read(&priv->ptimer->tmr_ctrl)
							| TMR_CTRL_ENABLE);
				gfar_write(&regs->rctrl,
					gfar_read(&regs->rctrl)
							| RCTRL_TS_ENABLE);
			}
		}
	}
	return count;
}

void gfar_1588_proc_init(struct of_device_id *dev_id, int cnt)
{
	gfar_1588_proc_file = create_proc_entry(GFAR_1588_PROCFS_NAME,\
							 0644, NULL);

	if (gfar_1588_proc_file == NULL) {
		remove_proc_entry(GFAR_1588_PROCFS_NAME, NULL);
		printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
			GFAR_1588_PROCFS_NAME);
		return;
	}
	strcpy(gfar_1588_data.name, GFAR_1588_PROCFS_NAME);
	strcpy(gfar_1588_data.value, "1");
	gfar_1588_proc_file->read_proc  = gfar_1588_proc_read;
	gfar_1588_proc_file->write_proc = gfar_1588_proc_write;
	gfar_1588_proc_file->mode	= S_IFREG | S_IRUGO;
	gfar_1588_proc_file->uid	= 0;
	gfar_1588_proc_file->gid	= 0;
	gfar_1588_proc_file->data	= &gfar_1588_data;
	gfar_node.gfar_node_match = dev_id;
	gfar_node.match_cnt = cnt;
	printk(KERN_INFO "/proc/%s created \r\n", GFAR_1588_PROCFS_NAME);
}

void gfar_1588_proc_exit()
{
	remove_proc_entry(GFAR_1588_PROCFS_NAME, NULL);
	printk(KERN_INFO "/proc/%s removed \r\n", GFAR_1588_PROCFS_NAME);
}

/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Tony Li <tony.li@freescale.com>
 *
 * Description:
 * UPC external definitions and structure.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _ASM_POWERPC_UPC_H
#define _ASM_POWERPC_UPC_H

#include <asm/qe.h>
#include <asm/ucc_fast.h>

#define UPC_MAX_NUM		2
#define UPC_UCC_MAX_NUM		4
#define UPC_SLOT_MAX_NUM	4
#define UPC_SLOT_EP_MAX_NUM 	32

enum upcd_port_enable {
	MASK_RX,
	MASK_TX,
	MASK_ALL,
};

enum upc_ul_pl {
	UTOPIA,
	POS,
};

struct upc_slot_tx {
	int slot; /* Tx upc device num */
	int tmp; /* Transmit multiple PHY */
	int tsp; /* Transmit single port */
	int tb2b; /* Tansmit back to back */
	int tehs; /* Transmit extra header size */
	int tudc; /* Transmit user-defined cells */
	int txpw; /* Transmit data bus width */
	int tpm; /* Tx selection priority */
};

struct upc_slot_rx {
	int slot; /* Rx upc device num */
	int rehs; /* Receive extra header size */
	int rmp; /* Receive multiple PHY */
	int rb2b; /* Receive back to back */
	int rudc; /* Receive user-defined cells */
	int rxpw; /* Receive data bus width */
	int rxp; /* Receive parity check */
};

struct upc_slot_rate_info {
	u8 pre;
	u8 tirec;
	int sub_en[4];
	u16 tirr[4];
};

struct upc_slot_info {
	struct upc_info *upc_info;
	int ep_num;
	int upc_slot_num;
	int icd; /* idle cell discard */
	int pe; /* port enable */
	int heci; /* HEC included */
	int hecc; /* HEC check */
	int cos; /* Coset mode on the HEC */
	struct upc_slot_rate_info rate;
};

struct upc_info {
	int upc_num;
	u32 regs;
	const char *rx_clock;
	const char *tx_clock;
	const char *internal_clock;
	enum upc_ul_pl mode;
	int tms;
	int rms;
	int addr;
	int diag;
	int uplpat;
	int uplpar;
	int uphec;
	struct upc_slot_info *us_info[UPC_SLOT_MAX_NUM];
};

struct upc_private {
	struct upc_info *upc_info;
	struct upc *upc_regs;
};

int ucc_attach_upc_tx_slot(int ucc_num, struct upc_slot_tx *ust);
int ucc_detach_upc_tx_slot(int ucc_num, struct upc_slot_tx *ust);
int ucc_attach_upc_rx_slot(int ucc_num, struct upc_slot_rx *usr);
int ucc_detach_upc_rx_slot(int ucc_num, struct upc_slot_rx *usr);
int upc_init(void);
void upc_dump(int upc_num);
#endif

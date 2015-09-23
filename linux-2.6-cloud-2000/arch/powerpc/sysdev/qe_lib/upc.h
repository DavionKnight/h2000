/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Tony Li <tony.li@freescale.com>
 *
 * Description:
 * UPC register definition.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __POWERPC_SYSDEV_UPC_H
#define __POWERPC_SYSDEV_UPC_H

/* UPC General Configuration Register(UPGCR) */
#define UPGCR_PROT	0x80000000
#define UPGCR_TMS	0x40000000
#define UPGCR_RMS	0x20000000
#define UPGCR_ADDR	0x10000000
#define UPGCR_DIAG	0x01000000

/* UPC Last PHY Address Register(UPLPA) */
#define MAX_PHY_NUMBER	0x1F
#define UPLPA_TX_LAST_PHY_SHIFT	24
#define UPLPA_RX_LAST_PHY_SHIFT	16

/* UPC UCC Configuration Register(UPUC) */
#define UPUC_TMP	0x80
#define UPUC_TSP	0x40
#define UPUC_TB2B	0x20
#define UPUC_ALL	(UPUC_TMP | UPUC_TSP | UPUC_TB2B)

/* UPC Device Configuration Register bit field */
#define UPDC_TEHS	0xF0000000
#define UPDC_REHS	0x0F000000
#define UPDC_ICD	0x00800000
#define UPDC_PE		0x00600000
#define UPDC_PE_TX	0x00400000
#define UPDC_PE_RX	0x00200000
#define UPDC_TXUCC	0x000c0000
#define UPDC_RXUCC	0x00030000
#define UPDC_TXENB	0x00008000
#define UPDC_RXENB	0x00004000
#define UPDC_RB2B	0x00002000
#define UPDC_TUDC	0x00001000
#define UPDC_RUDC	0x00000800
#define UPDC_RXP	0x00000400
#define UPDC_TXPW	0x00000080
#define UPDC_RXPW	0x00000040
#define UPDC_TPM	0x00000020
#define UPDC_RMP	0x00000008
#define UPDC_HECI	0x00000004
#define UPDC_HECC	0x00000002
#define UPDC_COS	0x00000001

/* UPC Device X internal Rate Configuration Register */
#define UPRP_PRE	0xFF00
#define UPRP_TIREC	0x000F

/* UPC Device X Transmit Internal Rate Register */
#define UPTIRR_EN	0x8000
#define UPTIRR_TIRR	0x7FFF

#endif

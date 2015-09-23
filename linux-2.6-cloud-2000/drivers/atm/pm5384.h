/*
 * Copyright (C) 2006-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Dave Liu (daveliu@freescale.com)
 *         Tony Li (tony.li@freescale.com)
 * Description:
 * suni5384 phy register descriptions.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

/* SUNI registers */

#define SUNI_MRI		0x00	/* Master Reset and Identity */
#define SUNI_MC			0x01	/* Master Configuration */
#define SUNI_CMC1		0x04	/* Channel Master Configuration#1 */
#define SUNI_CMC2		0x05	/* Channel Master Configuration#2 */
#define SUNI_MIS1		0x06	/* Channel Reset/Interrupt Status#1 */
#define SUNI_MIS2		0x07	/* Channel Master Interrupt Status#2 */

#define SUNI_CALRC		0x08	/* Channel Auto Line RDI Control */
#define SUNI_CAPRC		0x09	/* Channel Auto Path RDI Control */
#define SUNI_CAEPRC		0x0A	/* Channel Auto Enhanced Path RDI Control */
#define SUNI_CRRERC		0x0B	/* Channel Receive RDI and Enhanced RDI Control */
#define SUNI_CRLAC		0x0C	/* Channel Receive Line AIS Control */
#define SUNI_CRPAC		0x0D	/* Channel Receive Path AIS Control */

#define SUNI_CRAC1		0x0E	/* Channel Receive Alarm Control #1 */
#define SUNI_CRAC2		0x0F	/* Channel Receive Alarm Control #2 */

#define SUNI_RSOP_CIE		0x10	/* RSOP Control/Interrupt Enable */
#define SUNI_RSOP_SIS		0x11	/* RSOP Status/Interrupt Status */
#define SUNI_RSOP_SBL		0x12	/* RSOP Section BIP-8 LSB */
#define SUNI_RSOP_SBM		0x13	/* RSOP Section BIP-8 MSB */
#define SUNI_TSOP_CTRL		0x14	/* TSOP Control */
#define SUNI_TSOP_DIAG		0x15	/* TSOP Diagnostic */

#define SUNI_RLOP_CS		0x18	/* RLOP Control/Status */
#define SUNI_RLOP_IES		0x19	/* RLOP Interrupt Enable/Status */
#define SUNI_RLOP_LBL		0x1A	/* RLOP Line BIP-8/24 LSB */
#define SUNI_RLOP_LB		0x1B	/* RLOP Line BIP-8/24 */
#define SUNI_RLOP_LBM		0x1C	/* RLOP Line BIP-8/24 MSB */
#define SUNI_RLOP_LFL		0x1D	/* RLOP Line FEBE LSB */
#define SUNI_RLOP_LF		0x1E	/* RLOP Line FEBE */
#define SUNI_RLOP_LFM		0x1F	/* RLOP Line FEBE MSB */

#define SUNI_TLOP_CTRL		0x20	/* TLOP Control */
#define SUNI_TLOP_DIAG		0x21	/* TLOP Diagnostic */
#define SUNI_TLOP_TXK1		0x22	/* TLOP Transmit K1 */
#define SUNI_TLOP_TXK2		0x23	/* TLOP Transmit K2 */
#define SUNI_TLOP_TXS1		0x24	/* TLOP Transmit S1 */
#define SUNI_TLOP_TXJ0Z0	0x25	/* TLOP Transmit J0/Z0 */

#define SUNI_RPOP_SC		0x30	/* RPOP Status/Control */
#define SUNI_RPOP_IS		0x31	/* RPOP Interrupt Status */
#define SUNI_RPOP_PIS		0x32	/* RPOP Pointer Interrupt Status */
#define SUNI_RPOP_IE		0x33	/* RPOP Interrupt Enable */
#define SUNI_RPOP_PIE		0x34	/* RPOP Pointer Interrupt Enable */
#define SUNI_RPOP_PL		0x35	/* RPOP Pointer LSB */
#define SUNI_RPOP_PM		0x36	/* RPOP Pointer MSB */
#define SUNI_RPOP_PSL		0x37	/* RPOP Path Signal Label */
#define SUNI_RPOP_PBL		0x38	/* RPOP Path BIP-8 LSB */
#define SUNI_RPOP_PBM		0x39	/* RPOP Path BIP-8 MSB */
#define SUNI_RPOP_PFL		0x3A	/* RPOP Path FEBE LSB */
#define SUNI_RPOP_PFM		0x3B	/* RPOP Path FEBE MSB */
#define SUNI_RPOP_RDI		0x3C	/* RPOP RDI */
#define SUNI_RPOP_PBC		0x3D	/* RPOP Ring Control */

#define SUNI_TPOP_CD		0x40	/* TPOP Control/Diagnostic */
#define SUNI_TPOP_PC		0x41	/* TPOP Pointer Control */
#define SUNI_TPOP_CPL		0x43	/* TPOP Current Pointer LSB */
#define SUNI_TPOP_CPM		0x44	/* TPOP Current Pointer MSB */
#define SUNI_TPOP_APL		0x45	/* TPOP Arbitrary Pointer LSB */
#define SUNI_TPOP_APM		0x46	/* TPOP Arbitrary Pointer MSB */
#define SUNI_TPOP_PT		0x47	/* TPOP Path Trace */
#define SUNI_TPOP_PSL		0x48	/* TPOP Path Signal Label */
#define SUNI_TPOP_PS		0x49	/* TPOP Path Status */
#define SUNI_TPOP_CL		0x4E	/* TPOP Concatenation LSB */
#define SUNI_TPOP_CM		0x4F	/* TPOP Concatenation MSB */

#define SUNI_SPTB_EPSL		0x54	/* SPTB Expected Path Signal Label */

#define SUNI_RXCP_CFG1		0x60	/* RXCP Configuration 1 */
#define SUNI_RXCP_CFG2		0x61	/* RXCP Configuration 2 */
#define SUNI_RXCP_FUCC		0x62	/* RXCP FIFO/UTOPIA Control and Configuration */
#define SUNI_RXCP_IECS		0x63	/* RXCP Interrupt Enable and Count Status */
#define SUNI_RXCP_STATUS	0x64	/* RXCP Status/Interrupt Status */
#define SUNI_RXCP_LCDM		0x65	/* RXCP LCD Count Threshold MSB */
#define SUNI_RXCP_LCDL		0x66	/* RXCP LCD Count Threshold LSB */
#define SUNI_RXCP_IDLE_PTRN	0x67	/* RXCP Idle Cell Header Pattern */
#define SUNI_RXCP_IDLE_MASK	0x68	/* RXCP Idle Cell Header Mask */
#define SUNI_RXCP_HCS		0x6A	/* RXCP HCS Error Count */
#define SUNI_RXCP_RCCL		0x6B	/* RACP Receive Cell Counter LSB */
#define SUNI_RXCP_RCC		0x6C	/* RACP Receive Cell Counter */
#define SUNI_RXCP_RCCM		0x6D	/* RACP Receive Cell Counter MSB */

#define SUNI_TXCP_CFG1		0x80	/* TXCP Configuration 1 */
#define SUNI_TXCP_CFG2		0x81	/* TXCP Configuration 2 */
#define SUNI_TXCP_TCCS		0x82	/* TXCP Transmit Cell Count Status */
#define SUNI_TXCP_IES		0x83	/* TXCP Interrupt Enable/Status */
#define SUNI_TXCP_IHDRC		0x84	/* TXCP Idle/Unassigned Cell Hdr control */
#define SUNI_TXCP_IPLDC		0x85	/* TACP Idle/Unassigned Cell Payload control */
#define SUNI_TXCP_TCCL		0x86	/* TACP Transmit Cell Counter LSB */
#define SUNI_TXCP_TCC		0x87	/* TACP Transmit Cell Counter */
#define SUNI_TXCP_TCCM		0x88	/* TACP Transmit Cell Counter MSB */

#define SUNI_MIS		0x100	/* SUNI Master Interrupt Status */
#define SUNI_DIC		0x101	/* SUNI DCC Interface Configuration */
#define SUNI_CCSC		0x102	/* CSPI Clock Synthesis Config Register */

#define SUNI_TFCLK_CFG		0x108	/* TFCLK DLL Configuration */
#define SUNI_TFCLK_RST		0x10A	/* TFCLK DLL Reset Register */
#define SUNI_TFCLK_CS		0x10B	/* TFCLK DLL Control Status */
#define SUNI_RFCLK_CFG		0x10C	/* RFCLK DLL Configuration */
#define SUNI_RFCLK_RST		0x10E	/* RFCLK DLL Reset Register */
#define SUNI_RFCLK_CS		0x10F	/* RFCLK DLL Control Status */

#define SUNI_L2RA		0x130	/* Level 2 Receive Address Register */
#define SUNI_L2TA		0x131	/* Level 2 Transmit Address Register */

#define SUNI_MT			0x200	/* Master Test */


/* SUNI register values */

/* Master Reset and Identity - 0x00 */
#define SUNI_MRI_TYPE_MASK	0x78	/* PM5384 type mask */
#define SUNI_MRI_TYPE_VAL	0x10	/* PM5384 type value */
#define SUNI_MRI_ID_MASK	0x07	/* PM5384 ID mask */
#define SUNI_MRI_RESET		0x80	/* RW, reset & power down chip
					0: normal operation
					1: reset & low power */
/* Master Configuration -0x01 */
#define SUNI_MC_ATM8		0x02	/* Enable utopia level 2 8 bit bus */
#define SUNI_MC_ATM16		0x00	/* Enable utopia Level 2 16 bit bus */

/* Channel Master Configuration#1 -0x04 */
#define SUNI_CMC1_DLE		0x08	/* Enable diagnostic loop */
#define SUNI_CMC1_PDLE		0x10	/* Enable parallel diagnostic loopback */

/* Channel Master Configuration#2 -0x05 */
#define SUNI_CMC2_LOOPT		0x20	/* Transmit section clock source select
						0- derived from REFCLK
						1- derived from line recovered clock */
#define SUNI_CMC2_DLE		0x40	/* Enable diagnostic loopback */
#define SUNI_CMC2_LLE		0x80	/* Enable line loopback */

/* Channel Receive Alarm Control #2 -0x0F */
#define SUNI_CRAC2_LOSEN	0x01	/* Loss of signal */
#define SUNI_CRAC2_LOFEN	0x02	/* Loss of frame */
#define SUNI_CRAC2_OOFEN	0x04	/* Out of frame */
#define SUNI_CRAC2_LAISEN	0x08	/* Line Alarm Indication Signal */
#define SUNI_CRAC2_LRDIEN	0x10	/* Line remote Defect Indication */
#define SUNI_CRAC2_SDBEREN	0x20	/* Signal Degrade Bit Error Rate */
#define SUNI_CRAC2_SFBEREN	0x40	/* Signal Fail Bit Error Rate */
#define SUNI_CRAC2_STIMEN	0x80	/* Section Trace Identifier Mismatch */

/* RSOP Control/Interrupt Enable -0x10 */
#define SUNI_RSOP_CIE_OOFE	0x01	/* out of frame interrupt enable */
#define SUNI_RSOP_CIE_LOFE	0x02	/* loss of frame interrupt enable */
#define SUNI_RSOP_CIE_LOSE	0x04	/* loss of signal interrupt enable */
#define SUNI_RSOP_CIE_BIPEE	0x08	/* section BIP-8 errors interrupt enable */
#define SUNI_RSOP_CIE_ALGO2	0x10	/* The second framing algorithms enable */
#define SUNI_RSOP_CIE_FOOF	0x20	/* force out of frame */
#define SUNI_RSOP_CIE_DDS	0x40	/* descrambling disable */
#define SUNI_RSOP_CIE_BIP	0x80	/* enable the accumulating of BIP word errors */
#define SUNI_RSOP_CIE_LOS	(SUNI_RSOP_CIE_OOFE | SUNI_RSOP_CIE_LOFE | SUNI_RSOP_CIE_LOSE)

/* RSOP Status/Interrupt Status -0x11 */
#define SUNI_RSOP_SIS_OOFV	0x01	/* out of frame */
#define SUNI_RSOP_SIS_LOFV	0x02	/* loss of frame */
#define SUNI_RSOP_SIS_LOSV	0x04	/* loss of signal */
#define SUNI_RSOP_SIS_OOFI	0x08	/* out of frame interrupt */
#define SUNI_RSOP_SIS_LOFI	0x10	/* loss of frame interrupt */
#define SUNI_RSOP_SIS_LOSI	0x20	/* loss of signal interrupt */
#define SUNI_RSOP_SIS_BIPEI	0x40	/* section BIP-8 interrupt */
#define SUNI_RSOP_SIS_LOS	(SUNI_RSOP_SIS_OOFV | SUNI_RSOP_SIS_LOFV | SUNI_RSOP_SIS_LOSV)

/* TSOP Control -0x14 */
#define SUNI_TSOP_CTRL_LAIS	0x01	/* insert alarm indication signal */
#define SUNI_TSOP_CTRL_DS	0x40	/* disable scrambling */

/* TSOP Diagnostic -0x15 */
#define SUNI_TSOP_DIAG_DFP	0x01	/* insert single bit error cont. */
#define SUNI_TSOP_DIAG_DBIP8	0x02	/* insert section BIP err (cont) */
#define SUNI_TSOP_DIAG_DLOS	0x04	/* set line to zero (loss of signal) */

/* TLOP Control -0x20 */
#define SUNI_TLOP_CTRL_LRDI	0x01	/* insert line RDI into transmit stream */
#define SUNI_TLOP_CTRL_APSREG	0x20	/* select the source for xmit */

/* TLOP Diagnostic -0x21 */
#define SUNI_TLOP_DIAG_DBIP	0x01	/* insert line BIP err (continuously) */

/* TPOP Control/Diagnostic -0x40 */
#define SUNI_TPOP_DIAG_PAIS	0x01	/* insert STS path alarm ind (cont) */
#define SUNI_TPOP_DIAG_DB3	0x02	/* insert path BIP err (continuously) */

/* TPOP Arbitrary Pointer MSB -0x46 */
#define SUNI_TPOP_APM_APTR	0x03	/* arbitrary pointer, upper 2 bits */
#define SUNI_TPOP_APM_APTR_SHIFT	0
#define SUNI_TPOP_APM_S		0x0C	/* "unused" bits of payload pointer */
#define SUNI_TPOP_APM_S_SHIFT	2
#define SUNI_TPOP_APM_NDF	0xF0	 /* NDF bits */
#define SUNI_TPOP_APM_NDF_SHIFT	4

#define SUNI_TPOP_S_SONET	0	/* set S bits to 00 */
#define SUNI_TPOP_S_SDH		2	/* set S bits to 10 */

/* RXCP Configuration 1 -0x60 */
#define SUNI_RXCP_CFG1_HCSADD	0x04	/* polynomial added and HCS compared */
#define SUNI_RXCP_CFG1_HDSCR	0x40	/* 1-descrambling polynomial over all bytes */
#define SUNI_RXCP_CFG1_DDSCR	0x80	/* 1-cell payload descrambling disable */

/* RXCP Configuration 2 -0x61 */
#define SUNI_RXCP_CFG2_IDLEPASS	0x20	/* Idle cell header pattern ignored, idle cell pass */

/* RXCP FIFO/UTOPIA Control and Configuration -0x62 */
#define SUNI_RXCP_FUCC_FIFORST		0x01	/* Rx FIFO reset */
#define SUNI_RXCP_FUCC_RCALEVEL0	0x10	/* RCA level 0 control, 1-empty, 0-near empty deassert */
#define SUNI_RXCP_FUCC_RCAINV		0x20	/* Invert polarity of RCA */
#define SUNI_RXCP_FUCC_RXPTYP		0x80	/* 1-RDAT[0:15] even parity, 0-odd parity bit */

/* RXCP Interrupt Enable and Count Status -0x63 */
#define SUNI_RXCP_IECS_LCDE	0x01	/* loss of cell delineation interrupt enable */
#define SUNI_RXCP_IECS_FOVRE	0x02	/* FIFO overrun error interrupt enable */
#define SUNI_RXCP_IECS_HCSE	0x04	/* HCS error interrupt enable */
#define SUNI_RXCP_IECS_OOCDE	0x08	/* cell delineation interrupt enable */
#define SUNI_RXCP_IECS_XFERE	0x10	/* accumulation interval interrupt enable */
#define SUNI_RXCP_IECS_OVR	0x40	/* PM counter overrun status */
#define SUNI_RXCP_IECS_XFERI	0x80	/* PM counter register updated */

/* RXCP Status/Interrupt Status -0x64 */
#define SUNI_RXCP_STATUS_LCDI	0x01	/* loss of cell delineation change */
#define SUNI_RXCP_STATUS_FOVRI	0x02	/* FIFO overrun */
#define SUNI_RXCP_STATUS_HCSI	0x04	/* HCS error */
#define SUNI_RXCP_STATUS_OOCDI	0x10	/* SYNC state */
#define SUNI_RXCP_STATUS_LCDV	0x40	/* loss of cell delineation state */
#define SUNI_RXCP_STATUS_OOCDV	0x80	/* 1-Hunt, 0-SYNC */

/* TXCP Configuration 1 -0x80 */
#define SUNI_TXCP_CFG1_FIFORST	0x01	/* Tx FIFO reset */
#define SUNI_TXCP_CFG1_DSCR	0x02	/* 1-cell payload scrambling disable, 0- enable */
#define SUNI_TXCP_CFG1_HCSADD	0x04	/* 1-polynomial is added, resulting HCS inserted */
#define SUNI_TXCP_CFG1_HCSB	0x08	/* 1-disable insert HCS into transmit cell stream */
#define SUNI_TXCP_CFG1_TCALEVEL0	0x40	/* TCA level 0 control, 1- 26 of 27word, 0-21 of 27word deassert  */
#define SUNI_TXCP_CFG1_TPTYP	0x80	/* 1-TDAT[0:15] even parity, 0-odd parity bit */

/* TXCP Configuration 2 -0x81 */
#define SUNI_TXCP_CFG2_HCSCTLEB	0x01	/* 1-disable HCS control*/
#define SUNI_TXCP_CFG2_DHCS	0x02	/* control insertion of HCS error */
#define SUNI_TXCP_CFG2_FIFODP_MASK	0x0C
#define SUNI_TXCP_CFG2_FIFODP1	0x0C	/* Tx FIFO CLAV deep at 1 cell */
#define SUNI_TXCP_CFG2_FIFODP2	0x08	/* Tx FIFO CLAV deep at 2 cell */
#define SUNI_TXCP_CFG2_FIFODP3	0x04	/* Tx FIFO CLAV deep at 3 cell */
#define SUNI_TXCP_CFG2_FIFODP4	0x00	/* Tx FIFO CLAV deep at 4 cell */
#define SUNI_TXCP_CFG2_TCAINV	0x10	/* Invert polarity of TCA */

/* CSPI Clock Synthesis Config Register -0x102 */
#define SUNI_CCSC_MODE0		0x01	/* CSU Mode bit 0 */
#define SUNI_CCSC_MODE1		0x02	/* CSU Mode bit 1 */
#define SUNI_CCSC_MODE2		0x04	/* CSU Mode bit 2 */
#define SUNI_CCSC_MODE_VAL	(SUNI_CCSC_MODE2 | SUNI_CCSC_MODE0)	/* Mode[2:0]='101' */

/* The Path Signal Label for ATM Cells */
#define SUNI_PSL_ATM_CELLS	0x13

#define SUNI_IDLE_PATTERN	0x6A	/* idle pattern */

int suni5384_init(struct atm_dev *dev, struct device_node *np,
			int *upc_slot, int *port_width, int *phy_id,
			u32 *line_bitr,	u32 *max_bitr, u32 *min_bitr);
void suni5384_exit(struct atm_dev *dev);

/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Tony Li <tony.li@freescale.com>
 *
 * Description:
 * The FUA driver private data structures
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __FUA_H__
#define __FUA_H__

#include <linux/kernel.h>
#include <asm/types.h>
#include <linux/atmdev.h>
#include <linux/slab.h>
#include <asm/types.h>
#include <asm/immap_qe.h>
#include <asm/qe.h>
//#include <asm/mpc83xx.h>
#include <asm/atomic.h>

//#define	FUA_DEBUG

#ifdef FUA_DEBUG
#define fua_debug(format, args...) \
	printk(KERN_EMERG"%s:"format,__FUNCTION__,##args)
#define	fua_dump(format,args...) \
	printk(format,##args)
#else
#define fua_debug(format, args...)
#define fua_dump(format, args...)
#endif

#define fua_err(format,args...) \
	printk(KERN_ERR format,##args);
#define fua_warning(format,args...) \
	printk(KERN_WARNING format,##args);

/* UCC ATM Event Register */
#define UCCE_ATM_TIRU	0x04000000
#define UCCE_ATM_GRLI	0x02000000
#define UCCE_ATM_GBPG	0x01000000
#define UCCE_ATM_GINT3	0x00800000
#define UCCE_ATM_GINT2	0x00400000
#define UCCE_ATM_GINT1	0x00200000
#define UCCE_ATM_GINT0	0x00100000
#define UCCE_ATM_INTO3	0x00080000
#define UCCE_ATM_INTO2	0x00040000
#define UCCE_ATM_INTO1	0x00020000
#define UCCE_ATM_INTO0	0x00010000

#define UCCM_ATM_TIRU	UCCE_ATM_TIRU
#define UCCM_ATM_GRLI	UCCE_ATM_GRLI
#define UCCM_ATM_GBPG	UCCE_ATM_GBPG
#define UCCM_ATM_GINT3	UCCE_ATM_GINT3
#define UCCM_ATM_GINT2	UCCE_ATM_GINT2
#define UCCM_ATM_GINT1	UCCE_ATM_GINT1
#define UCCM_ATM_GINT0	UCCE_ATM_GINT0
#define UCCM_ATM_INTO3	UCCE_ATM_INTO3
#define UCCM_ATM_INTO2	UCCE_ATM_INTO2
#define UCCM_ATM_INTO1	UCCE_ATM_INTO1
#define UCCM_ATM_INTO0	UCCE_ATM_INTO0

/* ATM MUR paramter table */

/* UCC parameter page in ATM Non Multi-Threading mode */
typedef	struct ucc_para_atm_pg {
	u16	local_pg_para_ptr;
	u16	subpg0_conf_tbl_ptr;
	u16	subpg0_rx_tmp_tbl_ptr;
	u16	subpg0_tx_tmp_tbl_ptr;
	u16	subpg1_conf_tbl_ptr;
	u16	subpg1_rx_tmp_tbl_ptr;
	u16	subpg1_tx_tmp_tbl_ptr;
} __attribute__((packed)) ucc_para_atm_pg_t;

typedef	struct	local_pg_para_tbl {
	u16	int_rct_tmp_ptr;
	u16	ima_tmp;	/* only for IMA */
	u16	rx_tmp;
	u16	rxqd_tmp;	/* only for AAL2 or CES */
	u16	pprs_int_ptr;	/* only for AAL2 or CES */
	u8	res0[2];
	u8	res1[4];
} __attribute__((packed)) local_pg_para_tbl_t;

/* UCC parameter page in ATM Multi-Threading mode */
typedef	struct distributor_para_ram_pg {
	u16	distributor_local_pg_para_ptr;
	u16	subpg0_conf_tbl_ptr;
	u16	subpg0_rx_tmp_tbl_ptr;
	u16	subpg0_tx_tmp_tbl_ptr;
	u16	subpg1_conf_tbl_ptr;
	u16	res0;
	u16	res1;
} __attribute__((packed)) distributor_para_ram_pg_t;

typedef	struct	distributor_local_pg_para_tbl {
	u16	res0;
	u16	res1;
	u16	rx_tmp;
	u16	res2;
	u8	res3[24];
} __attribute__((packed)) distributor_local_pg_para_tbl_t;

typedef	struct thread_para_ram_pg{
	u16	local_pg_para_ptr;
	u16	subpg0_conf_tbl_ptr;
	u16	subpg0_rx_tmp_tbl_ptr;
	u16	subpg0_tx_tmp_tbl_ptr;
	u16	subpg1_conf_tbl_ptr;
	u16	subpg1_rx_tmp_tbl_ptr;
	u16	subpg1_tx_tmp_tbl_ptr;
} __attribute__((packed)) thread_para_ram_pg_t;

typedef struct thread_local_pg_para_tbl {
	u16	int_rct_tmp_ptr;
	u16	res0;
	u8	threadid;	/* automatically allocate by QE whil issuing ATM_ini_MT_UCC command */
	u8	res1;
	u16	rxqd_tmp;	/* for aal2 or ces */
	u16	pprs_int_ptr;	/* for aal2 or ces */
	u8	res2[38];
} __attribute__((packed)) thread_local_pg_para_tbl_t;

typedef	struct	sub_pg0_conf_tbl {
	u16	gbl_atm_para_tbl_ptr;
	u16	oam_ch_rct_ptr;
	u16	imaroot;
	u16	ucc_mode;
	u32	cam_mask;
	u16	gmode;
	u16	int_tcte_tmp_ptr;
	u16	add_comp_lookup_base;
	u16	dyn_add_comp_base;
	u16	rev_timeout_req_period;
	u16	vci_filter;
	u16	apcp_base;
	u16	fbt_base;
	u16	intt_base;
	u16	uni_statt_base;
	/* 0x20 */
	u16	uead_offset;
	u8	res0[2];
	u8	res1[2];
	u8	bd_base_ext;
	u8	res2;
	u32	tcell_tmp_base_ext;	/* only for AAL2 */
	u16	rxqd_base_int;		/* only for E2AAL2 */
	u16	pmt_base;
	u32	aal1_ext_stats_base;
	u16	aal1_dummy_cell_base;
	u16	init_rtcrt_base;	/* for AAL1 */
	u32	ext_rtcrt_base;	/* for AAL1 */
	u16	aal1_snpt_base;
	u16	adapt_threshd_base;	/* for AAL1 */
	/* 0x40 */
	u32	srts_base;		/* only for AAL1 */
	u16	in_cas_blk_base;	/*  for AAL1 */
	u16	out_cas_blk_base;	/* for AAL1 */
	u16	aal1_out_cas_stat_reg;	/*  for AAL1 */
	u8	res3[2];		/* aal1_int_stats_base */
	u16	aal1_int_stats_base;	/* for AAL1 */
	u16	eaal2_pad_tmp_base;
	u32	ras_timer_duration;	/* for E2AAL2 */
	u32	rxqd_base_ext;		/* ?! only for E2AAL2 */
	u32	rx_udc_base;		/* only for aal2 VCs */
	u32	tx_udc_base;
	/* 0x60 */
	u8	rx_term_snum;		/* rxTerminatorSnum only for multi-Threading */
	/* u8	res4; */
	u8	tx_term_snum;		/* txTerminatorSnum */
	u16	com_mth_para_tbl_base;	/* only for multi-Threading */
	u16	mth_term_rx_stat_ptr;	/* only for multi-Threading */
	u16	mth_term_tx_stat_ptr;	/* only for multi-Threading */
	u32	atm_aval_rx_thd_mask;	/* only for multi-Threading */
	u32	atm_aval_tx_thd_mask;	/* only for multi-Threading */
	/* 0x70 */
	u8	res4[16];
} __attribute__((packed)) sub_pg0_conf_tbl_t;
#define VCI_FILTER_VC3	0x1000
#define VCI_FILTER_VC4	0x0800
#define VCI_FILTER_VC6	0x0200
#define VCI_FILTER_VC7	0x0100
#define VCI_FILTER_VC8	0x0080
#define VCI_FILTER_VC9	0x0040
#define VCI_FILTER_VC10	0x0020
#define VCI_FILTER_VC11	0x0010
#define VCI_FILTER_VC12	0x0008
#define VCI_FILTER_VC13	0x0004
#define VCI_FILTER_VC14	0x0002
#define VCI_FILTER_VC15	0x0001

#define GMODE_GBL	0x2000
#define GMODE_ALB	0x0200
#define GMODE_CTB	0x0100
#define GMODE_REM	0x0080
#define GMODe_MSP	0x0040
#define GMODE_IMA_EN	0x0020
#define GMODE_UEAD	0x0010
#define GMODE_CUAB	0x0008
#define GMODE_EVPT	0x0004
#define GMODE_ALM_ADD_COMP	0x0001

#define UCC_MODE_MULTI_THREAD_EN	0x8000
#define UCC_MODE_TSR_SEL	0x2000
#define UCC_MODE_DYN_AC_EN	0x0400
#define UCC_MODE_UID		0x0300
#define UCC_MODE_VPSW_EN	0x0080
#define UCC_MODE_NPL	0x0040
#define UCC_MODE_MCAL	0x0020

typedef	struct	gbl_atm_para_tbl {
	u16	int_rct_base;
	u16	int_tct_base;
	u32	ext_rct_base;
	u32	ext_tct_base;
	u32	ext_tcte_base;
	u16	int_tcte_base;
	u16	com_info_ctl;
	u16	com_info_cc;
	u16	com_info_bt;
	u32	com_sus_tmp;
	u16	com_lk_tmp;
	u16	atm_avcon_base;	/* for AAL2 */
	u8	res[16];
} __attribute__((packed)) gbl_atm_para_tbl_t;
#define ACT_OTH	0x0
#define ACT_VBR	0x1
#define ACT_GBR_UBR	0x2
#define ACT_HF	0x3

typedef struct comm_mth_para_tbl {
	u16	atm_threads_tbl_base;
	u16	atm_thread_cam_base;
	u32	atm_thread_empty_status;
	u8	atm_thread_cam_size;
} __attribute__((packed)) comm_mth_para_tbl_t;

typedef struct thread_entry {
	u8	snum;
	u8	res;
	u16	page;
} __attribute__((packed)) thread_entry_t;

typedef	struct	add_lookup_tbl {
	u32	vpt_base;
	u32	vct_base;
	u8	vclt_sf;
	u8	vp_lvl_mask[3];
} __attribute__((packed)) add_lookup_tbl_t;

typedef	struct vplt_entry {
	u16	vc_mask;
	u16	vcoffset;
} __attribute__((packed)) vplt_entry_t;

typedef	u32	vclt_entry;
#define VCLT_MS	0x80000000

#define MK_VCLT_ENTRY(pid,cc) \
		((pid & 0x 3f) << 0x10) | \
		(cc & 0xffff)

typedef	struct	sub_pg1_conf_tbl {
	u32	txqd_ext_base;	/* for AAL2 */
	u32	tx_cid_stat_addr_base;	/* for AAL2 */
	u16	tx_cid_stat_offset_base;	/* for AAL2 */
	u8	res0[2];
	u32	pprs_ext_base;	/* for AAL2 */
	u32	ext_upc_base;
	u16	int_upc_base;
	u16	fs_base;	/* for MSP */
	u16	qlts_base;	/* for MSP */
	u16	fdtp_base;	/* for MSP */
	u16	fcp_base;	/* for MSP */
	u16	ins_cell_tmp_ptr;	/* for MSP */
	u16	mce_base;	/* for MSP */
	u16	feptr_base;	/* for MSP */
	u32	msp_upc_base;	/* for MSP */
	u32	ext_ins_cell_base;	/* for MSP */
	u16	int_ins_cell_base;	/* for MSP */
	u8	ins_cell_header_loc;	/* for MSP */
	u8	res1;
	u16	aal2_threshold_tbl_base;	/*  for E2AAL2 */
	u8	res2[14];		/* u16 res[14] in netcomm?? */
} __attribute__((packed)) sub_pg1_conf_tbl_t;

/* data structure and operation for RCT/TCT */
typedef	struct	aal5_rct {
	u16	tml;
	u32	rx_crc;
	u16	rbdcnt;
	u16	res;
	u16	attr;
} __attribute__((packed)) aal5_rct_t;
#define AAL5_RCT_RXBM	0x0080
#define AAL5_RCT_RXFM	0x0040

typedef	struct	rct_entry {
	u32	attr;
	u32	rxdbptr;
	u16	cts_h;
	u16	cts_l;
	u16	rbd_offset;
	aal5_rct_t	aal5;
	u16	mrblr;
	u16	pmt_rbd_base;
	u16	rbd_base_pm;
} __attribute__((packed)) rct_entry_t;
#define RCT_ENTRY_ATTR_GBL	0x20000000
#define RCT_ENTRY_ATTR_CETM	0x04000000
#define RCT_ENTRY_ATTR_DTB	0x02000000
#define RCT_ENTRY_ATTR_BDB	0x01000000
#define RCT_ENTRY_ATTR_BUFM	0x00400000
#define RCT_ENTRY_ATTR_SEGF	0x00200000
#define RCT_ENTRY_ATTR_ENDF	0x00100000
#define RCT_ENTRY_ATTR_INF	0x00004000
#define RCT_SET_BO(rct) \
		setbits32(&rct->attr, 0x10000000)
#define RCT_SET_INTQ(rct,intq) \
	clrbits32(&rct->attr, 0x30000); \
	setbits32(&rct->attr, intq << 16)
#define RCT_SET_AAL(rct,aal) \
	clrbits32(&rct->attr, 7); \
	setbits32(&rct->attr, aal);
#define IS_RCT_IDEL(rct) \
		~(in_be32(&rct->attr) & RCT_ENTRY_ATTR_INF)
#define RCT_SET_PMT(rct,pmt) \
		setbits16(&rct->pmt_rbd_base, (pmt << 8))
#define RCT_SET_RBD_BASE(rct,rbd_base) \
		setbits16(&rct->pmt_rbd_base, ((u32)rbd_base >> 16) & 0xff); \
		setbits16(&rct->rbd_base_pm, (u32)rbd_base & 0xfff0)
#define RCT_GET_RBD_BASE(bd_base_ext,rct) \
		(bd_base_ext << 24) | \
		((in_be32(&rct->pmt_rbd_base) & 0xff) << 16) | \
		(in_be32(&rct->rbd_base_pm) & 0xfff0)

typedef struct	aal5_tct {
	u32	tx_crc;
	u16	tml;
} __attribute__((packed)) aal5_tct_t;

typedef struct	tct_entry {
	u32	attr;
	u32	txdbptr;
	u16	tbdcnt;
	u16	tbd_offset;
	u32	rate;
	aal5_tct_t	aal5;
	u16	apclc;
	u32	cell_header;
	u16	pmt_tbd_base;
	u16	tbd_base_oths;
} __attribute__((packed)) tct_entry_t;
#define TCT_ENTRY_ATTR_GBL	0x20000000
#define TCT_ENTRY_ATTR_AVCF	0x00800000
#define TCT_ENTRY_ATTR_VCON	0x00040000
#define TCT_ENTRY_ATTR_CPUU	0x00080000
#define TCT_ENTRY_ATTR_INF	0x00004000
#define TCT_SET_BO(tct) \
		setbits32(&tct->attr, 0x10000000)
#define PCR	0x00000000
#define VBR	0x00100000
#define UBRPLUS	0x00200000
#define GBR	0x00300000
#define TCT_SET_ATT(tct,type) \
	do { \
		clrbits32(&tct->attr, 0x300000); \
		setbits32(&tct->attr, type); \
	} while (0)
#define IS_VCON(tct) \
	in_be32(&tct->attr) & TCT_ENTRY_ATTR_VCON
#define TCT_SET_INTQ(tct,intq) \
	do { \
		clrbits32(&tct->attr, 0x30000); \
		setbits32(&tct->attr, intq << 16); \
	} while(0)
#define IS_TCT_IDEL(tct) \
		~(in_be32(&tct->attr) & TCT_ENTRY_ATTR_INF)
#define AAL0	0
#define AAL1	1
#define AAL5	2
#define AAL2	4
#define AAL1_CES	5
#define MSP	6
#define TCT_SET_AAL(tct,aal) \
	do { \
		clrbits32(&tct->attr, 7); \
		setbits32(&tct->attr, aal); \
	} while (0)
#define TCT_SET_PCR_FRACTION(tct,fraction) \
	setbits32(&tct->rate, (fraction << 16) & 0xff0000);
#define TCT_SET_PCR(tct,pcr) \
		setbits32(&tct->rate, (pcr & 0xffff))
#define MK_CELL_HEADER(tct,vpi,vci,pti,clp) \
	do { \
		u32 val; \
		val = ((vpi << 20) & 0xff00000) | \
				((vci << 4) & 0xffff0) | \
				((pti << 1) & 0xe) | \
				(clp & 0x1 ); \
		out_be32(&tct->cell_header, val); \
	} while (0)
#define TCT_SET_PMT(tct,pmt) \
		setbits16(&tct->pmt_tbd_base, (pmt << 8))
#define TCT_SET_TBD_BASE(tct,tbd_base) \
		setbits16(&tct->pmt_tbd_base, ((u32)tbd_base >> 16) & 0xff); \
		setbits16(&tct->tbd_base_oths, (u32)tbd_base & 0xfff0)
#define TCT_STP(tct) \
		setbits16(&tct->tbd_base_oths, 0x4)
#define TCT_GET_TBD_BASE(bd_base_ext,tct) \
		(bd_base_ext << 24) | \
		((in_be32(&tct->pmt_tbd_base) & 0xff) << 16) | \
		(in_be32(&tct->tbd_base_oths) & 0xfff0)

#define BD_BASE_ALIGN	0xF /* bd queue head must aligned with 16 bytes */

/* APC data structure */
typedef struct	apc_para_tbl {
	u16	apcl_first;
	u16	apcl_last;
	u16	apcl_ptr;
	u8	cps;
	u8	cps_cnt;
	u8	max_iteration;
	u8	res0;
	u16	first_ubrplus_level;
	u32	real_tstp;
	u32	apc_state;
	u8	res1[4];
	u32	apc_slot_dur_val_int;
	u16	apc_slot_dur_frac;
	u16	scheduler_mode;
} __attribute__((packed)) apc_para_tbl_t;
#define SCHEDULER_MODE_AFM	0x8000
#define SCHEDULER_MODE_AFC	0x4000
#define SCHEDULER_MODE_PUBRPLUS	0x1000
#define SCHEDULER_MODE_BURST_INHIBIT	0x0400

typedef struct	apc_prio_tbl {
	u16	apc_levi_base;
	u16	apc_levi_end;
	u16	apc_levi_rptr;
	u16	apc_levi_sptr;
} __attribute__((packed)) apc_prio_tbl_t;

typedef unsigned short	apc_slot;
typedef unsigned short	apc_ctrl_slot;
#define CTRL_SLOT_TCTE	0x8000
#define CTRL_SLOT_SCL_EN	0x4000
#define CTRL_SLOT_EXT_CS	0x2000

/* ATM controller BDs */
typedef struct	aal5_bd {
	u16	attr;
	u16	data_length;
	u32	dbptr;
} __attribute__((packed)) aal5_bd_t;
#define AAL5_RXBD_ATTR_E	0x8000
#define AAL5_RXBD_ATTR_W	0x2000
#define AAL5_RXBD_ATTR_I	0x1000
#define AAL5_RXBD_ATTR_L	0x0800
#define AAL5_RXBD_ATTR_F	0x0400
#define AAL5_RXBD_ATTR_CM	0x0200
#define AAL5_RXBD_ATTR_PNC	0x0080
#define AAL5_RXBD_ATTR_CLP	0x0020
#define AAL5_RXBD_ATTR_CNG	0x0010
#define AAL5_RXBD_ATTR_ABRT	0x0008
#define AAL5_RXBD_ATTR_CPUU	0x0004
#define AAL5_RXBD_ATTR_LNE	0x0002
#define AAL5_RXBD_ATTR_CRE	0x0001

#define AAL5_TXBD_ATTR_R	0x8000
#define AAL5_TXBD_ATTR_W	0x2000
#define AAL5_TXBD_ATTR_I	0x1000
#define AAL5_TXBD_ATTR_L	0x0800
#define AAL5_TXBD_ATTR_CM	0x0200
#define AAL5_TXBD_ATTR_CLP	0x0020
#define AAL5_TXBD_ATTR_CNG	0x0010

/* UNI statistics data structure */
typedef struct	uni_stat_tbl {
	u16	utopiae;
	u16	mic_count;
} __attribute__((packed)) uni_stat_tbl_t;

/* ATM exceptions in Non multi-thread mode */
typedef struct	intr_que_para_tbl {
	u32	intq_base;
	u32	intq_ptr;
	u16	int_cnt;
	u16	int_icnt;
	u32	intq_entry;
} __attribute__((packed)) intr_que_para_tbl_t;

/* ATM exceptions in multi-thread mode */
typedef struct	intr_que_para_mth_tbl {
	u32	intq_base;
	u16	intq_offset_out;
	u16	intq_offset_in;
	u16	int_cnt;
	u16	int_icnt;
	u16	res;
	u16	intq_size;
} __attribute__((packed)) intr_que_para_mth_tbl_t;

/* Non UPC-Policer */
typedef struct	intr_que_entry {
	u16	attr;
	u16	channel_code;
} __attribute__((packed)) intr_que_entry_t;
#define INT_QUE_ENT_ATTR_V	0x8000
#define INT_QUE_ENT_ATTR_UPC	0x4000
#define INT_QUE_ENT_ATTR_W	0x2000
#define INT_QUE_ENT_ATTR_TBNR	0x0010
#define INT_QUE_ENT_ATTR_RXF	0x0008
#define INT_QUE_ENT_ATTR_BSY	0x0004
#define INT_QUE_ENT_ATTR_TXB	0x0002
#define INT_QUE_ENT_ATTR_RXB	0x0001

/**************************************************************************************/
#define UCC_ATM_URFS_INIT	0x100
#define UCC_ATM_URFET_INIT	0x80
#define UCC_ATM_URFSET_INIT	0xc0
#define UCC_ATM_UTFS_INIT	0x100
#define UCC_ATM_UTFET_INIT	0x80
#define UCC_ATM_UTFTT_INIT	0x40


#define MAX_PHY_NUMBER	4
#define MAX_DEVICE_NUMBER	MAX_PHY_NUMBER
#define MAX_PHY_LINEBITR        155000000
#define MIN_PHY_LINEBITR        50000000

#define MAX_QUE_NUMBER 4

#define MAX_VPI_BITS	0
#define MAX_VCI_BITS	8

#define MAX_INTERNAL_CHANNEL_CODE	255
#define MAX_INTERNAL_RCT	MAX_INTERNAL_CHANNEL_CODE
#define MAX_INTERNAL_TCT	MAX_INTERNAL_CHANNEL_CODE
#define MAX_RCT_NUMBER	MAX_INTERNAL_RCT
#define MAX_TCT_NUMBER	MAX_INTERNAL_TCT

#define VP_MASK_LEN	22
#define VC_MASK_LEN	16

/* #define LOOPBACK */
enum traffic_type {
	FUA_NONE = 0,
	FUA_UBR = 1,
	FUA_CBR = 2,
	FUA_VBR = 3,
	FUA_ABR = 4,
	FUA_ANY = 5,
};

/*	For BD management and allocation	*/
typedef struct	bd_pool {
	spinlock_t	lock;
	int size; /* allocated mem size */
	int number; /* The number of bd. size maybe not the multiple of sizeof(struct qe_bd) */
	int frees; /* current free bd number */
	unsigned char *occupied;	/* bitmap */
	struct qe_bd	*head;
} bd_pool_t;

struct fua_vcc {
	int status;
	int aal;
	struct atm_vcc *vcc;

	/* Receive channel */
	int rx_intq_num;
	int rx_cc;
	rct_entry_t *rct;
	struct qe_bd *rxbase, *rxcur, *first;
	int rbuf_size;
	struct sk_buff_head rx_list;
	/* Transmit channel */
	int avcf;
	int traffic_type;
	int tx_intq_num;
	int tx_cc;
	tct_entry_t *tct;
	struct qe_bd *txbase, *txcur;
	struct sk_buff_head tx_list,pop_list;

	struct list_head list;
};

struct phy_info {
	int upc_slot;
	int phy_id;
	int port_width;
	int link_rate;
	int prio_level;
	u8 max_iteration; /* For APC */
	u32 line_bitr;
	u32 max_bitr;
	u32 min_bitr;
	u16 scheduler_mode; /* For APC */
};

struct fua_device {
	int u_id; /* utopia bus id */
	struct atm_dev *dev; /* point back */
	struct phy_info *phy_info;
	u8 cps;
	struct list_head vcc_list;
	struct list_head list;   /* linked into fua_priv structure */
	atomic_t refcnt;
	struct fua_private *fua_priv;	/* Back pointer to */
};

#define MAX_MTH_INTR_QUE_SIZE	0x4000

union intr_que {
	struct intr_que_para_tbl *base;
	struct intr_que_para_mth_tbl *mth_base;
};

struct fua_private{
	struct ucc_fast_private *uccf;
	struct fua_info *fua_info;

	spinlock_t lock; /* To protect interrupt handle */
	struct tasklet_struct task;
	u32 intr_event;

	u32 subblock;

	u16 oam_ch_rct_ptr; /* pointer to the oam rct in MUR! !must be 1 according to UM?? Should del in the future. */

	union intr_que intr_que; /* [MAX_QUE_NUMBER] */
	u16 intr_que_offset;
	intr_que_entry_t *intrcur[MAX_QUE_NUMBER]; /* The read pointer for non-mth mode interrupt queue. */

	/* BD pool management */
	bd_pool_t bd_pool;

	int vplt_size;
	int vclt_size;

	/* Connection Table Base (CSB addr) */
	u32 int_rct_base;
	u16 int_rct_offset;
	u32 ext_rct_base;
	u32 int_tct_base;
	u16 int_tct_offset;
	u32 ext_tct_base;
	u32 int_tcte_base;
	u16 int_tcte_offset;
	u32 ext_tcte_base;

	/* Parameter Table in MUR (CSB addr) */
	ucc_para_atm_pg_t *ucc_para_pg; /* After allocating it,issue "Assign page to device" command */
	u16 ucc_para_pg_offset;
	add_lookup_tbl_t *addr_tbl;
	u16 addr_tbl_offset;
	apc_para_tbl_t *apc_tbl_base;
	u16 apc_tbl_offset;
	uni_stat_tbl_t *uni_stat_base;
	u16 uni_stat_offset;

	/* Channel code managemnet */
	struct atm_vcc **rx_vcc; /* Received Channel code to VCC */
	struct atm_vcc **tx_vcc; /* For tx channel code management */

	u8 cps[MAX_PHY_NUMBER]; //???

	/* statistics */
	int interrupts;
	struct list_head dev_list;
};

struct fua_info {
	int mthmode;
	int threads;
	int link_rate; /* ATM OC3 PCR */
	int phy_last; /* The last phy_id number */
	int max_thread; /* The number of thead this driver can used. 1 <= threads <= 32; */
	int max_channel; /* The maximum number of channel this driver can used. */
	int max_bd; /* Number of bd in bd_pool */
	int bd_per_channel;
	int max_intr_que; /* The maximum number of interrupt queue */
	int intr_ent_per_que; /* The number of entries per interrupt queue */
	int intr_threshold; /* The number of interupts required before QE issue a global interrupt. */
	u16 vci_filter;
	u16 vc_mask; /* All vp level table entry has the same vc_mask. */
	u32 vpc_info; /* The frist byte is vclt_sf. The last three btyes are vp_lvl_mask. */
	int vct_exp; /* The exponent of vc level table size vct_sie = PAGE_SIZE * 2^exponent */
	struct ucc_fast_info uf_info;
	struct upc_slot_tx *upc_slot_tx[UPC_SLOT_MAX_NUM];
	struct upc_slot_rx *upc_slot_rx[UPC_SLOT_MAX_NUM];
};

#define INT_RX_QUE	INT_QUE_1
#define INT_TX_QUE	INT_QUE_2
#define INT_QUE_0	0
#define INT_QUE_1	1
#define INT_QUE_2	2
#define INT_QUE_3	3
#define MAX_QUE_NUMBER	4

static inline struct qe_bd * bd_get_next(struct qe_bd *cur, struct qe_bd *base, const int flag)
{
	return (cur->status & flag) ? base : ++cur;
}

#endif	/* __FSL_ATM_H__ */

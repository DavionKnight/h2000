/*
 * Copyright (C) 2007-2010 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Tony Li <tony.li@freescale.com>
 *         Dave Liu <DaveLiu@freescale.com>
 *
 * Description:
 * The FUA driver. This driver supports Freescale QUICC Engine UCC ATM mode.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __KERNEL__
#define __KERNEL__
#endif

#define	EXPORT_SYNTAB

#ifdef MODULE
#include <linux/module.h>
#endif

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/sonet.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/uio.h>
#include <linux/bitops.h>
#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>

#include <asm/dma-mapping.h>
#include <asm/cacheflush.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <asm/string.h>
#include <asm/byteorder.h>
#include <asm/delay.h>
#include <asm/ucc_fast.h>
#include <asm/upc.h>
#include <asm/qe.h>

#include "fua.h"
#include "pm5384.h"

#ifdef MODULE
#warning "ATM work as module"
#endif

#define DRV_NAME "fua"

struct fua_info fua_primary_info = {
	.max_thread = 0x10,
	.max_channel = 0x200,
	.max_bd = 0x1000,
	.bd_per_channel = 0x100,
	.max_intr_que = 0x4,
	.intr_ent_per_que = 0x16,
	.intr_threshold = 1,
	.vci_filter = 0x1BFF,
	.vc_mask = 0xFF, /*just 256 items for one entry in vp level table */
	.vpc_info = 0x0000F000, /* enable phy_id */
	.uf_info = {
			.bd_mem_part = MEM_PART_SYSTEM,
			.max_rx_buf_length = 1536, // Seems no use
			.uccm_mask = 0xFFFFFFFF,
			/* adjusted at startup if max-speed 1000 */
			.urfs = UCC_ATM_URFS_INIT,
			.urfet = UCC_ATM_URFET_INIT,
			.urfset = UCC_ATM_URFSET_INIT,
			.utfs = UCC_ATM_UTFS_INIT,
			.utfet = UCC_ATM_UTFET_INIT,
			.utftt = UCC_ATM_UTFTT_INIT,
			.ufpt = 256,
			.mode = UCC_FAST_PROTOCOL_MODE_ATM,
			.ttx_trx = UCC_FAST_GUMR_TRANSPARENT_TTX_TRX_NORMAL,
			.tenc = UCC_FAST_TX_ENCODING_NRZ,
			.renc = UCC_FAST_RX_ENCODING_NRZ,
			.tcrc = UCC_FAST_16_BIT_CRC,
			.synl = UCC_FAST_SYNC_LEN_NOT_USED,
	},
};

/*************************parameter page table handle routine****************/
void dump_addrlut(struct add_lookup_tbl *lut)
{
	fua_dump("address lookup table locates at: %p\n", lut);
	fua_dump("\tvpt_base: ox%x\n", lut->vpt_base);
	fua_dump("\tvct_base: ox%x\n", lut->vct_base);
	fua_dump("\tvclt_sf: ox%x\n", *(u32 *) & lut->vclt_sf);
}

/*************************************
 *	Address lookup operation routine
 * Now only support
 *	vpi range 0 -- 0
 *	vci range 0 -- 65535
 * only vp level table are established.
 * vc level table is allocated and
 * all item are identified as invalid
 ************************************/
int addr_comp_lookup_init(add_lookup_tbl_t __iomem *addr_tbl,
				u32 vpc_info, u16 vc_mask, int *vct_exp)
{
	int i;
	int vpt_size, vct_size;
	int all_vc, exponent, factor;
	u8 vclt_sf;
	u16 vcoffset;
	u32 *vc_current, vp_lvl_mask;
	u32 vpt_base;
	unsigned long vct_base;
	vplt_entry_t *vplt_entry;

	vclt_sf = (vpc_info & 0xFF000000) >> 24;
	vp_lvl_mask = vpc_info & 0x00FFFFFF;

	/* calculate vp table size */
	vpt_size = 1;
	for (i = 0; i < VP_MASK_LEN; i++, vp_lvl_mask >>= 1) {
		if (vp_lvl_mask & 0x1)
			vpt_size <<= 1;
	}
	/* entry in vp table is 4 bytes */
	vpt_base = qe_muram_alloc((vpt_size * 4), 4);
	if (IS_ERR_VALUE(vpt_base)) {
		fua_dump("vp table alloc failed\n");
		goto out;
	}
	_memset_io((void *)qe_muram_addr(vpt_base), 0x0, vpt_size * 4);
	/* calculate the size of vc table */
	vct_size = 1;
	factor = vc_mask;
	for (i = 0; i < VC_MASK_LEN; i++, factor >>= 1) {
		if (factor & 0x1)
			vct_size <<= 1;
	}

	/* factor = 2^(2 + vclt_sf) */
	factor = 1;
	for (i = 1; i <= vclt_sf; i++ )
		factor <<= 1;
	factor *= 4;

	all_vc = vpt_size * vct_size * factor;
	all_vc = (all_vc + (PAGE_SIZE - 1)) / PAGE_SIZE;
	for (exponent = 0; all_vc > (1 << exponent); exponent++) ;
	vct_base = (u32) __get_free_pages(GFP_KERNEL, exponent);
	if (!vct_base) {
		fua_dump("vc table alloc failed need 0x%x\n",
				vct_size * vpt_size * factor);
		goto out1;
	}

	/* initiate vp and vc level addr table */
	vcoffset = 0;
	vc_current = (u32 *) vct_base;
	vplt_entry = (vplt_entry_t *)qe_muram_addr(vpt_base);
	while (vpt_size > 0) {
		out_be16(&vplt_entry->vc_mask, vc_mask);
		out_be16(&vplt_entry->vcoffset, vcoffset);

		/* All vclt entry is not match in default */
		for (i = 0; i < vct_size; i++) {
			out_be32(vc_current, VCLT_MS);
			vc_current++;
			vcoffset++;
		}

		vplt_entry++;
		vpt_size--;
	}

	out_be32(&addr_tbl->vpt_base, vpt_base);
	out_be32(&addr_tbl->vct_base, virt_to_phys((void *)vct_base));
	out_be32((u32 *)&addr_tbl->vclt_sf, vpc_info);

	*vct_exp = exponent;
	return 0;
out1:
	qe_muram_free(vpt_base);
out:
	return -ENOMEM;
}

/*
 * Before exit, the receive should be stopped
 */
void addr_comp_lookup_exit(add_lookup_tbl_t __iomem *addr_tbl, int vct_exp)
{
	u32 vpt_base;
	unsigned long vct_base;

	vpt_base = in_be32(&addr_tbl->vpt_base);
	vct_base = (unsigned long)in_be32(&addr_tbl->vct_base);

	free_pages((unsigned long)phys_to_virt(vct_base), vct_exp);
	qe_muram_free(vpt_base);
	qe_muram_free((uint)addr_tbl);

	return;
}

/*****************************************************
 * Input:
 *		addr_tbl:
 *		uid: utopia bus id (2 bits)
 *		phy_id : phy address in utopia bus (8 bits)
 *		vpi/vci: (12 bits/16 bits)
 *		channel_code: (16 bits)
 *		pid: UPC policer id (10 bits)
 *****************************************************/
static int addr_comp_lookup_add(add_lookup_tbl_t *addr_tbl, int uid, int phy_id,
			 short vpi, int vci, u16 channel_code, u16 pid)
{
	int i, j, factor;
	u32 *vc_current, vp_value;
	u32 vpt_base;
	unsigned long vct_base;
	u32 vp_lvl_mask;
	u32 offset;
	u16 vc_mask, vc_offset;
	u32 mask, value;
	u8 vclt_sf;
	vplt_entry_t *vplt_entry;

	vpt_base = (u32)qe_muram_addr(in_be32(&addr_tbl->vpt_base));
	vct_base = (unsigned long)phys_to_virt((unsigned long)(in_be32(&addr_tbl->vct_base)));

	vclt_sf = in_8(&addr_tbl->vclt_sf);
	vp_lvl_mask = in_be32((u32 *)&addr_tbl->vclt_sf) & 0x00FFFFFF;
	vp_value = ((uid & 0x3) << 20) | ((phy_id & 0xFF) << 12) | (vpi & 0xFFF);

	offset = i = j = 0;
	for (i = 0; i < VP_MASK_LEN; i++) {
		mask = vp_lvl_mask >> i;
		value = vp_value >> i;
		if (mask & 1) {
			offset += (value & 1) << j;
			j++;
		}
	}
	vplt_entry = (vplt_entry_t *)(vpt_base + offset * 4);
	vc_mask = in_be16(&vplt_entry->vc_mask);
	vc_offset = in_be16(&vplt_entry->vcoffset);

	offset = i = j = 0;
	for (i = 0; i < VC_MASK_LEN; i++) {
		mask = vc_mask >> i;
		value = vci >> i;
		if (mask & 1) {
			offset += (value & 1) << j;
			j++;
		}
	}
	factor = 1;
	for (i = 1; i <= vclt_sf; i++)
		factor <<= 1;
	factor *= 4;
	vc_current = (u32 *)(vct_base + vc_offset * factor + offset * 4);

	out_be32(vc_current, ((pid & 0x3f) << 16) | channel_code);

	return 0;
}

static int addr_comp_lookup_remove(add_lookup_tbl_t *addr_tbl, int uid,
				int phy_id, short vpi, int vci)
{
	int i, j, factor;
	u32 *vc_current, vp_value;
	u32 vpt_base;
	unsigned long vct_base;
	u32 vp_lvl_mask;
	u32 offset;
	u16 vc_mask, vc_offset;
	u8 vclt_sf;
	vplt_entry_t *vplt_entry;

	vpt_base = (u32)qe_muram_addr(in_be32(&addr_tbl->vpt_base));
	vct_base = (unsigned long)phys_to_virt(in_be32(&addr_tbl->vct_base));

	vclt_sf = in_8(&addr_tbl->vclt_sf);
	vp_lvl_mask = in_be32((u32 *)&addr_tbl->vclt_sf) & 0x00FFFFFF;
	vp_value = ((uid & 0x3) << 20) | ((phy_id & 0xFF) << 12) | (vpi & 0xFFF);
	offset = i = j = 0;
	for (i = 0; i < VP_MASK_LEN; i++) {
		vp_lvl_mask >>= i;
		vp_value >>= i;
		if (vp_lvl_mask & 1) {
			offset += (vp_value & 1) << j;
			j++;
		}
	}
	vplt_entry = (vplt_entry_t *) (vpt_base + offset * 4);
	vc_mask = in_be16(&vplt_entry->vc_mask);
	vc_offset = in_be16(&vplt_entry->vcoffset);

	offset = i = j = 0;
	for (i = 0; i < VC_MASK_LEN; i++) {
		vc_mask >>= i;
		vci >>= i;
		if (vc_mask & 1) {
			offset += (vci & 1) << j;
			j++;
		}
	}
	factor = 1;
	for (i = 1; i <= vclt_sf; i++)
		factor <<= 1;
	factor *= 4;
	vc_current = (u32 *) (vct_base + vc_offset * factor + offset * 4);

	out_be32(vc_current, VCLT_MS);
	return 0;
}

/************************************************************************************
 * All the table reside in MUR
 * The pointers in prio_tbl should be aligned with half-word(2 bytes)
 ************************************************************************************/
#if 1
void dump_apc(apc_para_tbl_t * apc_phy)
{
	apc_prio_tbl_t *prio, *prio_last;
	apc_slot *slot, *slot_end;
	int i, j;
	u16 offset;

	fua_dump("apc at %p\n", apc_phy);
	fua_dump("\tapcl_first: 0x%x at %p\n",
			apc_phy->apcl_first, &apc_phy->apcl_first);
	fua_dump("\tapcl_last: 0x%x at %p\n",
			apc_phy->apcl_last, &apc_phy->apcl_last);
	fua_dump("\tapcl_ptr: 0x%x at %p\n",
			apc_phy->apcl_ptr, &apc_phy->apcl_ptr);
	fua_dump("\tcps: 0x%x at %p\n", apc_phy->cps, &apc_phy->cps);
	fua_dump("\tcps_cnt: 0x%x at %p\n",
			apc_phy->cps_cnt, &apc_phy->cps_cnt);
	fua_dump("\tmax_iteration: 0x%x at %p\n",
			apc_phy->max_iteration, &apc_phy->max_iteration);
	fua_dump("\tfirst_ubrplus_level: 0x%x at %p\n",
			apc_phy->first_ubrplus_level,
			&apc_phy->first_ubrplus_level);
	fua_dump("\treal_tstp: 0x%x at %p\n",
			apc_phy->real_tstp, &apc_phy->real_tstp);
	fua_dump("\tapc_state: 0x%x at %p\n",
			apc_phy->apc_state, &apc_phy->apc_state);
	fua_dump("\tapc_slot_dur_val_int: 0x%x at %p\n",
			apc_phy->apc_slot_dur_val_int,
			&apc_phy->apc_slot_dur_val_int);
	fua_dump("\tapc_slot_dur_frac: 0x%x at %p\n",
			apc_phy->apc_slot_dur_frac,
			&apc_phy->apc_slot_dur_frac);
	fua_dump("\tschdeuler_mode: 0x%x at %p\n",
			apc_phy->scheduler_mode,
			&apc_phy->scheduler_mode);

	i = 0;
	offset = in_be16(&apc_phy->apcl_first);
	prio = (apc_prio_tbl_t *)qe_muram_addr(offset);
	offset = in_be16(&apc_phy->apcl_last);
	prio_last = (apc_prio_tbl_t *)qe_muram_addr(offset);
	while (prio <= prio_last) {
		fua_dump("prio %d at %p\n", i, prio);
		fua_dump("\tapc_levi_base: 0x%x at %p\n",
				prio->apc_levi_base, &prio->apc_levi_base);
		fua_dump("\tapc_levi_end: 0x%x at %p\n",
				prio->apc_levi_end, &prio->apc_levi_end);
		fua_dump("\tapc_levi_rptr: 0x%x at %p\n",
				prio->apc_levi_rptr, &prio->apc_levi_rptr);
		fua_dump("\tapc_levi_sptr: 0x%x at %p\n",
				prio->apc_levi_sptr, &prio->apc_levi_sptr);

		j = 0;
		offset = in_be16(&prio->apc_levi_base);
		slot = (apc_slot *)qe_muram_addr(offset);
		offset = in_be16(&prio->apc_levi_end);
		slot_end = (apc_slot *)qe_muram_addr(offset);
		while (slot <= slot_end) {
			fua_dump("\t\tslot %d: 0x%x at %p\n",
					j, *slot, slot);
			slot++;
			j++;
		}
		fua_dump("\t\tcontrol slot: 0x%x at %p\n", *slot, slot);
		prio++;
		i++;
	}
}
#endif

int apc_prio_init(apc_prio_tbl_t * prio, int slot_cnt)
{
	int i;
	u32 offset;
	apc_slot *slot;

	/* add 1 for control slot */
	offset = qe_muram_alloc(sizeof(apc_slot) * (slot_cnt + 1),
					sizeof(apc_slot));
	if (IS_ERR_VALUE(offset)) {
		return -ENOMEM;
	}
	_memset_io((void *)qe_muram_addr(offset),
			 0x0, sizeof(apc_slot) * (slot_cnt + 1));

	out_be16(&prio->apc_levi_base, (u16)offset);

	/* point to the beginning of the last slot exclude control slot */
	out_be16(&prio->apc_levi_end,
			offset + sizeof(apc_slot) * (slot_cnt - 1));
	out_be16(&prio->apc_levi_rptr, offset);
	out_be16(&prio->apc_levi_sptr, offset);

	slot = (apc_slot *) qe_muram_addr(offset);
	for (i = 0; i < slot_cnt; i++) {
		out_be16(slot, 0);
		slot++;
	}
	/* the control slot */
	out_be16(slot, 0);
	return 0;
}

void apc_prio_exit(apc_prio_tbl_t * prio)
{
	qe_muram_free(in_be16(&prio->apc_levi_base));
}

void apc_exit(apc_para_tbl_t *apc_tbl_base, struct phy_info *phy_info);

int apc_init(apc_para_tbl_t *apc_tbl, struct phy_info *phy_info,
		u8 *cell_per_slot, int type)
{
	int i;
	u8 cps;
	unsigned long offset;
	int slot;
	apc_prio_tbl_t *prio;

	if ((phy_info->line_bitr > MAX_PHY_LINEBITR)
		|| (phy_info->max_bitr > MAX_PHY_LINEBITR)
		|| (phy_info->line_bitr < phy_info->max_bitr)
		|| (phy_info->max_bitr < phy_info->min_bitr)) {
		return -EINVAL;
	}

	cps = phy_info->line_bitr / phy_info->max_bitr;
	slot = (phy_info->line_bitr / (cps * phy_info->min_bitr)) + 1;

	/* priority table base */
	offset = qe_muram_alloc(sizeof(apc_prio_tbl_t) * phy_info->prio_level,
				sizeof(apc_prio_tbl_t));
	if (IS_ERR_VALUE(offset)) {
		return -ENOMEM;
	}
	_memset_io((void *)qe_muram_addr(offset),
			0x0, sizeof(apc_prio_tbl_t) * phy_info->prio_level);

	out_be16(&apc_tbl->apcl_first, offset);
	out_be16(&apc_tbl->apcl_ptr, offset);
	out_be16(&apc_tbl->apcl_last,
		offset + (phy_info->prio_level - 1) * sizeof(apc_prio_tbl_t));
	out_8(&apc_tbl->cps,cps);
	out_8(&apc_tbl->cps_cnt,cps);
	out_8(&apc_tbl->max_iteration,phy_info->max_iteration);
	out_8(&apc_tbl->res0, 0);
	out_be32(&apc_tbl->real_tstp,0);
	out_be32(&apc_tbl->apc_state,0);
	out_be32((u32 *)&apc_tbl->res1, 0);
	out_be16(&apc_tbl->scheduler_mode,phy_info->scheduler_mode);

	prio = (apc_prio_tbl_t *)qe_muram_addr(offset);
	for (i = 0; i < phy_info->prio_level; i++) {
		if (apc_prio_init(prio, slot))
			goto out;
		prio++;
	}

	*cell_per_slot = cps;
	return 0;
out:
	phy_info->prio_level = i;
	apc_exit(apc_tbl, phy_info);
	return -1;
}

void apc_exit(apc_para_tbl_t *apc_tbl, struct phy_info *phy_info)
{
	int i;
	u32 offset;
	apc_prio_tbl_t *prio;

	offset = in_be16(&apc_tbl->apcl_first);
	prio = (apc_prio_tbl_t *) qe_muram_addr(offset);
	for (i = 0; i < phy_info->prio_level; i++) {
		apc_prio_exit(prio);
		prio++;
	}
	qe_muram_free(offset);
	return;
}

void dump_intq(struct intr_que_para_tbl *queue)
{
	int i;
	intr_que_entry_t *entry;

	fua_dump("que at 0x%p\n", queue);
	fua_dump("\tintq_base: 0x%x at 0x%p\n",
			queue->intq_base, &queue->intq_base);
	fua_dump("\tintq_ptr: 0x%x at 0x%p\n",
			queue->intq_ptr, &queue->intq_ptr);
	fua_dump("\tint_cnt: 0x%x at 0x%p\n",
			queue->int_cnt, &queue->int_cnt);
	fua_dump("\tint_icnt: 0x%x at 0x%p\n",
			queue->int_icnt, &queue->int_icnt);
	fua_dump("\tintq_entry: 0x%x at 0x%p\n",
			queue->intq_entry, &queue->intq_entry);

	entry = bus_to_virt(queue->intq_base);
	entry--;
	i = 0;
	fua_dump("\tentrys:\n");
	do {
		entry++;
		i++;
		fua_dump("\t\t %d attr: 0x%x at 0x%p\n",
				i, entry->attr, &entry->attr);
		fua_dump("\t\t channel_code: 0x%x at 0x%p\n",
				entry->channel_code, &entry->channel_code);
	} while (!(entry->attr & INT_QUE_ENT_ATTR_W));
}

void dump_mth_intq(intr_que_para_mth_tbl_t * mth_queue)
{
	int i;
	intr_que_entry_t *entry;

	fua_dump("que at 0x%p\n", mth_queue);
	fua_dump("\tintq_base: 0x%x at 0x%p\n",
			mth_queue->intq_base, &mth_queue->intq_base);
	fua_dump("\tintq_offset_out: 0x%x at 0x%p\n",
			mth_queue->intq_offset_out,
			&mth_queue->intq_offset_out);
	fua_dump("\tintq_offset_in: 0x%x at 0x%p\n",
			mth_queue->intq_offset_in, &mth_queue->intq_offset_in);
	fua_dump("\tint_cnt: 0x%x at 0x%p\n",
			mth_queue->int_cnt, &mth_queue->int_cnt);
	fua_dump("\tint_icnt: 0x%x at 0x%p\n",
			mth_queue->int_icnt, &mth_queue->int_icnt);
	fua_dump("\tintq_size: 0x%x at 0x%p\n",
			mth_queue->intq_size, &mth_queue->intq_size);

	entry = bus_to_virt(mth_queue->intq_base);
	entry--;
	i = 0;
	fua_dump("\tentrys:\n");
	do {
		entry++;
		i++;
		fua_dump("\t\t %d attr: 0x%x at 0x%p\n",
				i, entry->attr, &entry->attr);
		fua_dump("\t\t channel_code: 0x%x at 0x%p\n",
				entry->channel_code, &entry->channel_code);
	} while (!(entry->attr & INT_QUE_ENT_ATTR_W));
}

int intq_nmth_init(intr_que_para_tbl_t *intr_que, intr_que_entry_t **intcur,
			int intent_cnt, int threshold)
{
	int i, intque_size;
	dma_addr_t addr;
	intr_que_entry_t *entry;

	if (intent_cnt < threshold) {
		return -EINVAL;
	}

	intque_size = intent_cnt * sizeof(intr_que_entry_t);
	entry = (intr_que_entry_t *)
		dma_alloc_coherent(NULL, intque_size, &addr, GFP_DMA);
	if (entry == NULL)
		return -ENOMEM;

	out_be32(&intr_que->intq_base, addr);
	out_be32(&intr_que->intq_ptr, addr);
	out_be16(&intr_que->int_icnt, threshold);
	out_be16(&intr_que->int_cnt, threshold);
	out_be32(&intr_que->intq_entry, addr);
	*intcur = entry;

	for (i = 0; i < intent_cnt; i++) {
		entry->attr = 0;
		entry->channel_code = 0;
		if (i < (intent_cnt - 1))
			entry++;
	}
	entry->attr |= INT_QUE_ENT_ATTR_W;

	return 0;
}

int intq_mth_init(intr_que_para_mth_tbl_t *intr_que,
			int intent_cnt, int threshold)
{
	int i, intque_size;
	dma_addr_t addr;
	intr_que_entry_t *entry;

	if (intent_cnt < threshold) {
		return -EINVAL;
	}

	intque_size = intent_cnt * sizeof(intr_que_entry_t);
	if (intque_size > MAX_MTH_INTR_QUE_SIZE) {
		fua_debug("Exceed the 16K memory boundary \
				limit in multi-thread mode\n");
		return -EINVAL;
	}

	entry = (intr_que_entry_t *)
		dma_alloc_coherent(NULL, intque_size, &addr, GFP_DMA);
	if (entry == NULL)
		return -ENOMEM;

	out_be32(&intr_que->intq_base, addr);
	out_be16(&intr_que->intq_offset_out, 0);
	out_be16(&intr_que->intq_offset_in, 0);
	out_be16(&intr_que->int_icnt, threshold);
	out_be16(&intr_que->int_cnt, threshold);
	out_be16(&intr_que->intq_size, intque_size);

	for (i = 0; i < intent_cnt; i++) {
		entry->attr = 0;
		entry->channel_code = 0;
		if (i < (intent_cnt - 1))
			entry++;
	}
	entry->attr |= INT_QUE_ENT_ATTR_W;

	return 0;
}

void intq_exit(void *intr_que_tbl, int intent_cnt)
{
	int intque_size;
	u32 intq_base;

	intque_size = intent_cnt * sizeof(intr_que_entry_t);
	intq_base = in_be32(intr_que_tbl);

	dma_free_coherent(NULL, intque_size, bus_to_virt(intq_base),
				(dma_addr_t)intq_base);
}

int bd_pool_init(bd_pool_t *bd_pool, int max_bd)
{
	int space_needed, exponent;
	unsigned long flags;

	spin_lock_init(&bd_pool->lock);

	space_needed = max_bd * sizeof(struct qe_bd);
	if (space_needed % PAGE_SIZE)
		space_needed = space_needed / PAGE_SIZE + 1;
	else
		space_needed = space_needed / PAGE_SIZE;
	for (exponent = 0; space_needed > (1 << exponent); exponent++) ;
	spin_lock_irqsave(&bd_pool->lock, flags);
	bd_pool->head = (struct qe_bd *) __get_free_pages(GFP_DMA, exponent);
	if (!bd_pool->head) {
		goto out;
	}

	bd_pool->size = (1 << exponent) * PAGE_SIZE;
	bd_pool->number = bd_pool->size / sizeof(struct qe_bd);

	/* Clear all of free bd */
	memset(bd_pool->head, 0, bd_pool->size);

	bd_pool->occupied = kzalloc(sizeof(bd_pool->occupied[0])
					* bd_pool->number, GFP_KERNEL);
	if (!bd_pool->occupied)
		goto out1;
	bd_pool->frees = bd_pool->number;
	spin_unlock_irqrestore(&bd_pool->lock, flags);
	return 0;
out1:
	free_pages((unsigned long)bd_pool->head, exponent);
out:
	spin_unlock(&bd_pool->lock);
	return -ENOMEM;
}

void bd_pool_exit(bd_pool_t *bd_pool)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&bd_pool->lock, flags);
	kfree(bd_pool->occupied);
	for (i = 0; bd_pool->size > ((1 << i) * PAGE_SIZE); i++) ;
	free_pages((unsigned long)bd_pool->head, i);
	spin_unlock_irqrestore(&bd_pool->lock, flags);
	return;
}

void dump_bd_pool(bd_pool_t * bd_pool, int flag)
{
	fua_dump("bd_pool at 0x%p\n", bd_pool);
	fua_dump("\tsize 0x%x\n", bd_pool->size);
	fua_dump("\tnumber 0x%x\n", bd_pool->number);
	fua_dump("\tfrees 0x%x\n", bd_pool->frees);
}

static struct qe_bd *alloc_bds(bd_pool_t *bd_pool, int number)
{
	int i, j;
	struct qe_bd *head;

	spin_lock(&bd_pool->lock);
	if (bd_pool->frees < number) {
		spin_unlock(&bd_pool->lock);
		return NULL;
	}

	j = 0;
	head = bd_pool->head;
	for (i = 0; i < bd_pool->number; i++) {
		if (j >= number)
			break;
		if (bd_pool->occupied[i]) {
			j = 0;
			head = NULL;
		}
		else {
			if (!head) {
				if (!((u32) head & BD_BASE_ALIGN)) {
					head = bd_pool->head + i;
				}
				else
					continue;
			}
			j++;
		}
	}
	if (i == bd_pool->number) {
		spin_unlock(&bd_pool->lock);
		fua_debug("Do not enough bds in bd_pool;"
			"number:0x%x frees:0x%x request:0x%x\n",
			bd_pool->number, bd_pool->frees, number);
		return NULL;
	}

	memset(&bd_pool->occupied[head - bd_pool->head], 0x1,
			sizeof(bd_pool->occupied[0]) * number);
	bd_pool->frees -= number;
	spin_unlock(&bd_pool->lock);
	return head;
}

static void free_bds(bd_pool_t *bd_pool, struct qe_bd *head, int number)
{
	int i;

	i = head - bd_pool->head;
	spin_lock(&bd_pool->lock);
	memset(&bd_pool->occupied[i], 0,
			sizeof(bd_pool->occupied[0]) * number);
	bd_pool->frees += number;
	spin_unlock(&bd_pool->lock);
}

void dump_thread_local_pg_para_tbl(struct thread_local_pg_para_tbl
					*thread_local)
{
	fua_dump("thread_local_page_parameter_table at %p\n", thread_local);
	fua_dump("\tint_rct_tmp_ptr: 0x%x\n", thread_local->int_rct_tmp_ptr);
	fua_dump("\tres0: 0x%x\n", thread_local->res0);
	fua_dump("\tthreadid: 0x%x\n", thread_local->threadid);
	fua_dump("\tres1: 0x%x\n", thread_local->res1);
	fua_dump("\trxqd_tmp: 0x%x\n", thread_local->rxqd_tmp);
	fua_dump("\tpprs_int_ptr: 0x%x\n", thread_local->pprs_int_ptr);
}

void dump_thread_para_ram_pg(struct thread_para_ram_pg *thread_para)
{
	fua_dump("thread_para_ram_pg_t at %p\n", thread_para);
	fua_dump("\tlocal_pg_para_ptr: 0x%x\n",
			thread_para->local_pg_para_ptr);
	fua_dump("\tsubpg0_config_table_ptr"
			"\tsubpg0_rxtmp_table_ptr"
			"\tsubpg0_txtmp_table_ptr\n");
	fua_dump("\t\t0x%x\t\t0x%x\t\t0x%x\n",
			thread_para->subpg0_conf_tbl_ptr,
			thread_para->subpg0_rx_tmp_tbl_ptr,
			thread_para->subpg0_tx_tmp_tbl_ptr);
	fua_dump("\tsubpg1_config_table_ptr"
			"\tsubpg0_rxtmp_table_ptr"
			"\tsubpg0_txtmp_table_ptr\n");
	fua_dump("\t\t0x%x\t\t0x%x\t\t0x%x\n",
			thread_para->subpg1_conf_tbl_ptr,
			thread_para->subpg1_rx_tmp_tbl_ptr,
			thread_para->subpg1_tx_tmp_tbl_ptr);
}

void dump_thread_para(struct thread_para_ram_pg *thread_para)
{
	struct thread_local_pg_para_tbl *thread_local;
	u16 offset;

	offset = in_be16(&thread_para->local_pg_para_ptr);
	thread_local = (struct thread_local_pg_para_tbl *)qe_muram_addr(offset);

	dump_thread_para_ram_pg(thread_para);
	dump_thread_local_pg_para_tbl(thread_local);
}

void dump_comm_mth(struct comm_mth_para_tbl *comm_mth, int required_threads)
{
	struct thread_entry *thread_entry;
	u16 offset;
	int i;

	fua_dump("Common MTH Parameters Table at %p\n", comm_mth);
	fua_dump("\tatm_threads_table_base: 0x%x\n",
			comm_mth->atm_threads_tbl_base);
	fua_dump("\tatm_thread_cam_base: 0x%x\n",
			comm_mth->atm_thread_cam_base);
	fua_dump("\tatm_thread_empty_status: 0x%x\n",
			comm_mth->atm_thread_empty_status);
	fua_dump("\tatm_thread_cam_size: 0x%x\n",
			comm_mth->atm_thread_cam_size);

	offset = in_be16(&comm_mth->atm_threads_tbl_base);
	thread_entry = (struct thread_entry *)qe_muram_addr(offset);
	fua_dump("seqence\tlocation\tsnum\tres\tpage\n");
	for (i = 0; i < required_threads; i++) {
		fua_dump("%d\t%p\t0x%x\t0x%x\t0x%x\n", i,
				thread_entry, thread_entry->snum,
				thread_entry->res, thread_entry->page);
		thread_entry++;
	}

	thread_entry = (struct thread_entry *)qe_muram_addr(offset);
	for (i = 0; i < required_threads; i++) {
		fua_dump("\nThe %dth thread parameter\n", i);
		dump_thread_para((struct thread_para_ram_pg *)
				 qe_muram_addr(in_be16(&thread_entry->page)));
		thread_entry++;
	}
}

void dump_subpg0(sub_pg0_conf_tbl_t * subpg0, int mthmode)
{
	fua_dump("subpg0 at %p\n", subpg0);
	fua_dump("\tgbl_atm_para_tbl_ptr: 0x%x at 0x%p\n",
		subpg0->gbl_atm_para_tbl_ptr, &subpg0->gbl_atm_para_tbl_ptr);
	fua_dump("\toam_ch_rct_ptr: 0x%x at 0x%p\n",
			subpg0->oam_ch_rct_ptr, &subpg0->oam_ch_rct_ptr);
	fua_dump("\tucc_mode: 0x%x at %p\n",
			subpg0->ucc_mode, &subpg0->ucc_mode);
	fua_dump("\tgmode: 0x%x at 0x%p\n", subpg0->gmode, &subpg0->gmode);
	fua_dump("\tint_tcte_tmp_ptr: 0x%x at 0x%p\n",
			subpg0->int_tcte_tmp_ptr, &subpg0->int_tcte_tmp_ptr);
	fua_dump("\tadd_comp_lookup_base: 0x%x at 0x%p\n",
			subpg0->add_comp_lookup_base,
			&subpg0->add_comp_lookup_base);
	fua_dump("\tapcp_base: 0x%x at 0x%p\n",
			subpg0->apcp_base, &subpg0->apcp_base);
	fua_dump("\tintt_base: 0x%x at 0x%p\n",
			subpg0->intt_base, &subpg0->intt_base);
	fua_dump("\tuni_statt_base: 0x%x at 0x%p\n",
			subpg0->uni_statt_base, &subpg0->uni_statt_base);
	fua_dump("\tbd_base_ext: 0x%x at 0x%p\n", subpg0->bd_base_ext,
			&subpg0->bd_base_ext);

	if (mthmode) {
		fua_dump("\trx_term_snum: 0x%x at 0x%p\n",
				subpg0->rx_term_snum, &subpg0->rx_term_snum);
		fua_dump("\ttx_term_snum: 0x%x at 0x%p\n",
				subpg0->tx_term_snum, &subpg0->tx_term_snum);
		fua_dump("\tcom_mth_para_tbl_base: 0x%x at 0x%p\n",
				subpg0->com_mth_para_tbl_base,
				&subpg0->com_mth_para_tbl_base);
		fua_dump("\tmth_term_rx_stat_ptr: 0x%x at 0x%p\n",
				subpg0->mth_term_rx_stat_ptr,
				&subpg0->mth_term_rx_stat_ptr);
		fua_dump("\tmth_term_tx_stat_ptr: 0x%x at 0x%p\n",
				subpg0->mth_term_tx_stat_ptr,
				&subpg0->mth_term_tx_stat_ptr);
		fua_dump("\tatm_aval_rx_thd_mask: 0x%x at 0x%p\n",
				subpg0->atm_aval_rx_thd_mask,
				&subpg0->atm_aval_rx_thd_mask);
		fua_dump("\tatm_aval_tx_thd_mask: 0x%x at 0x%p\n",
				subpg0->atm_aval_tx_thd_mask,
				&subpg0->atm_aval_tx_thd_mask);
	}
}

void dump_gbl(gbl_atm_para_tbl_t * gapt)
{
	fua_dump("gapt at %p\n", gapt);
	fua_dump("\tint_rct_base: 0x%x at %p\n", gapt->int_rct_base,
			&gapt->int_rct_base);
	fua_dump("\tint_tct_base: 0x%x at %p\n", gapt->int_tct_base,
			&gapt->int_tct_base);
	fua_dump("\text_rct_base: 0x%x at %p\n", gapt->ext_rct_base,
			&gapt->ext_rct_base);
	fua_dump("\text_tct_base: 0x%x at %p\n", gapt->ext_tct_base,
			&gapt->ext_tct_base);
	fua_dump("\text_tcte_base: 0x%x at %p\n", gapt->ext_tcte_base,
			&gapt->ext_tcte_base);
	fua_dump("\tint_tcte_base: 0x%x at %p\n", gapt->int_tcte_base,
			&gapt->int_tcte_base);
	fua_dump("\tcom_info_ctl: 0x%x at %p\n", gapt->com_info_ctl,
			&gapt->com_info_ctl);
	fua_dump("\tcom_info_cc: 0x%x at %p\n", gapt->com_info_cc,
			&gapt->com_info_cc);
	fua_dump("\tcom_info_bt: 0x%x at %p\n", gapt->com_info_bt,
			&gapt->com_info_bt);
	fua_dump("\tcom_sus_tmp: 0x%x at %p\n", gapt->com_sus_tmp,
			&gapt->com_sus_tmp);
	fua_dump("\tcom_lk_tmp: 0x%x at %p\n", gapt->com_lk_tmp,
			&gapt->com_lk_tmp);
}

void dump_distributor_para_ram_pg(struct distributor_para_ram_pg *dist_para)
{
	fua_dump("dist_para at %p\n", dist_para);
	fua_dump("\tdistributor_local_pg_para_ptr: 0x%x at %p\n",
			dist_para->distributor_local_pg_para_ptr,
			&dist_para->distributor_local_pg_para_ptr);
	fua_dump("\tsubpg0_conf_tbl_ptr: 0x%x at %p\n",
			dist_para->subpg0_conf_tbl_ptr,
			&dist_para->subpg0_conf_tbl_ptr);
	fua_dump("\tsubpg0_rx_tmp_tbl_ptr: 0x%x at %p\n",
			dist_para->subpg0_rx_tmp_tbl_ptr,
			&dist_para->subpg0_rx_tmp_tbl_ptr);
	fua_dump("\tsubpg0_tx_tmp_tbl_ptr: 0x%x at %p\n",
			dist_para->subpg0_tx_tmp_tbl_ptr,
			&dist_para->subpg0_tx_tmp_tbl_ptr);
	fua_dump("\tsubpg1_conf_tbl_ptr: 0x%x at %p\n",
			dist_para->subpg1_conf_tbl_ptr,
			&dist_para->subpg1_conf_tbl_ptr);
}

void dump_uccpg(ucc_para_atm_pg_t * ucc_para)
{
	fua_dump("uccpg at %p\n", ucc_para);
	fua_dump("\tlocal_pg_para_ptr: 0x%x at %p\n",
			ucc_para->local_pg_para_ptr, &ucc_para->local_pg_para_ptr);
	fua_dump("\tsubpg0_conf_tbl_ptr: 0x%x at %p\n",
			ucc_para->subpg0_conf_tbl_ptr,
			&ucc_para->subpg0_conf_tbl_ptr);
	fua_dump("\tsubpg0_rx_tmp_tbl_ptr: 0x%x at %p\n",
			ucc_para->subpg0_rx_tmp_tbl_ptr,
			&ucc_para->subpg0_rx_tmp_tbl_ptr);
	fua_dump("\tsubpg0_tx_tmp_tbl_ptr: 0x%x at %p\n",
			ucc_para->subpg0_tx_tmp_tbl_ptr,
			&ucc_para->subpg0_tx_tmp_tbl_ptr);
	fua_dump("\tsubpg1_conf_tbl_ptr: 0x%x at %p\n",
			ucc_para->subpg1_conf_tbl_ptr,
			&ucc_para->subpg1_conf_tbl_ptr);
	fua_dump("\tsubpg1_rx_tmp_tbl_ptr: 0x%x at %p\n",
			ucc_para->subpg1_rx_tmp_tbl_ptr,
			&ucc_para->subpg1_rx_tmp_tbl_ptr);
	fua_dump("\tsubpg1_tx_tmp_tbl_ptr: 0x%x at %p\n",
			ucc_para->subpg1_tx_tmp_tbl_ptr,
			&ucc_para->subpg1_tx_tmp_tbl_ptr);
}

void dump_ucc_para(struct fua_private *f_p)
{
	sub_pg0_conf_tbl_t *subpg0;
	gbl_atm_para_tbl_t *gapt;
	u16 offset;

	if (f_p->fua_info->mthmode) {
		distributor_para_ram_pg_t *dist_para;
		comm_mth_para_tbl_t *comm_mth;

		dist_para = (distributor_para_ram_pg_t *)
				f_p->ucc_para_pg;
		dump_distributor_para_ram_pg(dist_para);
		offset = in_be16(&dist_para->subpg0_conf_tbl_ptr);
		subpg0 = (sub_pg0_conf_tbl_t *)qe_muram_addr(offset);
		dump_subpg0(subpg0, 1);
		offset = in_be16(&subpg0->gbl_atm_para_tbl_ptr);
		gapt = (gbl_atm_para_tbl_t *)qe_muram_addr(offset);
		dump_gbl(gapt);
		offset = in_be16(&subpg0->com_mth_para_tbl_base);
		comm_mth = (comm_mth_para_tbl_t *)qe_muram_addr(offset);
		dump_comm_mth(comm_mth, f_p->fua_info->threads);
	} else {
		ucc_para_atm_pg_t *ucc_para;
		ucc_para = f_p->ucc_para_pg;
		dump_uccpg(ucc_para);
		offset = in_be16(&ucc_para->subpg0_conf_tbl_ptr);
		subpg0 = (sub_pg0_conf_tbl_t *)qe_muram_addr(offset);
		dump_subpg0(subpg0, 0);
		offset = in_be16(&subpg0->gbl_atm_para_tbl_ptr);
		gapt = (gbl_atm_para_tbl_t *)qe_muram_addr(offset);
		dump_gbl(gapt);
	}
}

void dump_bd(struct qe_bd * bd, int flag)
{
	int i;
	u8 *tmp;

	fua_dump("bd at: 0x%p\n", bd);
	fua_dump("\tbd->status: 0x%x\n", bd->status);
	fua_dump("\tbd->length: 0x%x = %d\n", bd->length, bd->length);
	fua_dump("\tbd->buf: 0x%x\n", bd->buf);
	tmp = phys_to_virt((unsigned long)bd->buf);
	fua_dump("\tbd->buf(virt): 0x%p\n", tmp);
	if (flag) {
		for (i = 0; i < bd->length; i++) {
			if (!(i % 8))
				fua_dump("\n%p:", tmp);
			fua_dump("\t 0x%x", *tmp);
			tmp++;
		}
		fua_dump("\n");
	}
}

void dump_data(u32 * data, u32 len)
{
	int i;
	fua_dump("data start at 0x%p len %d", data, len);
	for (i = 0; i < len; i++) {
		if (!(i % 8))
			fua_dump("\n");
		fua_dump("\t 0x%x", *data);
		data++;
	}
	fua_dump("\n");
}

/* ATM parameter init in Multi-thread mode */
int thread_para_init(thread_para_ram_pg_t * thread_para)
{
	unsigned long offset;
	thread_local_pg_para_tbl_t *thread_local;

	/* init thread local parameter ram page */
	offset = qe_muram_alloc(sizeof(thread_local_pg_para_tbl_t), 0x8);
	out_be16(&thread_para->local_pg_para_ptr, offset);
	out_be16(&thread_para->subpg0_conf_tbl_ptr, 0x0);
	offset = qe_muram_alloc(0x40, 0x8);
	out_be16(&thread_para->subpg0_rx_tmp_tbl_ptr, offset);
	out_be16(&thread_para->subpg0_tx_tmp_tbl_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x40);
	out_be16(&thread_para->subpg1_conf_tbl_ptr, 0x0);
	offset = qe_muram_alloc(0x40, 0x8);
	out_be16(&thread_para->subpg1_rx_tmp_tbl_ptr, offset);
	out_be16(&thread_para->subpg1_tx_tmp_tbl_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x40);

	thread_local = (thread_local_pg_para_tbl_t *)
		qe_muram_addr(in_be16(&thread_para->local_pg_para_ptr));
	_memset_io((void *)thread_local, 0x0,
		sizeof(thread_local_pg_para_tbl_t));
	offset = qe_muram_alloc(0x20, 0x20);
	out_be16(&thread_local->int_rct_tmp_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0,0x20);
	/* For aal2 and CES */
	offset = qe_muram_alloc(0x20, 0x20);
	out_be16(&thread_local->rxqd_tmp , offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x20);
	offset = qe_muram_alloc(0x40, 0x40);
	out_be16(&thread_local->pprs_int_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x40);
	return 0;
}

void thread_para_exit(thread_para_ram_pg_t * thread_para)
{
	thread_local_pg_para_tbl_t *thread_local;

	thread_local = (thread_local_pg_para_tbl_t *)
			qe_muram_addr(in_be16(&thread_para->local_pg_para_ptr));
	qe_muram_free(in_be16(&thread_local->pprs_int_ptr));
	qe_muram_free(in_be16(&thread_local->rxqd_tmp));
	qe_muram_free(in_be16(&thread_local->int_rct_tmp_ptr));

	qe_muram_free(in_be16(&thread_para->subpg1_rx_tmp_tbl_ptr));
	qe_muram_free(in_be16(&thread_para->subpg0_rx_tmp_tbl_ptr));
	qe_muram_free(in_be16(&thread_para->local_pg_para_ptr));
}

/* initialize the comm_mth_thread_table and thread parameter page */
#define DISTRIBUTOR_THREAD_ALIGNMENT	0x40
int comm_mth_thread_init(comm_mth_para_tbl_t *comm_mth, int required_threads)
{
	unsigned long offset;
	int i, snum;
	u32 status;
	thread_entry_t *thread_entry;

	if (required_threads > 32)
		return -EINVAL;

	offset = qe_muram_alloc(required_threads * sizeof(thread_entry_t), 4);
	out_be16(&comm_mth->atm_threads_tbl_base, offset);
	_memset_io((void *)qe_muram_addr(offset), 0,
			 required_threads * sizeof(thread_entry_t));

	thread_entry = (thread_entry_t *)qe_muram_addr(offset);
	for (i = 0; i < required_threads; i++) {
		if ((snum = qe_get_snum()) < 0) {
			fua_debug("Fail to get %dth snum\n", i);
			return -EINVAL;
		}
		out_8(&thread_entry->snum, snum);
		offset = qe_muram_alloc(sizeof(thread_para_ram_pg_t),
				DISTRIBUTOR_THREAD_ALIGNMENT);
		out_be16(&thread_entry->page, offset);
		_memset_io((void *)qe_muram_addr(offset),
				0x0, sizeof(thread_para_ram_pg_t));
		thread_para_init((thread_para_ram_pg_t *)
					qe_muram_addr(offset));
		thread_entry++;
	}

	offset = qe_muram_alloc(required_threads * sizeof(thread_entry_t), 4);
	out_be16(&comm_mth->atm_thread_cam_base, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0,
			required_threads * sizeof(thread_entry_t));

	status = -1UL >> required_threads;
	out_be32(&comm_mth->atm_thread_empty_status, status);
	out_8(&comm_mth->atm_thread_cam_size,
		required_threads * sizeof(thread_entry_t) - 1);

	return 0;
}

void comm_mth_thread_exit(comm_mth_para_tbl_t * comm_mth, int required_threads)
{
	int i;
	thread_entry_t *thread_entry;

	qe_muram_free(in_be16(&comm_mth->atm_thread_cam_base));

	thread_entry = (thread_entry_t *)
			qe_muram_addr(in_be16(&comm_mth->atm_threads_tbl_base));
	for (i = 0; i < required_threads; i++) {
		thread_para_exit((thread_para_ram_pg_t *)
				 qe_muram_addr(in_be16(&thread_entry->page)));
		qe_muram_free(in_be16(&thread_entry->page));
		qe_put_snum(in_8(&thread_entry->snum));
	}
	qe_muram_free(in_be16(&comm_mth->atm_threads_tbl_base));
}

int subpg0_init(void *ucc_para_pg,
		 int mthmode, int required_threads,
		 u16 oam_ch_rct_ptr,
		 u16 ucc_mode, u16 gmode,
		 u16 rev_timeout_req_period,
		 u16 add_comp_lookup_base,
		 u16 vci_filter, u16 apcp_base,
		 u16 intt_base, u16 uni_stat_base,
		 u8 bd_base_ext, u16 pmt_base,
		 u16 int_rct_base, u16 int_tct_base,
		 u32 ext_rct_base, u32 ext_tct_base)
{
	int i;
	unsigned long offset;
	sub_pg0_conf_tbl_t *subpg0;
	gbl_atm_para_tbl_t *gapt;

	if (mthmode) {
		offset = in_be16(&
				((distributor_para_ram_pg_t *)ucc_para_pg)
							->subpg0_conf_tbl_ptr);
		subpg0 = (sub_pg0_conf_tbl_t *)qe_muram_addr(offset);
	} else {
		offset = in_be16(&
				((ucc_para_atm_pg_t *)ucc_para_pg)
							->subpg0_conf_tbl_ptr);
		subpg0 = (sub_pg0_conf_tbl_t *)qe_muram_addr(offset);
	}

	offset = qe_muram_alloc(0x30, 0x20);
	out_be16(&subpg0->gbl_atm_para_tbl_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0, 0x30);
	/* pointer to channel 0 for raw cell queue */
	out_be16(&subpg0->oam_ch_rct_ptr, oam_ch_rct_ptr);
	out_be16(&subpg0->ucc_mode, 0);
	out_be32(&subpg0->cam_mask, 0);
	out_be16(&subpg0->gmode, 0);
	offset = qe_muram_alloc(0x20, 0x20);
	out_be16(&subpg0->int_tcte_tmp_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0, 0x20);
	out_be16(&subpg0->add_comp_lookup_base, add_comp_lookup_base);
	if ((ucc_mode & UCC_MODE_NPL) || (gmode & 0x0002))
		rev_timeout_req_period = 0;
	out_be16(&subpg0->rev_timeout_req_period, rev_timeout_req_period);
	out_be16(&subpg0->vci_filter, vci_filter);
	out_be16(&subpg0->apcp_base, apcp_base);
	out_be16(&subpg0->intt_base, intt_base);
	out_be16(&subpg0->uni_statt_base, uni_stat_base);
	out_8(&subpg0->bd_base_ext,bd_base_ext);
	out_be16(&subpg0->pmt_base, pmt_base);
	out_be16(&subpg0->gmode, gmode);
	out_be16(&subpg0->ucc_mode, ucc_mode);
	if (mthmode) {
		int snum;
		if ((snum = qe_get_snum()) < 0) {
			fua_debug("fail to alloc snum\n");
			return -EINVAL;
		}
		printk("Alloc snum:%d as atm rx ternimator\n", snum);
		subpg0->rx_term_snum = snum;
		if ((snum = qe_get_snum()) < 0) {
			fua_debug("fail to alloc snum\n");
			return -EINVAL;
		}
		printk("Alloc snum:%d as atm tx ternimator\n", snum);
		subpg0->tx_term_snum = snum;

		offset = qe_muram_alloc(sizeof(comm_mth_para_tbl_t), 0x10);
		out_be16(&subpg0->com_mth_para_tbl_base, offset);
		_memset_io((void *)qe_muram_addr(offset),
				0x0, sizeof(comm_mth_para_tbl_t));
		comm_mth_thread_init((comm_mth_para_tbl_t *)
					qe_muram_addr(offset),
					required_threads);

		offset = qe_muram_alloc(0x42, 0x80);
		out_be16(&subpg0->mth_term_rx_stat_ptr, offset);
		_memset_io((void *)qe_muram_addr(offset), 0x0, 0x42);
		offset = qe_muram_alloc(0x42, 0x80);
		out_be16(&subpg0->mth_term_tx_stat_ptr, offset);
		_memset_io((void *)qe_muram_addr(offset), 0x0, 0x42);

		offset = 0;
		for (i = 0; i < required_threads; i++)
			offset |= 0x80000000 >> i;
		out_be32(&subpg0->atm_aval_rx_thd_mask, offset);
		out_be32(&subpg0->atm_aval_tx_thd_mask, offset);

		out_be16(&subpg0->ucc_mode,
			in_be16(&subpg0->ucc_mode) | UCC_MODE_MULTI_THREAD_EN);
	}

	gapt = (gbl_atm_para_tbl_t *)
			qe_muram_addr(in_be16(&subpg0->gbl_atm_para_tbl_ptr));
	out_be16(&gapt->int_rct_base, int_rct_base);
	out_be16(&gapt->int_tct_base, int_tct_base);

	/*
	 * The internal channel offset must be added while
	 * calcualte external channel table address
	 */
	out_be32(&gapt->ext_rct_base, ext_rct_base -
		(MAX_INTERNAL_CHANNEL_CODE + 1) * sizeof(rct_entry_t));
	out_be32(&gapt->ext_tct_base, ext_tct_base -
		(MAX_INTERNAL_CHANNEL_CODE + 1) * sizeof(tct_entry_t));
	out_be32(&gapt->com_sus_tmp, 0x0);
	out_be16(&gapt->com_lk_tmp,0x0);

	return 0;
}

void subpg0_exit(void *ucc_para_pg, int mthmode, int required_threads)
{
	sub_pg0_conf_tbl_t *subpg0;

	if (mthmode)
		subpg0 = (sub_pg0_conf_tbl_t *)
		qe_muram_addr(((distributor_para_ram_pg_t *)ucc_para_pg)
				->subpg0_conf_tbl_ptr);
	else
		subpg0 = (sub_pg0_conf_tbl_t *)
		qe_muram_addr(((ucc_para_atm_pg_t *)ucc_para_pg)
				->subpg0_conf_tbl_ptr);

	if (mthmode) {
		qe_muram_free(in_be16(&subpg0->mth_term_tx_stat_ptr));
		qe_muram_free(in_be16(&subpg0->mth_term_rx_stat_ptr));
		comm_mth_thread_exit((comm_mth_para_tbl_t *)
			qe_muram_addr(in_be16(&subpg0->com_mth_para_tbl_base)),
				required_threads);
		qe_muram_free(in_be16(&subpg0->com_mth_para_tbl_base));

		qe_put_snum(in_8(&subpg0->tx_term_snum));
		qe_put_snum(in_8(&subpg0->rx_term_snum));
	}
	qe_muram_free(in_be16(&subpg0->int_tcte_tmp_ptr));
	qe_muram_free(in_be16(&subpg0->gbl_atm_para_tbl_ptr));
}

int distributor_para_pg_init(distributor_para_ram_pg_t *dist_para)
{
	unsigned long offset;
	distributor_local_pg_para_tbl_t *dist_local;

	offset = qe_muram_alloc(sizeof(distributor_local_pg_para_tbl_t), 8);
	out_be16(&dist_para->distributor_local_pg_para_ptr, offset);
	dist_local = (distributor_local_pg_para_tbl_t *)qe_muram_addr(offset);
	_memset_io((void *)dist_local, 0x0,
			sizeof(distributor_local_pg_para_tbl_t));

	offset = qe_muram_alloc(0x20, 0x20);
	out_be16(&dist_local->rx_tmp, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x20);

	offset = qe_muram_alloc(sizeof(sub_pg0_conf_tbl_t), 8);
	out_be16(&dist_para->subpg0_conf_tbl_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0,
			sizeof(sub_pg0_conf_tbl_t));
	offset = qe_muram_alloc(0x40, 8);
	out_be16(&dist_para->subpg0_rx_tmp_tbl_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x40);
	offset = qe_muram_alloc(0x40, 8);
	out_be16(&dist_para->subpg0_tx_tmp_tbl_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x40);
	return 0;
}

void distributor_para_pg_exit(distributor_para_ram_pg_t *dist_para)
{
	distributor_local_pg_para_tbl_t *dist_local;

	qe_muram_free(in_be16(&dist_para->subpg0_tx_tmp_tbl_ptr));
	qe_muram_free(in_be16(&dist_para->subpg0_rx_tmp_tbl_ptr));
	qe_muram_free(in_be16(&dist_para->subpg0_conf_tbl_ptr));

	dist_local = (distributor_local_pg_para_tbl_t *)qe_muram_addr(
			in_be16(&dist_para->distributor_local_pg_para_ptr));
	qe_muram_free(in_be16(&dist_local->rx_tmp));
	qe_muram_free(in_be16(&dist_para->distributor_local_pg_para_ptr));
}

/* Non-multi_thread mode */
int ucc_para_pg_init(ucc_para_atm_pg_t *ucc_para)
{
	unsigned long offset;
	local_pg_para_tbl_t *local;

	_memset_io((void *)ucc_para, 0x0, sizeof(ucc_para_atm_pg_t));
	offset = qe_muram_alloc(0x10, 8);
	out_be16(&ucc_para->local_pg_para_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x10);

	offset = qe_muram_alloc(0x60, 8);
	out_be16(&ucc_para->subpg0_conf_tbl_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x60);

	offset = qe_muram_alloc(0x40, 8);
	out_be16(&ucc_para->subpg0_rx_tmp_tbl_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x40);

	offset = qe_muram_alloc(0x40, 8);
	out_be16(&ucc_para->subpg0_tx_tmp_tbl_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x40);

	offset = qe_muram_alloc(0x40, 8);
	out_be16(&ucc_para->subpg1_conf_tbl_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x40);

	offset = qe_muram_alloc(0x40, 8);
	out_be16(&ucc_para->subpg1_rx_tmp_tbl_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x40);

	offset = qe_muram_alloc(0x40, 8);
	out_be16(&ucc_para->subpg1_tx_tmp_tbl_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x40);

	/* local page parameter initialization */
	local = (local_pg_para_tbl_t *)
		qe_muram_addr(in_be16(&ucc_para->local_pg_para_ptr));
	offset = qe_muram_alloc(0x40, 0x20);
	out_be16(&local->int_rct_tmp_ptr, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x40);

	offset = qe_muram_alloc(0x20, 8);
	out_be16(&local->rx_tmp, offset);
	_memset_io((void *)qe_muram_addr(offset), 0x0, 0x20);

	return 0;
}

void ucc_para_pg_exit(ucc_para_atm_pg_t *ucc_para)
{
	local_pg_para_tbl_t *local;

	local = (local_pg_para_tbl_t *)
		qe_muram_addr(in_be16(&ucc_para->local_pg_para_ptr));
	qe_muram_free(in_be16(&local->rx_tmp));
	qe_muram_free(in_be16(&local->int_rct_tmp_ptr));

	qe_muram_free(in_be16(&ucc_para->subpg1_tx_tmp_tbl_ptr));
	qe_muram_free(in_be16(&ucc_para->subpg1_rx_tmp_tbl_ptr));
	qe_muram_free(in_be16(&ucc_para->subpg1_conf_tbl_ptr));
	qe_muram_free(in_be16(&ucc_para->subpg0_tx_tmp_tbl_ptr));
	qe_muram_free(in_be16(&ucc_para->subpg0_rx_tmp_tbl_ptr));
	qe_muram_free(in_be16(&ucc_para->subpg0_conf_tbl_ptr));
	qe_muram_free(in_be16(&ucc_para->local_pg_para_ptr));
}

int ucc_atm_pg_init(void *ucc_para_pg,
			int mthmode, int required_threads,
			u32 subblock,
			u16 oam_ch_rct_ptr,
			u16 ucc_mode, u16 gmode,
			u16 rev_timeout_req_period,
			u16 add_comp_lookup_base,
			u16 vci_filter, u16 apcp_base,
			u16 intt_base, u16 uni_stat_base,
			u8 bd_base_ext, u16 pmt_base,
			u16 int_rct_base, u16 int_tct_base,
			u32 ext_rct_base, u32 ext_tct_base)
{
	if (mthmode)
		distributor_para_pg_init((distributor_para_ram_pg_t *)ucc_para_pg);
	else
		ucc_para_pg_init((ucc_para_atm_pg_t *)ucc_para_pg);

	subpg0_init(ucc_para_pg, mthmode, required_threads,
			oam_ch_rct_ptr,	ucc_mode, gmode, rev_timeout_req_period,
			add_comp_lookup_base, vci_filter, apcp_base, intt_base,
			uni_stat_base, bd_base_ext, pmt_base, int_rct_base,
			int_tct_base, ext_rct_base, ext_tct_base);

	if (mthmode)
		qe_issue_cmd(QE_ATM_MULTI_THREAD_INIT, subblock,
				QE_CR_PROTOCOL_ATM_POS, 0);

	return 0;
}

void ucc_atm_pg_exit(void *ucc_para_pg, int mthmode, int required_threads)
{
	subpg0_exit(ucc_para_pg, mthmode, required_threads);
	if (mthmode)
		distributor_para_pg_exit((distributor_para_ram_pg_t *)ucc_para_pg);
	else
		ucc_para_pg_exit((ucc_para_atm_pg_t *)ucc_para_pg);
}

/********************************Driver routines***************************************/

/*
 * act: ATM channel type
 *	for PCR,should be 0b00
 * pri: APC priority level (0b000 ~ 0b111)
 * bt: only accepted for VBR and Hierarchical Frame Mode
 ****************************************/
void atm_transmit(struct fua_vcc *fua_vcc, u8 act, u8 pri, u16 bt)
{
	struct fua_private *f_p;
	struct fua_device *fua_dev;
	sub_pg0_conf_tbl_t *subpg0;
	gbl_atm_para_tbl_t *gbl;
	u16 offset, value;

	while (fua_vcc->tct->attr & TCT_ENTRY_ATTR_VCON)
		cpu_relax();
	fua_vcc->tct->attr |= TCT_ENTRY_ATTR_VCON;
	fua_dev = (struct fua_device *)(fua_vcc->vcc->dev->dev_data);
	f_p = fua_dev->fua_priv;
	offset = in_be16(&f_p->ucc_para_pg->subpg0_conf_tbl_ptr);
	subpg0 = (sub_pg0_conf_tbl_t *)qe_muram_addr(offset);
	offset = in_be16(&subpg0->gbl_atm_para_tbl_ptr);
	gbl = (gbl_atm_para_tbl_t *)qe_muram_addr(offset);

	value = ((fua_dev->phy_info->phy_id & 0x7F) << 5) | ((act & 0x3) << 3)
			| (pri & 0x7);
	out_be16(&gbl->com_info_ctl, value);
	out_be16(&gbl->com_info_cc, fua_vcc->tx_cc);

	if ((act == ACT_VBR) || (act == ACT_HF))
		out_be16(&gbl->com_info_bt, bt);

	qe_issue_cmd(QE_ATM_TRANSMIT, f_p->subblock,
			QE_CR_PROTOCOL_ATM_POS, 0);
	return;
}

struct qe_bd *get_free_tx_bd(struct fua_vcc * fua_vcc)
{
	struct qe_bd *bd, *bd_now;

	bd = bd_now = fua_vcc->txcur;
	fua_vcc->txcur =
		bd_get_next(fua_vcc->txcur, fua_vcc->txbase, AAL5_TXBD_ATTR_W);
	while (bd->status & AAL5_TXBD_ATTR_R) {
		bd = fua_vcc->txcur;
		fua_vcc->txcur =
			bd_get_next(fua_vcc->txcur, fua_vcc->txbase,
					AAL5_TXBD_ATTR_W);
		if (bd == bd_now) {
			fua_warning("All tx bd are occupied\n");
			return NULL;
		}
	}
	return bd;
}

/* Calculate PCR and PCR_FRACTION */
void pcr_calc(int line_bitr, int vc_bitr, u8 cps, u16 * pcr, u8 * pcr_fraction)
{
	int tmp;

	if ((line_bitr == 0) || (vc_bitr == 0)) {
		*pcr = 0;
		*pcr_fraction = 0;
		return ;
	}
	*pcr = line_bitr / (vc_bitr * cps);
	/* keep 3 bit after dot */
	tmp = (line_bitr * 1000) / (vc_bitr * cps);
	tmp -= *pcr * 1000;
	*pcr_fraction = (u8) (tmp * 256 / 1000);
}

void dump_tct(tct_entry_t * tct, u32 aal)
{
	fua_dump("Transmit Connection Table: %p\n", tct);
	fua_dump("\tattr:\t0x%x\n", tct->attr);
	fua_dump("\ttxdbptr:\t0x%x\n", tct->txdbptr);
	fua_dump("\ttbdcnt:\t0x%x\n", tct->tbdcnt);
	fua_dump("\ttbd_offset:\t0x%x\n", tct->tbd_offset);
	fua_dump("\trate:\t0x%x\n", tct->rate);
	if (aal == ATM_AAL5) {
		aal5_tct_t *aal5;
		aal5 = &tct->aal5;
		fua_dump("\taal5 specific: %p\n", aal5);
		fua_dump("\t\ttx_crc:\t0x%x\n", aal5->tx_crc);
		fua_dump("\t\ttml:\t0x%x\n", aal5->tml);
	}
	fua_dump("\tapclc:\t0x%x\n", tct->apclc);
	fua_dump("\tcell_header:\t0x%x\n", tct->cell_header);
	fua_dump("\tpmt_tbd_base:\t0x%x\n", tct->pmt_tbd_base);
	fua_dump("\ttbd_base_oths:\t0x%x\n", tct->tbd_base_oths);
}

int open_tx(struct atm_vcc *vcc)
{
	struct atm_dev *dev;
	struct fua_device *fua_dev;
	struct fua_private *f_p;
	struct fua_info *fua_info;
	struct fua_vcc *fua_vcc;
	tct_entry_t *tct;
	struct qe_bd *bd;
	struct atm_trafprm *tx_qos;
	u16 pcr;
	u8 pcr_fraction;
	int i;

	dev = vcc->dev;
	fua_dev = (struct fua_device *)(dev->dev_data);
	f_p = fua_dev->fua_priv;
	fua_info = f_p->fua_info;
	fua_vcc = (struct fua_vcc *)(vcc->dev_data);
	tx_qos = &vcc->qos.txtp;

	if ((tx_qos->traffic_class == ATM_CBR)
		|| (tx_qos->traffic_class == ATM_UBR)
		|| (tx_qos->traffic_class == ATM_ANYCLASS)) {
		fua_vcc->traffic_type = PCR;
	} else
		return -1;

	fua_vcc->tx_intq_num = INT_TX_QUE;

	/* alloc bds */
	fua_vcc->txbase = alloc_bds(&f_p->bd_pool, fua_info->bd_per_channel);
	if (!fua_vcc->txbase) {
		fua_debug("failed in alloc bds\n");
		return -ENOMEM;
	}
	fua_vcc->txcur = bd = fua_vcc->txbase;
	for (i = 0; i < fua_info->bd_per_channel; i++) {
		bd->status = 0;
		bd->status |= AAL5_TXBD_ATTR_I;
		bd->length = 0;
		if (i < (fua_info->bd_per_channel - 1))
			bd++;
	}
	bd->status |= AAL5_TXBD_ATTR_W;

	for (i = 2; i < fua_info->max_channel; i++) {
		/* The first two channel are reserved according to UM */
		if (!f_p->tx_vcc[i]) {
			f_p->tx_vcc[i] = vcc;
			fua_vcc->tx_cc = i;
			if (i > MAX_INTERNAL_CHANNEL_CODE)
				fua_vcc->tct = (tct_entry_t *)
					(f_p->ext_tct_base +
					(i - MAX_INTERNAL_CHANNEL_CODE - 1)
					 * sizeof(tct_entry_t));
			else
				fua_vcc->tct = (tct_entry_t *)
					(f_p->int_tct_base +
						i * sizeof(tct_entry_t));
			break;
		}
	}
	if (i == fua_info->max_channel) {
		free_bds(&f_p->bd_pool, fua_vcc->txbase,
				fua_info->bd_per_channel);
		return -EFAULT;
	}

	tct = fua_vcc->tct;
	out_be32(&tct->attr, 0);
	setbits32(&tct->attr, TCT_ENTRY_ATTR_GBL);
	if (fua_vcc->avcf)
		setbits32(&tct->attr, TCT_ENTRY_ATTR_AVCF);
	TCT_SET_BO(tct);
	TCT_SET_ATT(tct, fua_vcc->traffic_type);
	TCT_SET_INTQ(tct, fua_vcc->tx_intq_num);
	TCT_SET_AAL(tct, fua_vcc->aal);

	out_be16(&tct->tbdcnt, 0);
	out_be16(&tct->tbd_offset, 0);
	out_be32(&tct->rate, 0);
	pcr_calc(tx_qos->max_pcr, tx_qos->pcr,
			fua_dev->cps, &pcr, &pcr_fraction);

	if (!pcr && !pcr_fraction) {
		pcr = 1;
		pcr_fraction = 140;
	}
	TCT_SET_PCR(tct, pcr);
	TCT_SET_PCR_FRACTION(tct, pcr_fraction);
	out_be16(&tct->apclc, 0);
	MK_CELL_HEADER(tct, vcc->vpi, vcc->vci, 0, 0);
	out_be16(&tct->pmt_tbd_base, 0);
	out_be16(&tct->tbd_base_oths, 0);
	TCT_SET_TBD_BASE(tct, virt_to_phys(fua_vcc->txbase));

	setbits16(&tct->tbd_base_oths, 0x00a);
	clrbits16(&tct->tbd_base_oths, 1);

	skb_queue_head_init(&fua_vcc->tx_list);
	skb_queue_head_init(&fua_vcc->pop_list);

	fua_vcc->vcc = vcc;

	/* start this channel. Put this channel into APC scheduling table */
	if (!fua_vcc->avcf) {
		atm_transmit(fua_vcc, 0, 0, 0);
	}

	return 0;
}

void close_tx(struct atm_vcc *vcc)
{
	struct fua_private *f_p;
	struct fua_vcc *fua_vcc;
	struct sk_buff *skb;

	f_p = ((struct fua_device *)(vcc->dev->dev_data))->fua_priv;
	fua_vcc = (struct fua_vcc *)(vcc->dev_data);

	/* stop this channel */
	TCT_STP(fua_vcc->tct);

	while ((skb = skb_dequeue(&fua_vcc->tx_list)) != NULL) {
		if (vcc->pop)
			vcc->pop(vcc, skb);
		else
			kfree_skb(skb);
	}
	while ((skb = skb_dequeue(&fua_vcc->pop_list)) != NULL) {
		if (vcc->pop)
			vcc->pop(vcc, skb);
		else
			kfree_skb(skb);
	}

	free_bds(&f_p->bd_pool, fua_vcc->txbase, f_p->fua_info->bd_per_channel);

	f_p->tx_vcc[fua_vcc->tx_cc] = NULL;
	fua_vcc->tx_cc = 0;
	fua_vcc->tct = NULL;

	return;
}

void dump_rct(rct_entry_t * rct, u32 aal)
{
	fua_dump("Receive Connection Table: %p\n", rct);
	fua_dump("\tattr:\t0x%x\n", rct->attr);
	fua_dump("\trxdbptr:\t0x%x\n", rct->rxdbptr);
	fua_dump("\tcts_h:\t0x%x\n", rct->cts_h);
	fua_dump("\tcts_l:\t0x%x\n", rct->cts_l);
	fua_dump("\trbd_offset:\t0x%x\n", rct->rbd_offset);
	if (aal == ATM_AAL5) {
		aal5_rct_t *aal5;
		aal5 = &rct->aal5;
		fua_dump("\taal5 specific: %p\n", aal5);
		fua_dump("\t\ttml:\t0x%x\n", aal5->tml);
		fua_dump("\t\trx_crc:\t0x%x\n", aal5->rx_crc);
		fua_dump("\t\trbdcnt:\t0x%x\n", aal5->rbdcnt);
		fua_dump("\t\tres:\t0x%x\n", aal5->res);
		fua_dump("\t\tattr:\t0x%x\n", aal5->attr);
	}
	fua_dump("\tmrblr:\t0x%x = %d\n", rct->mrblr, rct->mrblr);
	fua_dump("\tpmt_rbd_base:\t0x%x\n", rct->pmt_rbd_base);
	fua_dump("\trbd_base_pm:\t0x%x\n", rct->rbd_base_pm);
}

int open_rx(struct atm_vcc *vcc)
{
	struct fua_private *f_p;
	struct fua_info *fua_info;
	struct fua_device *fua_dev;
	struct phy_info *phy_info;
	struct fua_vcc *fua_vcc;
	rct_entry_t *rct;
	struct qe_bd *bd;
	struct atm_trafprm *rx_qos;
	int i, j;
	int err = 0;

	fua_dev = (struct fua_device *)(vcc->dev->dev_data);
	phy_info = fua_dev->phy_info;
	f_p = fua_dev->fua_priv;
	fua_info = f_p->fua_info;
	fua_vcc = (struct fua_vcc *)vcc->dev_data;
	rx_qos = &vcc->qos.rxtp;

	if ((rx_qos->traffic_class == ATM_CBR)
		|| (rx_qos->traffic_class == ATM_UBR)
		|| (rx_qos->traffic_class == ATM_ANYCLASS)) {
		fua_vcc->traffic_type = PCR;
	} else {
		fua_debug("rx_qos->traffic_class: 0x%x\n",
			rx_qos->traffic_class);
		return -EINVAL;
	}

	fua_vcc->rx_intq_num = INT_RX_QUE;
	fua_vcc->rxbase = alloc_bds(&f_p->bd_pool, fua_info->bd_per_channel);
	if (!fua_vcc->rxbase) {
		fua_debug("failed in alloc_bds num:0x%x\n",
			fua_info->bd_per_channel);
		return -ENOMEM;
	}
	fua_vcc->rxcur = bd = fua_vcc->rxbase;
	fua_vcc->first = NULL;
	for (i = 0; i < fua_info->bd_per_channel; i++) {
		bd->status = 0;
		bd->status |= (AAL5_RXBD_ATTR_E | AAL5_RXBD_ATTR_I);
		bd->length = 0;
		bd->buf = (u32) kzalloc(fua_vcc->rbuf_size, GFP_DMA);
		if (!bd->buf) {
			fua_debug("fail to alloc static rx buf\n");
			goto out;
		}
		bd->buf = virt_to_phys((void *)bd->buf);
		if (i < (fua_info->bd_per_channel - 1))
			bd++;
	}
	bd->status |= AAL5_RXBD_ATTR_W;

	for (j = 2; j < fua_info->max_channel; j++) {
		/* The first two channel are reserved according to UM */
		if (!f_p->rx_vcc[j]) {
			f_p->rx_vcc[j] = vcc;
			fua_vcc->rx_cc = j;
			if (j > MAX_INTERNAL_CHANNEL_CODE)
				fua_vcc->rct = (rct_entry_t *)
					(f_p->ext_rct_base +
					(j - MAX_INTERNAL_CHANNEL_CODE - 1)
					 * sizeof(rct_entry_t));
			else
				fua_vcc->rct = (rct_entry_t *)
					(f_p->int_rct_base +
						j * sizeof(rct_entry_t));
			break;
		}
	}
	if (j == fua_info->max_channel) {
		fua_debug("failed in alloc rct j:0x%x\n", j);
		err = -ENOMEM;
		goto out;
	}

	rct = fua_vcc->rct;
	out_be32(&rct->attr, RCT_ENTRY_ATTR_GBL);
	RCT_SET_BO(rct);
	RCT_SET_INTQ(rct, fua_vcc->rx_intq_num);
	RCT_SET_AAL(rct, fua_vcc->aal);

	out_be16(&rct->aal5.attr, 0);
	out_be16(&rct->aal5.res, 0);
	setbits16(&rct->aal5.attr, AAL5_RCT_RXFM | AAL5_RCT_RXBM);

	out_be16(&rct->rbd_offset, 0);
	out_be16(&rct->mrblr, fua_vcc->rbuf_size);
	out_be16(&rct->pmt_rbd_base, 0);
	out_be16(&rct->rbd_base_pm, 0);
	RCT_SET_PMT(rct, 0);
	RCT_SET_RBD_BASE(rct, virt_to_phys(fua_vcc->rxbase));
	clrbits16(&rct->rbd_base_pm, 0x1);

	addr_comp_lookup_add(f_p->addr_tbl, fua_dev->u_id, phy_info->phy_id, vcc->vpi,
				vcc->vci, fua_vcc->rx_cc, 0);

	skb_queue_head_init(&fua_vcc->rx_list);
	fua_vcc->vcc = vcc;
dump_rct(rct, ATM_AAL5);
	return 0;
out:
	bd = fua_vcc->rxbase;
	for (j = 0; j < i; j++) {
		if (bd->buf)
			kfree((void *)phys_to_virt(bd->buf));
		bd->buf = 0;
		bd++;
	}
	free_bds(&f_p->bd_pool, fua_vcc->rxbase, fua_info->bd_per_channel);

	return err;
}

void close_rx(struct atm_vcc *vcc)
{
	struct fua_private *f_p;
	struct fua_info *fua_info;
	struct fua_device *fua_dev;
	struct phy_info *phy_info;
	struct fua_vcc *fua_vcc;
	struct qe_bd *bd;
	int i;


	fua_dev = (struct fua_device *)(vcc->dev->dev_data);
	phy_info = fua_dev->phy_info;
	f_p = fua_dev->fua_priv;
	fua_info = f_p->fua_info;
	fua_vcc = (struct fua_vcc *)vcc->dev_data;

	addr_comp_lookup_remove(f_p->addr_tbl, fua_dev->u_id,
				phy_info->phy_id, vcc->vpi, vcc->vci);
	skb_queue_purge(&fua_vcc->rx_list);
	bd = fua_vcc->rxbase;
	for (i = 0; i < fua_info->bd_per_channel; i++) {
		if (bd->buf)
			kfree((void *)phys_to_virt(bd->buf));
		bd->buf = 0;
		bd++;
	}
	free_bds(&f_p->bd_pool, fua_vcc->rxbase, fua_info->bd_per_channel);
	f_p->rx_vcc[fua_vcc->rx_cc] = NULL;
	fua_vcc->rx_cc = 0;
	fua_vcc->rct = NULL;

	return;
}

enum tran_res {
	TRAN_OK,
	TRAN_DIE,
	TRAN_FAIL,
};

/*
 * Any failed skb will be requeue no matter whether parts of it has been sent
 * imitate the way of skb_copy_datagram_iovec() to handle skb_shinfo(skb)->nr_frags
 * and skb_shinfo(skb)->frag_list
 */
enum tran_res do_tx(struct sk_buff *skb)
{
	int i;
	struct qe_bd *bd;
	struct atm_vcc *vcc;
	struct fua_vcc *fua_vcc;

	vcc = ATM_SKB(skb)->vcc;
	fua_vcc = (struct fua_vcc *)(vcc->dev_data);
	if (!skb->len || skb->len > ATM_MAX_AAL5_PDU || !skb->data) {
		printk("failed skb->len:%d\n", skb->len);
		if (vcc->pop)
			vcc->pop(vcc, skb);
		else
			dev_kfree_skb_any(skb);
		return TRAN_DIE;
	}

	bd = get_free_tx_bd(fua_vcc);
	if (bd == NULL) {
		fua_debug("no bd\n");
		return TRAN_FAIL;
	}
	bd->buf = dma_map_single(NULL, skb->data, skb->len, DMA_TO_DEVICE);
	bd->length = skb->len;
	bd->status = ((bd->status & AAL5_TXBD_ATTR_W) | AAL5_TXBD_ATTR_I);
	if (!skb_shinfo(skb)->nr_frags && !skb_shinfo(skb)->frag_list) {
		bd->status |= (AAL5_TXBD_ATTR_L | AAL5_TXBD_ATTR_R);
		flush_dcache_range((size_t) bd,
				(size_t) (bd + sizeof(struct qe_bd)));
		if (fua_vcc->avcf)
			atm_transmit(fua_vcc, 0, 0, 0);
	} else {
		bd->status |= AAL5_TXBD_ATTR_R;
		flush_dcache_range((size_t) bd,
				(size_t) (bd + sizeof(struct qe_bd)));

		if (skb_shinfo(skb)->nr_frags) {
			fua_debug("A sk_frag got\n");
			for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
				bd = get_free_tx_bd(fua_vcc);
				if (bd == NULL)
					return TRAN_FAIL;
				bd->buf =
				 ((page_to_pfn(skb_shinfo(skb)->frags[i].page)
					 << PAGE_SHIFT) +
					skb_shinfo(skb)->frags[i].page_offset);
				bd->length = skb_shinfo(skb)->frags[i].size;
				bd->status = ((bd->status & AAL5_TXBD_ATTR_W)
							| AAL5_TXBD_ATTR_I);
				if (i != (skb_shinfo(skb)->nr_frags - 1)
					|| skb_shinfo(skb)->frag_list
					|| skb->next) {
					if (skb->next)
						fua_debug("the skb in list\n");
					bd->status |= AAL5_TXBD_ATTR_R;
				} else
					bd->status |=
						(AAL5_TXBD_ATTR_L |
						AAL5_TXBD_ATTR_R);
				if (fua_vcc->avcf)
					atm_transmit(fua_vcc, 0, 0, 0);
			}
		}
		if (skb_shinfo(skb)->frag_list) {
			/* This should be a bug ?? */
			struct sk_buff *list = skb_shinfo(skb)->frag_list;

			fua_debug("a listed frag_list\n");
			for (; list; list = list->next) {
				if (do_tx(list) != TRAN_OK)
					return TRAN_FAIL;
			}
		}
	}

	return TRAN_OK;
}

void discard_rx_bd(struct fua_vcc *fua_vcc, struct qe_bd * bd)
{
	if (bd == fua_vcc->rxcur) {
		/* rxbd ring is full */
		bd->status = ((bd->status & AAL5_RXBD_ATTR_W)
				| AAL5_RXBD_ATTR_I | AAL5_RXBD_ATTR_E);
		bd->length = 0;
		bd = bd_get_next(bd, fua_vcc->rxbase, AAL5_RXBD_ATTR_W);
	}
	while (bd != fua_vcc->rxcur) {
		bd->status = ((bd->status & AAL5_RXBD_ATTR_W)
				| AAL5_RXBD_ATTR_I | AAL5_RXBD_ATTR_E);
		bd->length = 0;
		bd = bd_get_next(bd, fua_vcc->rxbase, AAL5_RXBD_ATTR_W);
	}
}

/* bd is the last bd of the frame */
static void do_rx(struct fua_private *fua_priv, u32 channel_code, struct qe_bd * bd)
{
	struct atm_vcc *vcc;
	struct fua_vcc *fua_vcc;

	u8 *ptr;
	int frame_len;
	int sum_of_len;
	struct qe_bd *bd_tmp;
	struct sk_buff *skb;

	vcc = fua_priv->rx_vcc[channel_code];
	fua_vcc = (struct fua_vcc *)(vcc->dev_data);

	bd_tmp = fua_vcc->first;
	fua_vcc->first = NULL;
	frame_len = bd->length;
	skb = atm_alloc_charge(vcc, frame_len, GFP_ATOMIC);
	if (!skb) {
		fua_debug("Alloc sk_buff failed\n");
		discard_rx_bd(fua_vcc, bd);
		atomic_inc(&vcc->stats->rx_drop);
		return;
	}
	ATM_SKB(skb)->vcc = vcc;
	ptr = skb_put(skb, frame_len);
	sum_of_len = 0;
	while (bd_tmp != bd) {
		sum_of_len += bd_tmp->length;
		if (sum_of_len > frame_len) {
			memcpy(ptr, phys_to_virt(bd_tmp->buf),
				bd_tmp->length - (sum_of_len - frame_len));
			ptr += (bd_tmp->length - (sum_of_len - frame_len));
		} else {
			memcpy(ptr, phys_to_virt(bd_tmp->buf), bd_tmp->length);
			ptr += bd_tmp->length;
		}
		bd_tmp->length = 0;
		bd_tmp->status = (AAL5_RXBD_ATTR_E
					| AAL5_RXBD_ATTR_I
					| (bd_tmp->status & AAL5_RXBD_ATTR_W));
		bd_tmp =
			bd_get_next(bd_tmp, fua_vcc->rxbase, AAL5_RXBD_ATTR_W);
	}
	if (sum_of_len < frame_len)
		memcpy(ptr, phys_to_virt(bd_tmp->buf), frame_len - sum_of_len);
	bd_tmp->length = 0;
	bd_tmp->status = (AAL5_RXBD_ATTR_E
				| AAL5_RXBD_ATTR_I
				| (bd_tmp->status & AAL5_RXBD_ATTR_W));
	vcc->push(vcc, skb);
	atomic_inc(&vcc->stats->rx);

	return;
}

static void handle_intr_entry(struct fua_private *fua_priv, intr_que_entry_t * entry)
{
	u16 intr_attr;
	struct atm_vcc *vcc;
	struct fua_vcc *fua_vcc;
	struct qe_bd *bd;

	intr_attr = entry->attr;
	if (intr_attr & INT_QUE_ENT_ATTR_TBNR) {
		fua_warning("channel %d got tx buffer no ready intr\n",
			entry->channel_code);
		/*
		 * At here, the tx channel has been removed from APC and VCON flag cleared.
		 * To transmit again, you should issue a ATM_TRANSMIY command.
		 */
		vcc = fua_priv->rx_vcc[entry->channel_code];
		fua_vcc = (struct fua_vcc *)(vcc->dev_data);
		discard_rx_bd(fua_vcc, fua_vcc->rxcur);
	}
	if (intr_attr & INT_QUE_ENT_ATTR_BSY) {
		printk("channel %d got busy intr because BDs are inadequate\n",
			entry->channel_code);
		/* At this point, the channel halted. RXBD ring full. need do somthing */
		fua_debug("intr_entry->attr: 0x%x\n", entry->attr);
		vcc = fua_priv->rx_vcc[entry->channel_code];
		if (vcc) {
			atomic_inc(&vcc->stats->rx_drop);
			fua_debug("drop a rx\n");
		}
	}
	if (intr_attr & INT_QUE_ENT_ATTR_TXB) {
		struct sk_buff *skb;
		fua_debug("channel %d transmits a tx buffer\n",
				entry->channel_code);
		vcc = fua_priv->tx_vcc[entry->channel_code];
		fua_vcc = (struct fua_vcc *)(vcc->dev_data);

		skb = skb_dequeue(&fua_vcc->pop_list);
		if (skb) {
			if (vcc->pop)
				vcc->pop(vcc, skb);
			else
				dev_kfree_skb_any(skb);
		}
	}
	if (intr_attr & INT_QUE_ENT_ATTR_RXB) {
		int i = 0;
		fua_debug("channel %d revieves a rx buffer\n",
				entry->channel_code);
		vcc = fua_priv->rx_vcc[entry->channel_code];
		fua_vcc = (struct fua_vcc *)(vcc->dev_data);

		while (!(fua_vcc->rxcur->status & AAL5_RXBD_ATTR_E)) {
			bd = fua_vcc->rxcur;
			i++;
			if (bd->status & AAL5_RXBD_ATTR_F) {
				if (fua_vcc->first != NULL) {
					fua_debug("\tmisordered first bd\n");
					discard_rx_bd(fua_vcc,fua_vcc->first);
				}
				fua_vcc->first = bd;
			}
			/* This should be done before the last bd be handled */
			fua_vcc->rxcur =
				bd_get_next(bd, fua_vcc->rxbase,
							AAL5_RXBD_ATTR_W);
			if (bd->status & AAL5_RXBD_ATTR_L) {
				if (!fua_vcc->first)
					discard_rx_bd(fua_vcc,
						fua_vcc->rxcur);
				do_rx(fua_priv, entry->channel_code, bd);
			}
		}
	}
	if (intr_attr & INT_QUE_ENT_ATTR_RXF) {
		fua_debug("channel %d recevie a whole aal5 frame\n",
				entry->channel_code);
		vcc = fua_priv->rx_vcc[entry->channel_code];
		fua_vcc = (struct fua_vcc *)(vcc->dev_data);

		bd = fua_vcc->rxcur;
		if (bd->status & AAL5_RXBD_ATTR_E)
			return;
		if (bd->status & AAL5_RXBD_ATTR_F) {
			fua_debug("received the first bd at 0x%p\n", bd);
			if (fua_vcc->first != NULL) {
				fua_debug("\tmisordered first bd\n");
				discard_rx_bd(fua_vcc,
					fua_vcc->first);
			}
			fua_vcc->first = bd;
		}

		if (!fua_vcc->first) {
			fua_debug("miss the first bd in "
					"the frame for %d channel\n",
						entry->channel_code);
			fua_vcc->rxcur =
			bd_get_next(bd, fua_vcc->rxbase, AAL5_RXBD_ATTR_W);
			/* discard all bds include until rxcur. */
			discard_rx_bd(fua_vcc, fua_vcc->rxcur);
			return;
		}
		if ((bd->status & AAL5_RXBD_ATTR_ABRT)
			|| (bd->status & AAL5_RXBD_ATTR_LNE)
			|| (bd->status & AAL5_RXBD_ATTR_CRE)) {
			/* receive a abort message */
			fua_debug("\tabort frame for %d channel\n",
					 entry->channel_code);
			fua_vcc->rxcur =
			bd_get_next(bd, fua_vcc->rxbase, AAL5_RXBD_ATTR_W);
			discard_rx_bd(fua_vcc, fua_vcc->first);
			return;
		}
		fua_vcc->rxcur =
		bd_get_next(bd, fua_vcc->rxbase, AAL5_RXBD_ATTR_W);
		do_rx(fua_priv, entry->channel_code, bd);
	}
}

void handle_intr_que(struct fua_private *fua_priv, int que, int overflow)
{
	intr_que_para_tbl_t *queue;
	intr_que_para_mth_tbl_t *mth_queue;
	intr_que_entry_t *entry;
	u16 val16;
	u32 val32;

	if (fua_priv->fua_info->mthmode) {
		mth_queue = &fua_priv->intr_que.mth_base[que];
		val32 = in_be32(&mth_queue->intq_base);
		val16 = in_be16(&mth_queue->intq_offset_out);
		entry = (intr_que_entry_t *)bus_to_virt(val32 + val16);
		while (entry->attr & INT_QUE_ENT_ATTR_V) {
			if (entry->attr & INT_QUE_ENT_ATTR_W)
				val16 = 0;
			else
				val16 += sizeof(*entry);
			out_be16(&mth_queue->intq_offset_out, val16);
			handle_intr_entry(fua_priv, entry);
			entry->channel_code = 0;
			entry->attr = (entry->attr & INT_QUE_ENT_ATTR_W);
			entry = (intr_que_entry_t *)bus_to_virt(val32 + val16);
		}
	} else {
		queue = &fua_priv->intr_que.base[que];
		val32 = in_be32(&queue->intq_base);
		entry = fua_priv->intrcur[que];
		while (entry->attr & INT_QUE_ENT_ATTR_V) {
			if (entry->attr & INT_QUE_ENT_ATTR_W)
				fua_priv->intrcur[que] =
				(intr_que_entry_t *)phys_to_virt(val32);
			else
				(fua_priv->intrcur[que])++;
			handle_intr_entry(fua_priv, entry);
			entry->channel_code = 0;
			entry->attr = (entry->attr & INT_QUE_ENT_ATTR_W);
			entry = fua_priv->intrcur[que];
		}
		if (overflow)
			out_be32(&queue->intq_entry, in_be32(&queue->intq_ptr));
	}

	return;
}

void handle_intr(struct fua_private *fua_priv)
{
	int event;
	unsigned long flags;

	spin_lock_irqsave(&fua_priv->lock, flags);
	event = fua_priv->intr_event;
	spin_unlock_irqrestore(&fua_priv->lock, flags);

	if (event & UCCE_ATM_TIRU) {
		/* only occur in transmit internal rate mode */
		fua_debug("TIRU\n");
	}
	if (event & UCCE_ATM_GRLI) {
		/* globe free buffer pool red-line interrupt */
		fua_debug("GRLI\n");
	}
	if (event & UCCE_ATM_GBPG) {
		/* globe free buffer pool busy interrupt */
		fua_debug("GBPG\n");
	}
	if (event & UCCE_ATM_GINT3) {
		fua_debug("GINT3\n");
		handle_intr_que(fua_priv, 3, 0);
	}
	if (event & UCCE_ATM_GINT2) {
		fua_debug("GINT2\n");
		handle_intr_que(fua_priv, 2, 0);
	}
	if (event & UCCE_ATM_GINT1) {
		fua_debug("GINT1\n");
		handle_intr_que(fua_priv, 1, 0);
	}
	if (event & UCCE_ATM_GINT0) {
		fua_debug("GINT0\n");
		handle_intr_que(fua_priv, 0, 0);
	}
	if (event & UCCE_ATM_INTO3) {
		fua_debug("INTO3\n");
		handle_intr_que(fua_priv, 3, 1);
	}
	if (event & UCCE_ATM_INTO2) {
		fua_debug("INTO2\n");
		handle_intr_que(fua_priv, 2, 1);
	}
	if (event & UCCE_ATM_INTO1) {
		fua_debug("INTO1\n");
		handle_intr_que(fua_priv, 1, 1);
	}
	if (event & UCCE_ATM_INTO0) {
		fua_debug("INT0\n");
		handle_intr_que(fua_priv, 0, 1);
	}
}

static void fua_tasklet(unsigned long device)
{
	int i, j, qlen;
	int res;
	struct fua_private *fua_priv;
	struct fua_vcc *fua_vcc;
	struct sk_buff *skb;

	fua_priv = (struct fua_private *)device;

	handle_intr(fua_priv);

	/* Send any stuck sk_buff */
	for (i = 0; i < fua_priv->fua_info->max_channel; i++) {
		if (fua_priv->tx_vcc[i])
			fua_vcc = (struct fua_vcc *)(fua_priv->tx_vcc[i]->dev_data);
		else
			continue;
		qlen = fua_vcc->tx_list.qlen;
		for (j = 0; j < qlen; j++) {
			skb = skb_dequeue(&fua_vcc->tx_list);
			res = do_tx(skb);
			if (res == TRAN_FAIL)
				skb_queue_tail(&fua_vcc->tx_list, skb);
			else if (res == TRAN_OK) {
				skb_queue_tail(&fua_vcc->pop_list, skb);
				atomic_inc(&fua_priv->tx_vcc[i]->stats->tx);
			}
		}
	}
}

static irqreturn_t fua_int_handler(int irq, void *dev_id)
{
	struct fua_private *f_p;
	u32 ucce, uccm;

	f_p = (struct fua_private *)dev_id;
	uccm = in_be32(f_p->uccf->p_uccm);
	ucce = in_be32(f_p->uccf->p_ucce);
	f_p->intr_event = ucce & uccm;
	out_be32(f_p->uccf->p_ucce, 0xFFFFFFFF);

	tasklet_schedule(&f_p->task);
	return IRQ_HANDLED;
}

static int fua_send(struct atm_vcc *vcc, struct sk_buff *skb)
{
	int res;
	struct fua_vcc *fua_vcc;

	fua_vcc = (struct fua_vcc *)(vcc->dev_data);
	if (!fua_vcc) {
		fua_debug("no fua_vcc\n");
		goto fail;
	}
	if (!skb) {
		fua_debug("NULL SKB\n");
		return -EINVAL;
	}
	ATM_SKB(skb)->vcc = vcc;

	/* Only support AAL5 */
	if (vcc->qos.aal != ATM_AAL5) {
		fua_debug("A non_aal5 pdu\n");
		goto fail;
	}

	if (skb->len > ATM_MAX_AAL5_PDU) {
		fua_debug("Too big aal5 pdu %d\n", skb->len);
		goto fail;
	}

	res = do_tx(skb);
	if (res == TRAN_FAIL) {
		fua_debug("requeue vpi:%d vci:%d\n", vcc->vpi, vcc->vci);
		skb_queue_tail(&fua_vcc->tx_list, skb);
		tasklet_schedule(&((struct fua_device *)(vcc->dev->dev_data))->fua_priv->task);
	} else if (res == TRAN_OK) {
		skb_queue_tail(&fua_vcc->pop_list, skb);
		atomic_inc(&vcc->stats->tx);
	}
	return 0;

fail:
	if (vcc->pop)
		vcc->pop(vcc, skb);
	else
		dev_kfree_skb(skb);
	return -EINVAL;
}

static int fua_open(struct atm_vcc *vcc)
{
	struct atm_dev *dev = vcc->dev;
	struct fua_device *fua_dev;
	struct fua_vcc *fua_vcc = NULL;
	int error;
	short vpi;
	int vci;
	struct fua_private *f_p;
	struct fua_info *fua_info;

	if (!test_bit(ATM_VF_PARTIAL, &vcc->flags))
		vcc->dev_data = NULL;
	fua_dev = (struct fua_device *)dev->dev_data;
	f_p = fua_dev->fua_priv;
	fua_info = f_p->fua_info;

	vpi = vcc->vpi;
	vci = vcc->vci;
	if (vci != ATM_VPI_UNSPEC && vpi != ATM_VCI_UNSPEC)
		set_bit(ATM_VF_ADDR, &vcc->flags);
	if (vcc->qos.aal != ATM_AAL5) {
		printk("The aal must be AAL5\n");
		return -EINVAL;
	}
	fua_debug("(itf %d): open %d.%d\n",
			vcc->dev->number, vcc->vpi, vcc->vci);
	if (!test_bit(ATM_VF_PARTIAL, &vcc->flags)) {
		fua_vcc = kzalloc(sizeof(struct fua_vcc), GFP_KERNEL);
		if (!fua_vcc) {
			fua_debug("alloc fua_vcc failed\n");
			return -ENOMEM;
		}
		if (vcc->qos.aal == ATM_AAL5) {
			fua_vcc->aal = AAL5;
		} else if (vcc->qos.aal == ATM_AAL0) {
			fua_vcc->aal = AAL0;
		} else if (vcc->qos.aal == ATM_AAL1) {
			fua_vcc->aal = AAL1;
		} else if (vcc->qos.aal == ATM_AAL2) {
			fua_vcc->aal = AAL2;
		} else {
			fua_debug("aal: 0x%x\n", fua_vcc->aal);
			return -1;
		}
		fua_vcc->status = 0;
		fua_vcc->avcf = 0;
		/* must be multiple of 48 bytes. */
		fua_vcc->rbuf_size = 48 * 100;

		vcc->dev_data = (void *)fua_vcc;
	}
	if (vci == ATM_VPI_UNSPEC || vpi == ATM_VCI_UNSPEC) {
		fua_debug("vci or vpi is not specified\n");
		return 0;
	}
	if ((error = open_rx(vcc))) {
		printk("open_rx failed\n");
		goto out;
	}
	if ((error = open_tx(vcc))) {
		printk("open_tx failed\n");
		goto out1;
	}

	if(atomic_read(&fua_dev->refcnt) == 0) {
		if (!(dev->phy) || dev->phy->start(dev)) {
			fua_debug("PHY failed in starting\n");
			error = -ENODEV;
			goto out2;
		}
		atomic_add(1, &fua_dev->refcnt);
	}

	set_bit(ATM_VF_READY, &vcc->flags);
	list_add(&fua_vcc->list, &fua_dev->vcc_list);
#ifdef MODULE
	__module_get(THIS_MODULE);
#endif

	error = request_irq(fua_info->uf_info.irq, fua_int_handler,
		IRQF_DISABLED | IRQF_SAMPLE_RANDOM, "fua", (void *)f_p);
	if (error)
		printk(KERN_ERR"request_irq for fua failed\n");

	return 0;

out2:
	close_tx(vcc);
out1:
	close_rx(vcc);
out:
	return error;
}

static void fua_close(struct atm_vcc *vcc)
{
	struct atm_dev *dev = vcc->dev;
	struct fua_device *fua_dev = (struct fua_device *)(dev->dev_data);
	struct fua_vcc *fua_vcc = (struct fua_vcc *)(vcc->dev_data);
	struct fua_private *f_p;
	struct fua_info *fua_info;

	f_p = fua_dev->fua_priv;
	fua_info = f_p->fua_info;

	free_irq(fua_info->uf_info.irq, (void *)f_p);

	clear_bit(ATM_VF_READY, &vcc->flags);
	fua_debug("vpi:%d vci:%d\n", vcc->vpi, vcc->vci);
	list_del(&fua_vcc->list);
	close_tx(vcc);
	close_rx(vcc);
	kfree(fua_vcc);
	clear_bit(ATM_VF_ADDR, &vcc->flags);
	if (atomic_sub_return(1, &fua_dev->refcnt) == 0) {
		if(dev->phy)
			dev->phy->stop(dev);
	}
#ifdef MODULE
	module_put(THIS_MODULE);
#endif
	return;
}

static int fua_ioctl(struct atm_dev *dev, unsigned int cmd,
				void __user * arg)
{
	fua_debug("ioctl:cmd 0x%x arg %p\n", cmd, arg);
	switch (cmd) {
	case ATM_GETLINKRATE:
		break;
	case ATM_GETNAMES:
		break;
	case ATM_GETTYPE:
		break;
	case ATM_GETESI:
		break;
	case ATM_GETADDR:
		break;
	case ATM_RSTADDR:
		break;
	case ATM_ADDADDR:
		break;;
	case ATM_DELADDR:
		break;
	case ATM_GETCIRANGE:
		break;
	case ATM_SETCIRANGE:
		break;
	case ATM_SETESI:
		break;
	case ATM_SETESIF:
		break;
	case ATM_GETSTAT:
		break;
	case ATM_GETSTATZ:
		break;
	case ATM_GETLOOP:
		break;
	case ATM_SETLOOP:
		break;
	case ATM_QUERYLOOP:
		break;
	case ATM_SETSC:
		break;
	case ATM_SETBACKEND:
		break;
	case ATM_NEWBACKENDIF:
		break;
	case ATM_ADDPARTY:
		break;
	case ATM_DROPPARTY:
		break;
	}
	if (!dev->phy->ioctl)
		return -ENOIOCTLCMD;
	return dev->phy->ioctl(dev, cmd, arg);
}

static int fua_getsockopt(struct atm_vcc *vcc, int level, int optname,
				void *optval, int optlen)
{
	return -EINVAL;
}

static int fua_setsockopt(struct atm_vcc *vcc, int level, int optname,
				void *optval, unsigned int optlen)
{
	return -EINVAL;
}

static int fua_change_qos(struct atm_vcc *vcc, struct atm_qos *qos,
				int flags)
{
	return -1;
}

/*
 * This routine was called by atm_dev_deregister
 * Generally, while call this routine,
 * all vcc must have been closed by stack, right?!
 */
static void fua_atm_device_remove(struct fua_device *fua_dev);

/* Is called by atm_dev_put which is called by atm_dev_deregisger */
void fua_dev_close(struct atm_dev *dev)
{
	if (dev->phy)
		dev->phy->stop(dev);
	if (dev->phy_data)
		suni5384_exit(dev);
	/*
	 * This function maybe called during initialization
	 * while the data structures are not initialized.
	 */
	if (dev->dev_data)
		fua_atm_device_remove(dev->dev_data);
	return;
}

static int fua_proc_read(struct atm_dev *dev, loff_t *pos, char *page)
{
	int left, count;
	struct list_head *p;
	struct fua_device *fua_dev;
	struct fua_vcc *fua_vcc;
	uni_stat_tbl_t *uni_stat;

	left = *pos;
	fua_dev = (struct fua_device *)(dev->dev_data);
	uni_stat = (uni_stat_tbl_t *)(fua_dev->fua_priv->uni_stat_base) +
					fua_dev->phy_info->phy_id;

	if (!left--)
		return sprintf(page, "QE UCC3 atm driver :) \n");
	if (!left--) {
		count = 0;
		list_for_each(p, &fua_dev->vcc_list) {
			fua_vcc = list_entry(p, struct fua_vcc, list);
			count += sprintf(page + count, "\nRECEIVE CHANNEL: "
					 "\nfua_vcc at %p"
					 "\nutopia bus: %d"
					 "\nrx_cc: %d	rx_intq_num: %d"
					 "\nrct locate at: %p"
					 "\nrxbase at: %p		rxcur at: %p		first: %p"
					 "\nrbuf_szie: %d\n",
					 fua_vcc,
					 fua_dev->u_id,
					 fua_vcc-> rx_cc,
					 fua_vcc->rx_intq_num,
					 fua_vcc->rct,
					 fua_vcc->rxbase,
					 fua_vcc->rxcur,
					 fua_vcc->first,
					 fua_vcc->rbuf_size);

			count += sprintf(page + count, "TRANSMIT CHANNEL: "
					 "\nfua_vcc at %p"
					 "\nutopia bus: %d	avcf mode: %d"
					 "\ntx_cc: %d		tx_intq_num: %d"
					 "\ntct locate at: %p"
					 "\ntxbase at: %p		txcur at:%p\n",
					 fua_vcc,
					 fua_dev->u_id,
					 fua_vcc->avcf,
					 fua_vcc->tx_cc,
					 fua_vcc->tx_intq_num,
					 fua_vcc->tct,
					 fua_vcc->txbase,
					 fua_vcc->txcur);
		}
		count += sprintf(page + count, "UNI STATISTICS: "
				 "\nutopia error: %d	lookup failure: %d\n",
				 in_be16(&uni_stat->utopiae),
				 in_be16(&uni_stat->mic_count));
		return count;
	}
	return 0;
}

static const struct atmdev_ops ops = {
	dev_close: fua_dev_close,
	open: fua_open,
	close: fua_close,
	ioctl: fua_ioctl,
	getsockopt: fua_getsockopt,
	setsockopt: fua_setsockopt,
	send: fua_send,
	phy_put: NULL,
	phy_get: NULL,
	change_qos: fua_change_qos,
	proc_read: fua_proc_read,
#ifdef MODULE
	owner:THIS_MODULE,
#endif
};

/*
 * This function
 * 1. create atm_dev
 * 2. Initialize correspondin phy dev and get phy info
 * 3. get corresponding upc slot info
 */
static int fua_atm_device_create(struct fua_private *f_p, struct device_node *phy_node)
{
	struct atm_dev *dev;
	struct fua_info *fua_info = f_p->fua_info;
	struct fua_device *fua_dev;
	struct phy_info *phy_info;
	struct upc_slot_tx *upc_slot_tx;
	struct upc_slot_rx *upc_slot_rx;
	int err;

	dev = atm_dev_register("Freescale UCC ATM", &ops, -1, NULL);
	if (dev == NULL)
		return -ENOMEM;

	dev->ci_range.vpi_bits = MAX_VPI_BITS;
	dev->ci_range.vci_bits = MAX_VCI_BITS;
	dev->link_rate = ATM_OC3_PCR;

	phy_info = kzalloc(sizeof(struct phy_info), GFP_KERNEL);
	if (phy_info == NULL) {
		err = -ENOMEM;
		goto out;
	}

	if ((err = suni5384_init(dev, phy_node, &phy_info->upc_slot,
			 &phy_info->port_width, &phy_info->phy_id,
			 &phy_info->line_bitr, &phy_info->max_bitr,
			 &phy_info->min_bitr)) != 0) {
		goto out1;
	}

	phy_info->prio_level = 2;
	phy_info->max_iteration = 3;
	phy_info->scheduler_mode = 0;

	if(phy_info->phy_id > fua_info->phy_last)
		fua_info->phy_last = phy_info->phy_id;

	if (fua_info->upc_slot_tx[phy_info->upc_slot] == NULL) {
		upc_slot_tx = kzalloc(sizeof(struct upc_slot_tx),GFP_KERNEL);
		if (upc_slot_tx == NULL) {
			err = -ENOMEM;
			goto out1;
		}
		upc_slot_tx->slot = phy_info->upc_slot;
		upc_slot_tx->tmp = 0;
		upc_slot_tx->tsp = 0;
		upc_slot_tx->tb2b = 1;
		upc_slot_tx->tehs = 0;
		upc_slot_tx->tudc = 0;
		upc_slot_tx->txpw = phy_info->port_width;
		upc_slot_tx->tpm = 0;
		fua_info->upc_slot_tx[phy_info->upc_slot] = upc_slot_tx;
	} else {
		upc_slot_tx = fua_info->upc_slot_tx[phy_info->upc_slot];
		upc_slot_tx->tmp = 1;
		upc_slot_tx->tsp = 0;
	}

	if (fua_info->upc_slot_rx[phy_info->upc_slot] == NULL) {
		upc_slot_rx = kzalloc(sizeof(struct upc_slot_rx),GFP_KERNEL);
		if(upc_slot_rx == NULL) {
			err = -ENOMEM;
			goto out2;
		}
		upc_slot_rx->slot = phy_info->upc_slot;
		upc_slot_rx->rehs = 0;
		upc_slot_rx->rmp = 0;
		upc_slot_rx->rb2b = 1;
		upc_slot_rx->rudc = 0;
		upc_slot_rx->rxpw = phy_info->port_width;
		upc_slot_rx->rxp = 0;
		fua_info->upc_slot_rx[phy_info->upc_slot] = upc_slot_rx;
	} else {
		upc_slot_rx = fua_info->upc_slot_rx[phy_info->upc_slot];
		upc_slot_rx->rmp = 1;
	}

	fua_dev = kzalloc(sizeof(struct fua_device), GFP_KERNEL);
	if (fua_dev == NULL) {
		err = -ENOMEM;
		goto out3;
	}
	fua_dev->u_id = fua_info->uf_info.ucc_num % 2 ? 1 : 0;
	fua_dev->dev = dev;
	fua_dev->phy_info = phy_info;
	fua_dev->fua_priv = f_p;
	INIT_LIST_HEAD(&fua_dev->vcc_list);
	list_add(&fua_dev->list, &f_p->dev_list);
	atomic_set(&fua_dev->refcnt, 0);
	dev->dev_data = fua_dev;

	return 0;
out3:
	kfree(upc_slot_rx);
out2:
	kfree(upc_slot_tx);
out1:
	kfree(phy_info);
out:
	atm_dev_deregister(dev);
	return err;
}

/* All the connection on this device should be closed */
static void fua_atm_device_remove(struct fua_device *fua_dev)
{
	struct phy_info *phy_info;

	list_del(&fua_dev->list);
	phy_info = fua_dev->phy_info;
	kfree(fua_dev);
	kfree(phy_info);

	return;
}

static int fua_priv_init(struct fua_private * f_p)
{
	struct fua_info *fua_info = f_p->fua_info;
	struct list_head *p, *n;
	struct fua_device *fua_dev;
	struct phy_info *phy_info;
	int i, j;
	int err = 0;
	u8 bd_base_ext;
	u16 ucc_mode, gmode;

	if(bd_pool_init(&f_p->bd_pool,fua_info->max_bd))
		return -EFAULT;

	i = j = 0;
	list_for_each_safe(p, n, &f_p->dev_list) {
		i++;
		fua_dev = list_entry(p, struct fua_device, list);
		phy_info = fua_dev->phy_info;
		err = apc_init(&f_p->apc_tbl_base[phy_info->phy_id], phy_info,
				&fua_dev->cps, ATM_CBR);
		if (err) {
			atm_dev_deregister(fua_dev->dev);
			j++;
		}
	}
	if (i == j)
		goto out;

	if (addr_comp_lookup_init(f_p->addr_tbl, fua_info->vpc_info,
					fua_info->vc_mask, &fua_info->vct_exp))
		goto out1;

	for (i = 0; i < fua_info->max_intr_que; i++) {
		if(fua_info->mthmode) {
			if(intq_mth_init(&f_p->intr_que.mth_base[i],
					fua_info->intr_ent_per_que,
					fua_info->intr_threshold))
				goto out2;
		 } else {
			if(intq_nmth_init(&f_p->intr_que.base[i],
					&f_p->intrcur[i],
					fua_info->intr_ent_per_que,
					fua_info->intr_threshold))
				goto out2;
		}
	}

	bd_base_ext = virt_to_phys((void *)(f_p->bd_pool.head)) >> 24;
	ucc_mode = gmode = 0;
	gmode = (GMODE_GBL | GMODE_ALM_ADD_COMP);
	if (f_p->fua_info->uf_info.ucc_num % 2)
		ucc_mode |= 1 << ffz(UCC_MODE_UID);
	err = ucc_atm_pg_init(f_p->ucc_para_pg, fua_info->mthmode,
				fua_info->threads, f_p->subblock,
				f_p->oam_ch_rct_ptr, ucc_mode, gmode, 53,
				f_p->addr_tbl_offset, fua_info->vci_filter,
				f_p->apc_tbl_offset, f_p->intr_que_offset,
				f_p->uni_stat_offset, bd_base_ext, 0,
				f_p->int_rct_offset, f_p->int_tct_offset,
				f_p->ext_rct_base, f_p->ext_tct_base);

	if (err)
		goto out2;

	return 0;

out2:
	for (j = i - 1; j > 0; j--) {
		intq_exit((void *)&f_p->intr_que.base[i],
				fua_info->intr_ent_per_que);
	}

	addr_comp_lookup_exit(f_p->addr_tbl, f_p->fua_info->vct_exp);
out1:
	list_for_each_safe(p, n, &f_p->dev_list) {
		fua_dev = list_entry(p, struct fua_device, list);
		phy_info = fua_dev->phy_info;
		apc_exit(&f_p->apc_tbl_base[phy_info->phy_id], phy_info);
		atm_dev_deregister(fua_dev->dev);
	}
out:
	bd_pool_exit(&f_p->bd_pool);
	return err;
}

static void fua_priv_exit(struct fua_private *f_p)
{
	struct fua_info *fua_info = f_p->fua_info;
	struct fua_device *fua_dev;
	struct list_head *p, *n;
	struct phy_info *phy_info;
	int i;

	ucc_atm_pg_exit(f_p->ucc_para_pg, fua_info->mthmode,
			fua_info->threads);

	for (i = 0; i < fua_info->max_intr_que; i++)
		intq_exit((void *)&f_p->intr_que.base[i],
				fua_info->intr_ent_per_que);

	addr_comp_lookup_exit(f_p->addr_tbl, f_p->fua_info->vct_exp);

	list_for_each_safe(p, n, &f_p->dev_list) {
		fua_dev = list_entry(p, struct fua_device, list);
		phy_info = fua_dev->phy_info;
		apc_exit(&f_p->apc_tbl_base[phy_info->phy_id], phy_info);
	}

	bd_pool_exit(&f_p->bd_pool);
	return;
}

static int fua_struct_init(struct fua_private *f_p)
{
	struct fua_info *fua_info = f_p->fua_info;
	unsigned long offset;
	int err,i;

	spin_lock_init(&f_p->lock);
	f_p->subblock = ucc_fast_get_qe_cr_subblock(fua_info->uf_info.ucc_num);

	if (fua_info->max_channel > MAX_INTERNAL_CHANNEL_CODE) {
		dma_addr_t addr;
		i = fua_info->max_channel - MAX_INTERNAL_CHANNEL_CODE;
		f_p->ext_rct_base = (u32)dma_alloc_coherent(NULL, sizeof(rct_entry_t) * i, &addr, GFP_DMA);
		if (!f_p->ext_rct_base) {
			err = -ENOMEM;
			goto out;
		}
		f_p->ext_tct_base = (u32)dma_alloc_coherent(NULL, sizeof(tct_entry_t) * i, &addr, GFP_DMA);
		if (!f_p->ext_tct_base) {
			err = -ENOMEM;
			goto out1;
		}
		i = MAX_INTERNAL_CHANNEL_CODE;
	} else
		i = fua_info->max_channel;
	offset = qe_muram_alloc(sizeof(rct_entry_t) * i, 32);
	if (IS_ERR_VALUE(offset)) {
		err = -ENOMEM;
		goto out2;
	}
	f_p->int_rct_offset = offset;
	f_p->int_rct_base = (u32)qe_muram_addr(offset);
	memset((void *)f_p->int_rct_base, 0x0, sizeof(rct_entry_t) * i);

	offset = qe_muram_alloc(sizeof(tct_entry_t) * i, 32);
	if(IS_ERR_VALUE(offset)) {
		err = -ENOMEM;
		goto out3;
	}
	f_p->int_tct_offset = offset;
	f_p->int_tct_base = (u32)qe_muram_addr(offset);
	memset((void *)f_p->int_tct_base, 0x0, sizeof(tct_entry_t) * i);
	f_p->oam_ch_rct_ptr = f_p->int_rct_offset;

	offset = qe_muram_alloc(sizeof(ucc_para_atm_pg_t), 0x40);
	if (IS_ERR_VALUE(offset)) {
		err = -ENOMEM;
		goto out4;
	}
	qe_issue_cmd(QE_ASSIGN_PAGE_TO_DEVICE,
		f_p->subblock, QE_CR_PROTOCOL_ATM_POS, offset);
	f_p->ucc_para_pg_offset = offset;
	f_p->ucc_para_pg = qe_muram_addr(offset);
	memset((void *)f_p->ucc_para_pg, 0x0, sizeof(ucc_para_atm_pg_t));

	offset = qe_muram_alloc(sizeof(add_lookup_tbl_t), 8);
	if (IS_ERR_VALUE(offset)) {
		err = -ENOMEM;
		goto out5;
	}
	f_p->addr_tbl_offset = offset;
	f_p->addr_tbl = qe_muram_addr(offset);
	memset((void *)f_p->addr_tbl, 0x0, sizeof(add_lookup_tbl_t));

	offset = qe_muram_alloc(sizeof(apc_para_tbl_t) *
			(fua_info->phy_last + 1), sizeof(apc_para_tbl_t));
	if (IS_ERR_VALUE(offset)) {
		err = -ENOMEM;
		goto out6;
	}
	f_p->apc_tbl_offset = offset;
	f_p->apc_tbl_base = qe_muram_addr(offset);
	memset((void *)f_p->apc_tbl_base, 0x0,
		sizeof(add_lookup_tbl_t) * (fua_info->phy_last + 1));

	offset = qe_muram_alloc(sizeof(uni_stat_tbl_t) *
			(fua_info->phy_last + 1), sizeof(uni_stat_tbl_t));
	if (IS_ERR_VALUE(offset)) {
		err = -ENOMEM;
		goto out7;
	}
	f_p->uni_stat_offset = offset;
	f_p->uni_stat_base = qe_muram_addr(offset);
	memset((void *)f_p->uni_stat_base, 0x0,
		sizeof(uni_stat_tbl_t) * (fua_info->phy_last + 1));

	f_p->tx_vcc = (struct atm_vcc **)kzalloc(sizeof(struct atm_vcc *) * fua_info->max_channel, GFP_KERNEL);
	if (f_p->tx_vcc == NULL) {
		err = -ENOMEM;
		goto out8;
	}
	f_p->rx_vcc = (struct atm_vcc **)kzalloc(sizeof(struct atm_vcc *) * fua_info->max_channel, GFP_KERNEL);
	if (f_p->tx_vcc == NULL) {
		err = -ENOMEM;
		goto out9;
	}

	offset = qe_muram_alloc(sizeof(intr_que_para_tbl_t) * fua_info->max_intr_que, 0x10);
	if (IS_ERR_VALUE(offset)) {
		err = -ENOMEM;
		goto out10;
	}
	f_p->intr_que_offset = offset;
	f_p->intr_que.base = qe_muram_addr(offset);
	memset((void *)f_p->intr_que.base, 0x0, sizeof(intr_que_para_tbl_t) * fua_info->max_intr_que);

	tasklet_init(&f_p->task, fua_tasklet, (unsigned long)f_p);
	f_p->intr_event = 0;
	f_p->interrupts = 0;

	if ((err = ucc_fast_init(&fua_info->uf_info, &f_p->uccf))) {
		goto out11;
	}

	return 0;

out11:
	tasklet_kill(&f_p->task);
	qe_muram_free(f_p->intr_que_offset);
out10:
	kfree(f_p->rx_vcc);
out9:
	kfree(f_p->tx_vcc);
out8:
	qe_muram_free(f_p->uni_stat_offset);
out7:
	qe_muram_free(f_p->apc_tbl_offset);
out6:
	qe_muram_free(f_p->addr_tbl_offset);
out5:
	qe_muram_free(f_p->ucc_para_pg_offset);
out4:
	qe_muram_free(f_p->int_tct_offset);
out3:
	qe_muram_free(f_p->int_rct_offset);
out2:
	if (fua_info->max_channel > MAX_INTERNAL_CHANNEL_CODE) {
		i = fua_info->max_channel - MAX_INTERNAL_CHANNEL_CODE;
		dma_free_coherent(NULL, sizeof(tct_entry_t) * i,
			(void *)f_p->ext_tct_base, (dma_addr_t)virt_to_bus(
						(void *)f_p->ext_tct_base));
	}
out1:
	if (fua_info->max_channel > MAX_INTERNAL_CHANNEL_CODE) {
		i = fua_info->max_channel - MAX_INTERNAL_CHANNEL_CODE;
		dma_free_coherent(NULL, sizeof(rct_entry_t) * i,
			(void *)f_p->ext_rct_base, (dma_addr_t)virt_to_bus(
						(void *)f_p->ext_rct_base));
	}
out:
	kfree(f_p);
	return err;
}

static void fua_struct_exit(struct fua_private *f_p)
{
	struct fua_info *fua_info = f_p->fua_info;
	int i;

	tasklet_kill(&f_p->task);
	qe_muram_free(f_p->intr_que_offset);
	kfree(f_p->rx_vcc);
	kfree(f_p->tx_vcc);
	qe_muram_free(f_p->uni_stat_offset);
	qe_muram_free(f_p->apc_tbl_offset);
	qe_muram_free(f_p->addr_tbl_offset);
	qe_muram_free(f_p->ucc_para_pg_offset);
	qe_muram_free(f_p->int_tct_offset);
	qe_muram_free(f_p->int_rct_offset);
	if (fua_info->max_channel > MAX_INTERNAL_CHANNEL_CODE) {
		i = fua_info->max_channel - MAX_INTERNAL_CHANNEL_CODE;
		dma_free_coherent(NULL, sizeof(tct_entry_t) * i,
			(void *)f_p->ext_tct_base, (dma_addr_t)virt_to_bus(
						(void *)f_p->ext_tct_base));
	}
	if (fua_info->max_channel > MAX_INTERNAL_CHANNEL_CODE) {
		i = fua_info->max_channel - MAX_INTERNAL_CHANNEL_CODE;
		dma_free_coherent(NULL, sizeof(rct_entry_t) * i,
			(void *)f_p->ext_rct_base, (dma_addr_t)virt_to_bus(
						(void *)f_p->ext_rct_base));
	}

	kfree(f_p);

	return;
}

static int check_phy_node(struct device_node *phy, struct device_node * ucc)
{
	struct device_node *np, *upc, *upc_slot;
	const phandle *ph;
	const unsigned int *prop;
	int ucc_num, err = -EFAULT;

	upc = upc_slot = NULL;

	prop = of_get_property(ucc, "cell-index", NULL);
	ucc_num = *prop - 1;

	ph = of_get_property(phy, "ucc-handle", NULL);
	np = of_find_node_by_phandle(*ph);
	if (np == ucc) {
		ph = of_get_property(phy, "upc-slot", NULL);
		upc_slot = of_find_node_by_phandle(*ph);
		if (upc_slot) {
			upc = of_get_parent(upc_slot);
			prop = of_get_property(upc, "device-id", NULL);
			if ((prop == NULL) || ((*prop - 1) + ucc_num) % 2) {
				printk(KERN_ERR"%s UCC PHY attach \
					to the wrong UPC\n",__FUNCTION__);
				err = -EFAULT;
			} else
				err = 0;
		}
	}

	of_node_put(np);
	of_node_put(upc_slot);
	of_node_put(upc);
	return err;
}

static int fua_probe(struct of_device *ofdev, const struct of_device_id *match)
{
	struct device *device = &ofdev->dev;
	struct device_node *np, *phy_node;
	const unsigned int *prop;
	struct resource res;
	struct fua_info * fua_info;
	struct fua_private *fua_priv = NULL;
	struct fua_device *fua_dev;
	struct list_head *n, *p;
	int ucc_num = -1;
	int i, err;

	err = 0;
	np = ofdev->node;
	of_node_get(np);
	fua_info = kzalloc(sizeof(struct fua_info), GFP_KERNEL);
	if (!fua_info) {
		return -ENOMEM;
	}
	memcpy((void *)fua_info, (void *)&fua_primary_info, sizeof(struct fua_info));

	if (upc_init())
		return -EFAULT;

	if (qe_enable_time_stamp(1))
		return -EFAULT;

	prop = of_get_property(np, "cell-index", NULL);
	if (prop)
		ucc_num = *prop - 1;
	if ((ucc_num < 0) || (ucc_num > 7)) {
		err = -ENODEV;
		goto out;
	}
	fua_info->uf_info.ucc_num = ucc_num;
	if (of_address_to_resource(np, 0, &res)) {
		err = -EINVAL;
		goto out;
	}
	fua_info->uf_info.regs = res.start;
	fua_info->uf_info.irq = irq_of_parse_and_map(np, 0);
	if (of_get_property(np, "multithread", NULL)) {
		fua_info->mthmode = 1;
		prop = of_get_property(np, "threads", NULL);
		if (prop != NULL)
			fua_info->threads = *prop;
		else
			fua_info->threads = 3;
	}
	else
		fua_info->mthmode = 0;
	of_node_put(np);

	fua_priv = kzalloc(sizeof(struct fua_private), GFP_KERNEL);
	if (fua_priv == NULL)
		goto out;
	fua_priv->fua_info = fua_info;
	INIT_LIST_HEAD(&fua_priv->dev_list);

	fua_info->phy_last = -1;
	for (phy_node = NULL;
	(phy_node = of_find_compatible_node(phy_node, "atm phy", "fua-phy"))
	!= NULL;) {
		if (!check_phy_node(phy_node, np)) {
			fua_atm_device_create(fua_priv, phy_node);
			fua_debug("Created atm device\n");
		}
	}

	/* Allocate necessary resources */
	if ((err = fua_struct_init(fua_priv)) != 0)
		goto out1;

	/*
	 * Initailize the necessary resource
	 * Needs the phy information gotten from fua_atm_device_create
	 */
	if ((err = fua_priv_init(fua_priv)) != 0)
		goto out2;

	dev_set_drvdata(device, fua_priv);

	/* Attach to upc slot */
	for (i = 0; i < UPC_SLOT_MAX_NUM; i++) {
		if (fua_info->upc_slot_tx[i]) {
			ucc_attach_upc_tx_slot(fua_info->uf_info.ucc_num,
				fua_info->upc_slot_tx[i]);
		}
		if (fua_info->upc_slot_rx[i]) {
			ucc_attach_upc_rx_slot(fua_info->uf_info.ucc_num,
				fua_info->upc_slot_rx[i]);
		}
	}

	ucc_fast_enable(fua_priv->uccf, COMM_DIR_RX_AND_TX);

	qe_issue_cmd(QE_INIT_TX_RX, fua_priv->subblock,
				QE_CR_PROTOCOL_ATM_POS, 0);

	return 0;
out2:
	fua_struct_exit(fua_priv);

out1:
	list_for_each_safe(p, n, &fua_priv->dev_list) {
		fua_dev = list_entry(p, struct fua_device, list);
		atm_dev_deregister(fua_dev->dev);
	}
out:
	kfree(fua_info);
	return err;
}

static int fua_remove(struct of_device *ofdev)
{
	struct device *device = &ofdev->dev;
	struct fua_private *fua_priv = dev_get_drvdata(device);
	struct fua_info *fua_info = fua_priv->fua_info;
	struct fua_device *fua_dev;
	struct list_head *n, *p;
	int i;

	dev_set_drvdata(device, NULL);
	for (i = 0; i < UPC_SLOT_MAX_NUM; i++) {
		if (fua_info->upc_slot_tx[i])
			ucc_detach_upc_tx_slot(fua_info->uf_info.ucc_num,
				fua_info->upc_slot_tx[i]);
		if (fua_info->upc_slot_rx[i])
			ucc_detach_upc_rx_slot(fua_info->uf_info.ucc_num,
				fua_info->upc_slot_rx[i]);
	}

	list_for_each_safe(p, n, &fua_priv->dev_list) {
		fua_dev = list_entry(p, struct fua_device, list);
		atm_dev_deregister(fua_dev->dev);
	}

	fua_priv_exit(fua_priv);
	fua_struct_exit(fua_priv);
	kfree(fua_info);

	return 0;
}

static struct of_device_id fua_match[] = {
	{
		.compatible = "fsl,qe-ucc-atm",
	},
	{},
};

MODULE_DEVICE_TABLE(of, fua_match);

static struct of_platform_driver fua_driver = {
	.name = DRV_NAME,
	.match_table = fua_match,
	.probe = fua_probe,
	.remove = fua_remove,
};

static int __init fua_init(void)
{
	return of_register_platform_driver(&fua_driver);
}

static void __exit fua_exit(void)
{
	of_unregister_platform_driver(&fua_driver);
}

module_init(fua_init);
module_exit(fua_exit);

MODULE_AUTHOR("Tony Li");
MODULE_DESCRIPTION(DRV_DESC);
MODULE_VERSION(DRV_VERSION);

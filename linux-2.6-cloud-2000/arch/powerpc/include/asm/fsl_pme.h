/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Roy Pledge, Roy.Pledge@freescale.com
 *         Geoff Thorpe, Geoff.Thorpe@freescale.com
 *
 * Description:
 * This declares the interfaces for the 8572 pattern-matcher driver, as
 * miscellaneous character devices for user-space, and a kernel API.
 *
 * This file is part of the Linux kernel
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef FSL_PME_H
#define FSL_PME_H

	/**********************/
	/* Common definitions */
	/**********************/

/* Data types and values that are shared between modules */
enum pme_deflate_type {
	deflate_type_passthru,
	deflate_type_deflate,
	deflate_type_gzip,
	deflate_type_zlib
};

/* Modes of operations (aka "activity code") */
#define PME_MAKEMODE(n)		(uint32_t)(0x01000000 | (n))
#define PME_MODE_SCAN_BIT	(uint32_t)(0x1)
#define PME_MODE_NO_SCAN_BIT	(uint32_t)(0x2)
#define PME_MODE_RESIDUE_BIT 	(uint32_t)(0x4)
#define PME_MODE_RECOVERY_BIT	(uint32_t)(0x8)

#define PME_MODE_ZLIB_BIT	(uint32_t)(0x10)
#define PME_MODE_GZIP_BIT	(uint32_t)(0x20)
#define PME_MODE_PASSTHRU_BIT	(uint32_t)(0x30)
#define PME_MODE_DEFLATE_MASK	(uint32_t)(0x30)

/* Note that (deflate|zlib|gzip)/Residue/Recover modes are not
 * supported due to bug 3826 */
enum pme_mode {
	PME_MODE_INVALID = 0,
	PME_MODE_CONTROL = PME_MAKEMODE(0x0),

	PME_MODE_DEFLATE_SCAN = PME_MAKEMODE(0x1),
	PME_MODE_DEFLATE_SCAN_RESIDUE = PME_MAKEMODE(0x5),
	PME_MODE_DEFLATE_RECOVER = PME_MAKEMODE(0xa),
	PME_MODE_DEFLATE_SCAN_RECOVER = PME_MAKEMODE(0x9),

	PME_MODE_ZLIB_SCAN = PME_MAKEMODE(0x11),
	PME_MODE_ZLIB_SCAN_RESIDUE = PME_MAKEMODE(0x15),
	PME_MODE_ZLIB_RECOVER = PME_MAKEMODE(0x1a),
	PME_MODE_ZLIB_SCAN_RECOVER = PME_MAKEMODE(0x19),

	PME_MODE_GZIP_SCAN = PME_MAKEMODE(0x21),
	PME_MODE_GZIP_SCAN_RESIDUE = PME_MAKEMODE(0x25),
	PME_MODE_GZIP_RECOVER = PME_MAKEMODE(0x2a),
	PME_MODE_GZIP_SCAN_RECOVER = PME_MAKEMODE(0x29),

	PME_MODE_PASSTHRU = PME_MAKEMODE(0x32),
	PME_MODE_PASSTHRU_SCAN = PME_MAKEMODE(0x31),
	PME_MODE_PASSTHRU_SCAN_RESIDUE = PME_MAKEMODE(0x35),
	PME_MODE_PASSTHRU_RECOVER = PME_MAKEMODE(0x3a),
	PME_MODE_PASSTHRU_SCAN_RECOVER = PME_MAKEMODE(0x39),
	PME_MODE_PASSTHRU_SCAN_RESIDUE_RECOVER = PME_MAKEMODE(0x3d),
};

static inline int pme_mode_is_residue_used(enum pme_mode mode)
{
	int val = (mode & PME_MODE_RESIDUE_BIT);
	return val;
}

static inline int pme_mode_is_recovery_done(enum pme_mode mode)
{
	int val = (mode & PME_MODE_RECOVERY_BIT);
	return val;
}

static inline int pme_mode_is_scan_done(enum pme_mode mode)
{
	int val = (mode & PME_MODE_SCAN_BIT);
	return val;
}

static inline int pme_mode_is_passthru(enum pme_mode mode)
{
	int val = ((mode & PME_MODE_DEFLATE_MASK) == PME_MODE_PASSTHRU_BIT);
	return val;
}

static inline int pme_mode_is_gzip(enum pme_mode mode)
{
	int val = ((mode & PME_MODE_DEFLATE_MASK) == PME_MODE_GZIP_BIT);
	return val;
}

static inline int pme_mode_is_zlib(enum pme_mode mode)
{
	int val = ((mode & PME_MODE_DEFLATE_MASK) == PME_MODE_ZLIB_BIT);
	return val;
}

static inline int pme_mode_is_deflate(enum pme_mode mode)
{
	int val = (!pme_mode_is_gzip(mode) && !pme_mode_is_zlib(mode) &&
		!pme_mode_is_passthru(mode));
	return val;
}

static inline void pme_mode_update_deflate_type(enum pme_mode *mode,
				       enum pme_deflate_type type)
{
	/* Clear the current type deflate type */
	*mode &= 0xFFFFFF0F;
	switch (type) {
	case deflate_type_passthru:
		*mode |= PME_MODE_PASSTHRU_BIT;
		break;
	case deflate_type_gzip:
		*mode |= PME_MODE_GZIP_BIT;
		break;
	case deflate_type_zlib:
		*mode |= PME_MODE_ZLIB_BIT;
		break;
	case deflate_type_deflate:
		/* Clearing the act code makes this a deflate
		 * code */
		break;
	default:
#ifdef __KERNEL__
		panic("Unknown deflate type %d\n", type);
#endif
		break;
	}
}

/* Flags to specify the options to set */
#define PME_PARAM_SET			0x00000001
#define PME_PARAM_SUBSET		0x00000002
#define PME_PARAM_SESSION_ID		0x00000004
#define PME_PARAM_MODE			0x00000008
#define PME_PARAM_REPORT_VERBOSITY	0x00000010
#define PME_PARAM_END_OF_SUI_ENABLE	0x00000020
/* Flag used to report status when a get is performed */
#define PME_PARAM_EXCLUSIVE		0x00000040
#define PME_PARAM_RESET_SEQ_NUMBER	0x00000080
#define PME_PARAM_RESET_RESIDUE		0x00000100

/* The high bits 0xFFFF0000 are reserved */
#define PME_PARAM_RESERVED		0xFFFF0000

/* Options set for the pme_device. 'flags' is used for "update" and "get"
 * functionality, it is ignored by constructors. */
struct pme_parameters {
	uint32_t flags;
	uint32_t session_id;
	enum pme_mode mode;
	uint16_t pattern_subset;
	uint8_t pattern_set;
	uint8_t end_of_sui_enable;
	uint8_t report_verbosity;
};

/* Define a magic value and ranges for each PM device */
#define PME_IOCTL_MAGIC 		0xB2
#define PME_COMMON_IOCTL_RANGE		0x00
#define PME_DATABASE_IOCTL_RANGE	0x10
#define PME_DEV_IOCTL_RANGE		0x20
#define PME_DMA_CTRL_IOCTL_RANGE	0x30
#define PME_CHAN_IOCTL_RANGE		0x40
#define PME_SCANNER_IOCTL_RANGE		0x70

/* Common IOCTL Values */
#define PME_IOCTL_BEGIN_EXCLUSIVE \
	_IO(PME_IOCTL_MAGIC, PME_COMMON_IOCTL_RANGE)
#define PME_IOCTL_END_EXCLUSIVE \
	_IO(PME_IOCTL_MAGIC, PME_COMMON_IOCTL_RANGE+1)
#define PME_IOCTL_RESET_SEQUENCE_NUMBER \
	_IO(PME_IOCTL_MAGIC, PME_COMMON_IOCTL_RANGE+2)
#define PME_IOCTL_RESET_RESIDUE \
	_IO(PME_IOCTL_MAGIC, PME_COMMON_IOCTL_RANGE+3)
#define PME_IOCTL_SET_CHANNEL \
	_IOW(PME_IOCTL_MAGIC, PME_COMMON_IOCTL_RANGE+4, int)
#define PME_IOCTL_FLUSH \
	_IO(PME_IOCTL_MAGIC, PME_COMMON_IOCTL_RANGE+5)
#define PME_IOCTL_GET_PARAMETERS \
	_IOR(PME_IOCTL_MAGIC, PME_COMMON_IOCTL_RANGE+6, struct pme_parameters)
#define PME_IOCTL_SET_PARAMETERS \
	_IOW(PME_IOCTL_MAGIC, PME_COMMON_IOCTL_RANGE+7, struct pme_parameters)

	/*******************/
	/* Admin interface */
	/*******************/

#define PME_CHANNEL_MAX	4
/* This structure is passed to PME_CTRL_IOCTL_MONITOR and is part of the
 * structure passed to PME_CHANNEL_IOCTL_MONITOR. When the ioctls return,
 * 'status' is populated with the error bits that are set in the interrupt
 * status register. If 'block' is zero, the ioctl returns immediately with
 * return code 0. Otherwise, the 'ignore_mask' element going into the ioctl
 * will be significant as the ioctl will wait until error bits not already
 * contained in 'ignore_mask' are asserted. If 'timeout' is non-zero, the ioctl
 * will return after that many milliseconds if there have been no new error
 * interrupts. The ioctl returns with -EINTR if a signal is received, 1 if the
 * timeout elapsed, or 0 (success) if there were non-ignored error bits. The
 * monitor logic for devices retains existing error bits in the interrupt
 * status but prevents those error bits from asserting interrupts repeatedly.
 * In particular, an error on the control device prevents channels from
 * resetting until it has been cleared (the behaviour is intentional). So any
 * bits in 'clear_mask' will cause those bits to be cleared from the interrupt
 * status. If they are also in the 'disable_mask', they'll be permanently
 * disabled from being set in the interrupt status again. */
struct pme_monitor_poll {
	uint32_t	status;
	uint32_t	ignore_mask;
	uint32_t	clear_mask;
	uint32_t	disable_mask;
	int		block;
	unsigned int	timeout_ms;
};

/******************/
/* Control device */
/******************/

/* Path to the device node */
#define PME_CTRL_NODE		"pme_ctrl"
#define PME_CTRL_PATH		"/dev/" PME_CTRL_NODE

/* Get global statistics */
struct pme_ctrl_stats {
	/* Bitmask chosen from PME_CTRL_ST*** and PME_CTRL_MIA*** values,
	 * see below */
	uint32_t	flags;
	/* Bitmask set from the same flags, indicating which of the retrieved
	 * (and reset) statistics had overflowed. */
	uint32_t	overflow;
	/* CC-space statistics */
	uint32_t	stnib;
	uint32_t	stnis;
	uint32_t	stnth1;
	uint32_t	stnth2;
	uint16_t	stnthl;
	uint16_t	stnch;
	uint16_t	stnpm;
	uint16_t	stns1m;
	uint16_t	stnpmr;
	uint16_t	stndsr;
	uint16_t	stnesr;
	uint16_t	stns1r;
	uint32_t	stnob;
	uint32_t	stdbc;
	uint32_t	stdbp;
	uint16_t	stdwc;
	uint32_t	mia_byc;
	uint32_t	mia_blc;
	uint32_t	mia_lshc0;
	uint32_t	mia_lshc1;
	uint32_t	mia_lshc2;
	uint32_t	mia_cwc0;
	uint32_t	mia_cwc1;
	uint32_t	mia_cwc2;
	uint16_t	stnths;
};
#define PME_CTRL_STNIB		((uint32_t)1 << 0)
#define PME_CTRL_STNIS		((uint32_t)1 << 1)
#define PME_CTRL_STNTH1		((uint32_t)1 << 2)
#define PME_CTRL_STNTH2		((uint32_t)1 << 3)
#define PME_CTRL_STNTHL		((uint32_t)1 << 4)
#define PME_CTRL_STNCH		((uint32_t)1 << 5)
#define PME_CTRL_STNPM		((uint32_t)1 << 6)
#define PME_CTRL_STNS1M		((uint32_t)1 << 7)
#define PME_CTRL_STNPMR		((uint32_t)1 << 8)
#define PME_CTRL_STNDSR		((uint32_t)1 << 9)
#define PME_CTRL_STNESR		((uint32_t)1 << 10)
#define PME_CTRL_STNS1R		((uint32_t)1 << 11)
#define PME_CTRL_STNOB		((uint32_t)1 << 12)
#define PME_CTRL_STDBC		((uint32_t)1 << 13)
#define PME_CTRL_STDBP		((uint32_t)1 << 14)
#define PME_CTRL_STDWC		((uint32_t)1 << 15)
#define PME_CTRL_MIA_BYC	((uint32_t)1 << 16)
#define PME_CTRL_MIA_BLC	((uint32_t)1 << 17)
#define PME_CTRL_MIA_LSHC0	((uint32_t)1 << 18)
#define PME_CTRL_MIA_LSHC1	((uint32_t)1 << 19)
#define PME_CTRL_MIA_LSHC2	((uint32_t)1 << 20)
#define PME_CTRL_MIA_CWC0	((uint32_t)1 << 21)
#define PME_CTRL_MIA_CWC1	((uint32_t)1 << 22)
#define PME_CTRL_MIA_CWC2	((uint32_t)1 << 23)
#define PME_CTRL_STNTHS		((uint32_t)1 << 24)
#define PME_CTRL_KES		(PME_CTRL_STNIB | PME_CTRL_STNIS | \
				PME_CTRL_STNTH1 | PME_CTRL_STNTH2 | \
				PME_CTRL_STNTHL | PME_CTRL_STNCH | \
				PME_CTRL_STNTHS)
#define PME_CTRL_DXE		(PME_CTRL_STNPM | PME_CTRL_STNS1M | \
				PME_CTRL_STNPMR)
#define PME_CTRL_SRE		(PME_CTRL_STNDSR | PME_CTRL_STNESR | \
				PME_CTRL_STNS1R | PME_CTRL_STNOB)
#define PME_CTRL_DEFLATE	(PME_CTRL_STDBC | PME_CTRL_STDBP | \
				PME_CTRL_STDWC)
#define PME_CTRL_FBM		(PME_CTRL_MIA_BYC | PME_CTRL_MIA_BLC | \
				PME_CTRL_MIA_LSHC0 | PME_CTRL_MIA_LSHC1 | \
				PME_CTRL_MIA_LSHC2 | PME_CTRL_MIA_CWC0 | \
				PME_CTRL_MIA_CWC1 | PME_CTRL_MIA_CWC2)

struct pme_ctrl_rev {
	uint16_t	ip_id;
	uint8_t		ip_mj;
	uint8_t		ip_mn;
	uint8_t		ip_int;
	uint8_t		ip_cfg;
};

struct __pme_fb_l {
	/* Index to the physical freelist */
	uint8_t phys_idx;
	/* Booleans */
	uint8_t is_enabled;
	uint8_t is_master;
};
struct __pme_fb_ch {
	/* Info for this channel's two virtual freelists */
	struct __pme_fb_l virt[2];
};
struct pme_ctrl_fb {
	/* The blocksizes for each of the 8 physical freelists */
	uint32_t blocksize[8];
	/* Virt-to-phys setup for the 4 PM channels */
	struct __pme_fb_ch channels[PME_CHANNEL_MAX];
};

/* Structure used to initaliate an SRE Rule Reset */
#define PME_SRE_RULE_VECTOR_SIZE 	8
struct pme_sre_reset {
	/* Rule vector reset mask (see block guide) */
	uint32_t rule_vector[PME_SRE_RULE_VECTOR_SIZE];
	/* First 32 byte entry to apply the rule vector to
	 * Range: 26 bits (0 - 67,108,864) */
	uint32_t rule_index;
	/* Number of 32 byte increments 0 - 4096 */
	uint16_t rule_increment;
	/* Number of times the reset operation should cycle
	 * Range : 0 - 16,777,216 (24 bits)*/
	uint32_t rule_repetitions;
	/* Number of cycles that must occur beween
	 * rule resets.
	 * Range: 0 - 4096 */
	uint16_t rule_reset_interval;
	/* Set to zero if incoming searches have priority
	 * over the reset process, non zero for the reset
	 * proccess to have priority over incoming searches */
	uint8_t  rule_reset_priority;
};

/* Structure used to instantiate a new channel */
struct pme_ctrl_load {
	uint8_t channel;		/* [0..3] */
	uint32_t context_table;
	uint32_t residue_table;
	uint8_t residue_size;		/* 32, 64, 96, or 128 */
	int fifo_reduced;
	uint32_t cmd_fifo;
	uint32_t not_fifo;
	uint32_t fb_fifo;
	uint32_t cmd_trigger;
	uint32_t not_trigger;
	uint16_t limit_deflate_blks;
	uint16_t limit_report_blks;
	struct __pme_ctrl_load_fb {
		uint32_t starve;
		uint32_t low;
		uint32_t high;
		uint32_t max;
		uint32_t delta;
	} fb[2];
};

/* Structure used to unload, kill, or reset a channel */
struct pme_ctrl_channel {
	/* [0..3] */
	uint8_t channel;
	enum {
		CHANNEL_UNLOAD,
		CHANNEL_KILL,
		CHANNEL_RESET
	} cmd;
	int block;
};

#define PME_CTRL_OPT_SWDB 	(uint32_t)0x00000001
#define PME_CTRL_OPT_DRCC	(uint32_t)0x00000002
#define PME_CTRL_OPT_EOSRP	(uint32_t)0x00000004
#define PME_CTRL_OPT_KVLTS	(uint32_t)0x00000008
#define PME_CTRL_OPT_MCL	(uint32_t)0x00000010

struct pme_ctrl_opts {
	uint32_t flags;
	/* Software Database Version Reg/Scratch Pad */
	uint32_t swdb;
	/* DXE Pattern Range Counter Configuration */
	uint32_t drcc;
	/* End of SUI Reaction Pointer */
	uint32_t eosrp; /* Lower 17 bits only */
	/* KES Variable Length Trigger Size */
	uint16_t kvlts;
	/* Maximum Collision Chain Depth */
	uint16_t mcl; /* Lower 15 bits only */
};

/* User-space ioctl()s */
#define PME_CTRL_IOCTL_STATS \
	_IOWR(PME_IOCTL_MAGIC, PME_DMA_CTRL_IOCTL_RANGE, struct pme_ctrl_stats)
#define PME_CTRL_IOCTL_SLEEP \
	_IOW(PME_IOCTL_MAGIC, PME_DMA_CTRL_IOCTL_RANGE+1, uint32_t)
#define PME_CTRL_IOCTL_REV \
	_IOR(PME_IOCTL_MAGIC, PME_DMA_CTRL_IOCTL_RANGE+2, struct pme_ctrl_rev)
#define PME_CTRL_IOCTL_FBM \
	_IOR(PME_IOCTL_MAGIC, PME_DMA_CTRL_IOCTL_RANGE+3, struct pme_ctrl_fb)
#define PME_CTRL_IOCTL_SRE_RESET \
	_IOW(PME_IOCTL_MAGIC, PME_DMA_CTRL_IOCTL_RANGE+4, struct pme_sre_reset)
#define PME_CTRL_IOCTL_SET_OPTS \
	_IOW(PME_IOCTL_MAGIC, PME_DMA_CTRL_IOCTL_RANGE+5, struct pme_ctrl_opts)
#define PME_CTRL_IOCTL_GET_OPTS \
	_IOWR(PME_IOCTL_MAGIC, PME_DMA_CTRL_IOCTL_RANGE+6, struct pme_ctrl_opts)
#define PME_CTRL_IOCTL_LOAD \
	_IOW(PME_IOCTL_MAGIC, PME_DMA_CTRL_IOCTL_RANGE+7, struct pme_ctrl_load)
#define PME_CTRL_IOCTL_CHANNEL \
	_IOW(PME_IOCTL_MAGIC, PME_DMA_CTRL_IOCTL_RANGE+8, \
			struct pme_ctrl_channel)
#define PME_CTRL_IOCTL_MONITOR \
	_IOW(PME_IOCTL_MAGIC, PME_DMA_CTRL_IOCTL_RANGE+9, \
			struct pme_monitor_poll)

/******************/
/* Channel device */
/******************/

#define PME_CHANNEL_NODE	"pme_channel_%d"
#define PME_CHANNEL_PATH	"/dev/" PME_CHANNEL_NODE

struct __pme_dump_fl {
	uint8_t enabled;
	uint8_t master;
	uint32_t blocksize;
	unsigned int bufs_infifo;
	unsigned int bufs_waiting;
	unsigned int bufs_owned;
	unsigned int bufs_cmdfifo;
	/* These are only relevant if 'master' is non-zero */
	unsigned int bufs_allocated;
	unsigned int bufs_hw;
};

#define FIFO_FBM	0
#define FIFO_CMD	1
#define FIFO_NOT	2
#define TABLE_RESIDUE	0
#define TABLE_CONTEXT	1
#define TABLE_NAME_LEN	24
struct pme_dump_channel {
	/* These entries are taken from a snapshot of the channel context */
	uint8_t idx;
	int killed;
	/* Info about the 3 FIFOs */
	struct __pme_dump_fifo {
		enum __pme_channel_fifo_type {
			fifo_cmd_reduced,
			fifo_cmd_normal,
			fifo_notify,
			fifo_fbm
		} type;
		void *kptr;
		size_t entry_size;
		uint32_t num;
		int block;
		uint32_t sw1, hw2, sw3, fill12, fill23;
	} fifos[3];
	/* Info about the freebuffer processing thread */
	struct __pme_dump_fbm {
		int scheduled;
		int interrupted;
		/* the 2 virtual freelists are accounted for; */
		uint8_t vfl_reset;	/* being (re)set */
		uint8_t vfl_sched;	/* has buffers to deallocate */
		uint8_t vfl_wait;	/* waiting for interrupt */
		uint8_t vfl_nosched;	/* idle */
	} fbm;
	/* Info about the two virtual freelists */
	struct __pme_dump_vfl {
		struct __pme_ctrl_load_fb params;
		uint8_t freelist_id, enabled, master;
		uint32_t blocksize;
		uint8_t scheduled, in_syncing, in_reset, synced;
		unsigned int chains_to_release;
		unsigned int num_infifo;
		unsigned int num_waiting;
		unsigned int num_owned;
		unsigned int num_input;
		unsigned int num_allocated;
		enum __pme_vfl_lowwater {
			lw_idle,	/* No low-water interrupt */
			lw_triggered,	/* Low-water interrupt received */
			lw_wait,	/* Buffers in 'release_list', waiting */
			lw_maxed	/* At the allocation limit */
		} lowwater;
	} vfl[2];
	struct __pme_dump_table {
		char unique_name[TABLE_NAME_LEN];
		unsigned int entry_size;
		unsigned int table_num;	/* zero if we're dynamic */
		unsigned int owned;
	} table[2];
	unsigned int num_objs;
	uint32_t monitor_IS, monitor_uid;
	unsigned int num_to_notify, num_access, num_sleeper, num_yawner;
	int access_waking, topdog;
	uint16_t stat_cmd_trigger, stat_not_trigger;
};

struct __pme_dump_ned {
	uint64_t virt;
	uint64_t phys;
};
struct pme_dump_freelist {
	/* 0=freelist-A, non-zero=freelist-B */
	int freelist_id;
	/* Storage for buffer pointers */
	unsigned int neds_num;
	struct __pme_dump_ned *neds;
	/* Number of buffers in physical freelist */
	uint32_t freelist_len;
	/* The on-deck pointer */
	uint64_t ondeck_virt;
	/* The buffer-size register (indicates if 'virt' is valid) */
	uint32_t bsize;
};

/* Get channel statistics */
struct pme_channel_stats {
	/* Bitmask chosen from PME_CHANNEL_ST_*** values, see below */
	uint32_t	flags;
	/* Bitmask set from the same flags, indicating which of the retrieved
	 * (and reset) statistics had overflowed. */
	uint32_t	overflow;
	/* channel-space statistics */
	uint16_t	ca_dcnt;
	uint16_t	ca_acnt;
	uint16_t	cb_dcnt;
	uint16_t	cb_acnt;
	uint16_t	truncic;
	uint32_t	rbc;
};
#define PME_CHANNEL_ST_CA		((uint32_t)3 << 0)
#define PME_CHANNEL_ST_CB		((uint32_t)3 << 2)
#define PME_CHANNEL_ST_TRUNCIC		((uint32_t)1 << 8)
#define PME_CHANNEL_ST_RBC		((uint32_t)1 << 9)

/* Channel monitoring also returns the current error-code register. When a
 * channel is first loaded, the reset counter is zero - each subsequent reset
 * increments the counter. A blocking monitoring ioctl will be woken if the
 * reset counter changes from the value supplied by the caller. Ie. this allows
 * the user to protect against races between monitoring and resets - because a
 * monitoring ioctl may not want to disable error interrupts and mask others if
 * it will apply to a freshly reset channel. */
struct pme_channel_monitor {
	struct pme_monitor_poll monitor;
	uint32_t error_code;
	uint32_t reset_counter;
};

/* User-space ioctl()s */
#define PME_CHANNEL_IOCTL_DUMP \
	_IOWR(PME_IOCTL_MAGIC, PME_CHAN_IOCTL_RANGE, struct pme_dump_channel)
#define PME_CHANNEL_IOCTL_DUMP_FREELIST \
	_IOWR(PME_IOCTL_MAGIC, PME_CHAN_IOCTL_RANGE+1, struct pme_dump_freelist)
#define PME_CHANNEL_IOCTL_STATS \
	_IOWR(PME_IOCTL_MAGIC, PME_CHAN_IOCTL_RANGE+2, struct pme_channel_stats)
#define PME_CHANNEL_IOCTL_SLEEP \
	_IOW(PME_IOCTL_MAGIC, PME_CHAN_IOCTL_RANGE+3, uint32_t)
#define PME_CHANNEL_IOCTL_WAIT \
	_IOW(PME_IOCTL_MAGIC, PME_CHAN_IOCTL_RANGE+4, uint32_t)
#define PME_CHANNEL_IOCTL_MONITOR \
	_IOW(PME_IOCTL_MAGIC, PME_CHAN_IOCTL_RANGE+5, \
			struct pme_channel_monitor)
#define PME_CHANNEL_IOCTL_CPI_BLOCK \
	_IOW(PME_IOCTL_MAGIC, PME_CHAN_IOCTL_RANGE+6, int)
#define PME_CHANNEL_IOCTL_NCI_BLOCK \
	_IOW(PME_IOCTL_MAGIC, PME_CHAN_IOCTL_RANGE+7, int)
#define PME_CHANNEL_IOCTL_FPI_BLOCK \
	_IOW(PME_IOCTL_MAGIC, PME_CHAN_IOCTL_RANGE+8, int)

	/*************************/
	/* PM database interface */
	/*************************/

#define PME_DEVICE_DATABASE_NODE	"pme_database"
#define PME_DEVICE_DATABASE_PATH	"/dev/" PME_DEVICE_DATABASE_NODE

/* The PME_DATABASE_IOCTL_SET_FREELIST ioctl sets the freelist source for output
 * from this interface. I/O is then performed using standard read/write
 * operations on the file-descriptor. */
enum pme_database_freelist {
		freelist_A = 0,
		freelist_B = 1
};
#define PME_DATABASE_IOCTL_SET_FREELIST	_IOWR(PME_IOCTL_MAGIC, \
						PME_DATABASE_IOCTL_RANGE, \
						enum pme_database_freelist)

	/************************/
	/* PM scanner interface */
	/************************/

#define PME_SCANNER_NODE		"pme_scanner"
#define PME_SCANNER_PATH		"/dev/" PME_SCANNER_NODE

/* pme_scanner_operation::cmd_flags uses these bits */
#define PME_SCANNER_CMD_SG_INPUT	0x00000001
#define PME_SCANNER_CMD_SG_DEFLATE	0x00000002
#define PME_SCANNER_CMD_SG_REPORT	0x00000004
/* allow dangerous zero-copy when root, so output may be an iovec too */
#define PME_SCANNER_CMD_ALLOW_ZC_IOVEC	0x00000008
/* indicate that this is the last data unit in a stream - allows the user to
 * reset the stream for more data */
#define PME_SCANNER_CMD_EOS		0x00000010

/* pme_scanner_operation::results[] uses these indices */
#define PME_SCANNER_DEFLATE_IDX		0
#define PME_SCANNER_REPORT_IDX		1
#define PME_SCANNER_NUM_RESULTS		2

/* pme_scanner_operation::results[].flags uses these bits */
#define PME_SCANNER_RESULT_SG		0x00000001
#define PME_SCANNER_RESULT_TRUNCATED	0x00000002
#define PME_SCANNER_RESULT_FB		0x00000004
#define PME_SCANNER_RESULT_ABORTED	0x00000008

/* Zero Copy PM Operation Structure :
 * 	This operation structure is used
 * 	to send data through the PM scanner using the
 * 	PME_EXECUTE_CMD ioctl()
 * 	If the report data and/or output data size is
 * 	specified as zero, the freebuffers will be mapped
 * 	into user space.  The user MUST free the data
 * 	by perfoming a PME_FREE_MEMORY ioctl() when done */
struct pme_scanner_operation {
	/* cmd_flags are set before the operation is executed */
	uint32_t cmd_flags;
	/* Input data */
	union {
		void *input_data;
		struct iovec *input_vector;
	};
	/* If input data is a flat buffer, the size of the buffer
	 * If input data is an iovec, the number of entries in the
	 * iovec array */
	size_t input_size;
	/* Output */
	struct pme_scanner_result_data {
		/* flags are set by the system when the operation is executed */
		uint32_t flags;
		union {
			void *data;
			struct iovec *vector;
		};
		/* If  output data is a flat buffer, the size of the buffer
		 * If output data is an iovec, the number of entries in the
		 *      iovec array
		 * If 0, freebuffers will be used for output and the value
		 *      will be set to the amount allocated on return (for
		 *      a buffer that is mapped as one area, it will be
		 *      the size of the area, for scatter/gather the size
		 *      will be the length of the iovec */
		size_t size;

		/* The size, in bytes, of the output generated */
		size_t used;

		/* Set to 0 to use freelist A, 1 for freelist B */
		uint8_t freelist_id;
		/* Error code */
		uint8_t exception_code;
	} results[PME_SCANNER_NUM_RESULTS];
	/* User Context */
	unsigned long user_data;
};

/* Structure used to collect completed asynchronous commands from the device */
struct pme_scanner_completion {
	/* Set timeout to 0 to wait forever
	 * -1 to check but not wait */
	int timeout;
	/* Maximum results to store in results array*/
	unsigned int max_results;
	/* Array that holds the completed scanner operations.
	 * This will be populated by the device driver */
	struct pme_scanner_operation *results;
	/* Number of results stored in results array
	 * This will be populated by the device driver */
	unsigned int num_results;
};

/* User-space ioctl()s */
#define PME_SCANNER_IOCTL_EXECUTE_CMD		_IOWR(PME_IOCTL_MAGIC, \
						PME_SCANNER_IOCTL_RANGE, \
						struct pme_scanner_operation)
#define PME_SCANNER_IOCTL_FREE_MEMORY		_IOW(PME_IOCTL_MAGIC, \
						PME_SCANNER_IOCTL_RANGE+1, \
						void *)
#define PME_SCANNER_IOCTL_EXECUTE_ASYNC_CMD	_IOWR(PME_IOCTL_MAGIC, \
						PME_SCANNER_IOCTL_RANGE+2, \
						struct pme_scanner_operation)
#define PME_SCANNER_IOCTL_COMPLETE_ASYNC_CMD	_IOWR(PME_IOCTL_MAGIC, \
						PME_SCANNER_IOCTL_RANGE+3, \
						struct pme_scanner_completion)

	/**************/
	/* Kernel API */
	/**************/

#ifdef __KERNEL__

/*********/
/* types */
/*********/

struct pme_channel;
struct pme_fbchain;
struct pme_callback;
struct pme_context;
struct pme_ctrl_load;

/************************/
/* SoC vs FPGA-over-PCI */
/************************/

/* When using the PCI-based CDS, we need to pass a non-NULL parameter to
 * dma_map_***() APIs. These wrappers accomplish this. */

struct device *pme_cds_dev(void);
#define DMA_MAP_SINGLE(ptr, sz, d) \
	dma_map_single(pme_cds_dev(), ptr, sz, d)
#define DMA_UNMAP_SINGLE(addr, sz, d) \
	dma_unmap_single(pme_cds_dev(), addr, sz, d)
#define DMA_MAP_PAGE(page, offset, sz, d) \
	dma_map_page(pme_cds_dev(), page, offset, sz, d)
#define DMA_UNMAP_PAGE(addr, sz, d) \
	dma_unmap_page(pme_cds_dev(), addr, sz, d)
#define DMA_MAP_SG(sg, nents, d) \
	dma_map_sg(pme_cds_dev(), sg, nents, d)
#define DMA_UNMAP_SG(sg, nents, d) \
	dma_unmap_sg(pme_cds_dev(), sg, nents, d)

/*******************/
/* Register access */
/*******************/

/* These functions provide read/write access to one of the channels' register
 * spaces or to the common control register space. Each register space is 4K,
 * and the offsets must be 4-byte aligned (ie. BUG_ON(offset&3)!!). */

u32 pme_channel_reg_get(struct pme_channel *, unsigned int offset);
void pme_channel_reg_set(struct pme_channel *, unsigned int offset,
			u32 value);
u32 pme_ctrl_reg_get(unsigned int offset);
void pme_ctrl_reg_set(unsigned int offset, u32 value);

/*************************/
/* Channel-device helper */
/*************************/

/* pme_base_device.h declares a user-space device interface for channels. A
 * common use of these devices is to allow user-space code to open a
 * corresponding handle/file-descriptor and pass it as an argument to another
 * interface on another device type (eg. binding a "pattern-matching"
 * device-handle to a given channel). Kernel code for such devices then need to
 * be able to convert user-space channel "handle"s to kernel-space channel
 * pointers - which is the role this function fulfills. It will also validate
 * that the given file is in fact a channel handle, so the caller can have
 * confidence that the function will safely return NULL unless the given handle
 * is of the right type and the corresponding channel is available. Note, the
 * file parameter can be obtained from a user-space file-descriptor integer
 * using fget(). Also, the return value has no corresponding reference count
 * (so there's nothing to release) as the file handle itself is enough to
 * ensure the channel is valid. For this reason, the return value should no
 * longer be used after calling fput(). However, objects created against the
 * channel while the handle was valid will remain valid after fput(). */
struct pme_channel *pme_device_get_channel(struct file *f);

/******************/
/* pme_channel API */
/******************/

/* These two APIs are exlusively for kernel code that uses the pattern matching
 * block (directly or through the pattern-matching layers). These should *not*
 * be used by device implementations for exposing pattern-matching and such to
 * user-space, instead those implementations should rely on the user-space
 * providing a file-based handle to a channel and obtaining the "struct
 * pme_channel" from that. See the "pme_base_device.h" header for details. */
int pme_channel_get(struct pme_channel **, unsigned int idx);
void pme_channel_put(struct pme_channel *);

/* Channel kill and reset functionality. NB, 'block' will allow the kill
 * function to sleep after putting the channel into the error state until the
 * channel can be reset. The 'block parameter to the reset function allows it
 * to sleep prior to resetting. */
int pme_channel_kill(struct pme_channel *, int block);
int pme_channel_reset(struct pme_channel *, int block);
int pme_channel_load(struct pme_ctrl_load *params);
int pme_channel_unload(u8 channel, int block);

/* Registration structure used as a context by channels for wake-ups when
 * channel errors occur. The structure is defined to allow users to instantiate
 * it within caller context structures. In particular, the callback can use
 * container_of() to obtain its context. */
struct pme_channel_err_ctx {
	/* Set this to have the callback invoked if the context is deregistered
	 * prior to a channel error */
	u8 cb_on_deregister;
	/* The callback is invoked with non-zero 'err' if a channel error
	 * occurs. If deregistering occurs prior to a channel error, this is
	 * invoked with zero 'err' if the cb_on_deregister flag is set. */
	void (*cb)(struct pme_channel_err_ctx *, int err);
	/* These variables are used within the driver - ignore. */
	u8 notified, complete;
	struct list_head list;
};

/* Register a notification context for channel errors */
int pme_channel_err_register(struct pme_channel *,
			struct pme_channel_err_ctx *);
/* Deregister. NB, this may race against a channel error, but if
 * 'cb_on_deregister' was set, then the callback is invoked exactly once
 * whether the error wins the race or not. NB, consider the following scenario,
 * channel error-handling is running on one CPU when the deregister() API is
 * called from another CPU - the 'block' parameter should be non-zero if the
 * caller does not want the API to return until the other CPU has finished
 * executing the callback (ie. it is highly unlikely there will be any real
 * blocking in this API, and it would be for a very short space of time if it
 * occurs). Certain use cases would require that, upon returning from this API,
 * the callback can no longer be running. Other use-cases OTOH may not want to
 * allow this API to sleep or yield (eg. if calling from interrupt-context), in
 * which case the 'block' parameter should be zero and the API user should be
 * aware of this case (for example, by putting any subsequent cleanup in the
 * callback itself). */
void pme_channel_err_deregister(struct pme_channel *,
			struct pme_channel_err_ctx *,
			int block);

/* Increment the reference count of an existing channel handle */
void pme_channel_up(struct pme_channel *);

#define PME_DMA_CHANNEL_FL_A_ENABLED	(u32)0x00000001
#define PME_DMA_CHANNEL_FL_B_ENABLED	(u32)0x00000010
/* Indicates the status of (virtual) freelists for this channel. The return
 * value is a bit-mask of PME_DMA_CHANNEL_FL_[A|B]_ENABLED flags. */
u32 pme_channel_freelists(struct pme_channel *);

/* Returns the constant data size of blocks for a given freelist_id. When
 * freelist output uses linked-list encoding, the actual data size per block
 * will be less. For scatter-gather output, the maximum data per block will be
 * this size. */
size_t pme_channel_freelist_blksize(struct pme_channel *, u8 freelist_id);

/******************/
/* pme_fbchain API */
/******************/

/* Returns the total data length in the chain. */
size_t pme_fbchain_length(struct pme_fbchain *chain);

/* A chain can be iterated. Initially, '_current' returns the first buffer (or
 * NULL if the chain is empty). Use '_next' to advance. Use '_reset' to restart
 * iteration at the first buffer. For scatter-gather, these APIs all behave as
 * though only data buffers were in the chain. */
void *pme_fbchain_current(struct pme_fbchain *chain);
void *pme_fbchain_next(struct pme_fbchain *chain);
void pme_fbchain_reset(struct pme_fbchain *chain);

/* Returns the amount of data in the '_current' buffer. */
size_t pme_fbchain_current_bufflen(struct pme_fbchain *chain);

/* The user should explicitly release any provided pme_fbchain objects,
 * either within the completion callback itself or deferred to a later time. */
void pme_fbchain_recycle(struct pme_fbchain *chain);

/* This enum is used to identify the type of chain. */
enum pme_fbchain_e {
	/* Unused */
	fbchain_null,
	/* The chain is a linked-list of freelist buffers */
	fbchain_ll,
	/* The chain is a scatter-gather buffer */
	fbchain_sg
};

/* Indicates the type of chain */
enum pme_fbchain_e pme_fbchain_type(struct pme_fbchain *chain);

/* Returns the maximum data size for buffers in the chain. Each buffer has this
 * much space available, and for chains provided by hardware as output, all
 * buffers will be filled to this size before a subsequent buffer is used. */
size_t pme_fbchain_max(struct pme_fbchain *chain);

/* Returns the number of buffers in the chain. For scatter-gather, this will
 * return the number of data buffers, ignoring scatter tables. */
unsigned int pme_fbchain_num(struct pme_fbchain *chain);

/* Allocte an empty chain that is compatible with 'compat'. This is invalid for
 * scatter-gather chains. Useful to _crop() and then _recycle() portions of an
 * output chain. */
struct pme_fbchain *pme_fbchain_new(struct pme_fbchain *compat,
				unsigned int gfp_flags);
/* Determine whether two chains are compatible. Returns non-zero for true. */
int pme_fbchain_is_compat(const struct pme_fbchain *chain1,
			const struct pme_fbchain *chain2);
/* Move all buffers from 'source' to 'dest' (which is already initialised, and
 * possibly non-empty). Invalid for scatter-gather chains and linked-list
 * chains that are incompatible. Returns zero on success. */
int pme_fbchain_mv(struct pme_fbchain *dest,
		struct pme_fbchain *source);
/* Make the '_current' buffer of 'source' be the new head of the chain, moving
 * all preceeding buffers in 'source' to 'dest'. Returns zero on success. */
int pme_fbchain_crop(struct pme_fbchain *dest,
		struct pme_fbchain *source);

/* This function will recycle all buffers prior to '_current'. Returns zero on
 * success (most likely failure is -ENOMEM). */
int pme_fbchain_crop_recycle(struct pme_fbchain *chain);

/***************/
/* pme_data API */
/***************/

/* This structure allows the caller to configure input and/or output data. */
struct pme_data {
	enum pme_data_type {
		/* Input types */
		data_in_normal,  /* contiguous memory buffer */
		data_in_sg,      /* scatter-gather table(s) */
		data_in_fbchain, /* a free-list chain from previous output */
		/* Output types (buffer-descriptor) */
		data_out_normal, /* contiguous memory buffer */
		data_out_sg,     /* scatter-gather table(s) */
		/* Output types (use hardware free-lists) */
		data_out_fl,     /* a buffer-chain from a freelist */
		data_out_sg_fl,  /* data_out_fl, but format scatter-gather */
	}			type;
	/* See possible flag values below */
	u32			flags;
	/* Physical address, unused by _fbchain and _fl types */
	dma_addr_t		addr;
	/* A free-list output chain, only used by data_in_fbchain */
	struct pme_fbchain *chain;
	/* 'size' is for a buffer's geometry. For _normal or _ll types, 'size'
	 * is the data-size of the/each block. For _sg, 'size' is the number of
	 * bytes in the first scatter-gather table (must be divisible by 16).
	 * For _fl and _fbchain types, 'size' is ignored. */
	u32			size;
	/* 'length' is the number of bytes of input data, or the maximum amount
	 * of output to populate. */
	u32			length;
	/* Specifies a freelist (0 or 1) */
	u8			freelist_id;
};
/* For data_***_sg types, ignore bits in the reserved area giving full IEEE1212
 * compatibility but losing offset support. Use this for foreign input or
 * output scatter-gather descriptors. */
#define PME_DATA_IEEE1212		(u32)0x00000001

/************************/
/* Module configuration */
/************************/

/* This configuration value is initialised via module parameter and required by
 * the pattern-matching layer to validate session ranges. Do not adjust this
 * value. */
extern unsigned int sre_session_ctx_num;

/******************/
/* Caller context */
/******************/

/* Completion callback data.
 *
 * The 'flags' parameter to the callback takes its values from the
 * PME_COMPLETION_*** flags.
 *
 * If the output or report data was placed into free-buffers, then the
 * 'fb_output' list contains these buffers and they need to be recycled
 * eventually using the pme_freebuffers_put() API. */

struct pme_context_callback {
	int (*completion) (struct pme_context *obj,
				u32 flags,
				u8 exception_code,
				u64 stream_id,
				struct pme_context_callback *cb,
				size_t output_used,
				struct pme_fbchain *fb_output);
	union {
		unsigned char bytes[4];
		u32 words[1];
	} ctx;
};

/*************************************/
/* flags for pme_context_io_command() */
/*************************************/
/* Invoke callback once the command has been consumed by hardware */
#define PME_FLAG_CB_COMMAND		(u32)0x00000001
/* The command is a NOP, it passes passively through the DE pipeline */
#define PME_FLAG_NOP			(u32)0x00000002
/* Set the End-Of-Stream bit in the command */
#define PME_FLAG_EOS			(u32)0x00000004
/* Use a polling interface for blocking semantics */
#define PME_FLAG_POLL			(u32)0x00000008
#define PME_CONTEXT_VALID_FLAGS		(PME_FLAG_NOP | \
					PME_FLAG_CB_COMMAND | \
					PME_FLAG_EOS | \
					PME_FLAG_POLL)

/***********************************************/
/* flags for 'pme_context_callback' completions */
/***********************************************/
/* The command wasn't consumed, there is a system level error and all callers
 * are being "kicked out" */
#define PME_COMPLETION_ABORTED		(u32)0x00000001
/* The activity code generated NULL output for this deflate/report completion */
#define PME_COMPLETION_NULL		(u32)0x00000002
/* The PME_FLAG_CB_COMMAND flag was used and this callback is command-expiry */
#define PME_COMPLETION_COMMAND		(u32)0x00000004
/* Output used free-buffer(s) */
#define PME_COMPLETION_FB		(u32)0x00000008
/* Output is scatter-gather */
#define PME_COMPLETION_SG		(u32)0x00000010
/* Output was truncated */
#define PME_COMPLETION_TRUNC		(u32)0x00000020

/* Flag set to indicate that operations on the context should not put the
 * caller to sleep */
#define PME_PARAM_DONT_SLEEP		0x80000000

enum pme_dtor {
	/* The destructor callback is being called to inform the object owner
	 * that an error has occured on the channel. The underlying context
	 * (and residue) are being automatically released, but the object
	 * destructor (via the pme_dtor_object "reason") won't be invoked until
	 * the object owner (and outstanding operations being flushed out of
	 * hardware) release outstanding references. This callback is a way for
	 * object owners to avoid performing their own err-registration
	 * directly with the channel. */
	pme_dtor_channel,
	/* The destructor callback is being invoked as the final reference to
	 * the object is released just prior to the object being destroyed. */
	pme_dtor_object
};

/* Allocate a "context" object bound to the specified channel. */
int pme_context_create(struct pme_context **obj,
		      struct pme_channel *channel,
		      const struct pme_parameters *params,
		      u64 deflate_sid, u64 report_sid,
		      unsigned int gfp_mode, void *userctx,
		      void (*dtor)(void *userctx, enum pme_dtor reason));

/* Wait for any outstanding requests to be completed then deallocate */
void pme_context_delete(struct pme_context *obj);

/* Update the context object (blocking) */
int pme_context_update(struct pme_context *obj,
		      const struct pme_parameters *params);

/* Obtains the current state of the context for all non-NULL parameters */
int pme_context_get(struct pme_context *obj,
		    struct pme_parameters *params);

/* Performs an IO command in the pattern matcher. If PME_PARAM_EXCLUSIVE is
 * specified, the hardware will block processing of all other channels when
 * this command is consumed so that subsequent commands to the same channel
 * have exclusivity. For this reason, the *last* command in an atomic series of
 * commands should *not* specify this flag. */
int pme_context_io_cmd(struct pme_context *obj, u32 flags,
		      struct pme_context_callback *cb,
		      struct pme_data *input_data,
		      struct pme_data *deflate_data,
		      struct pme_data *report_data);

/* Sets this context as the sole exclusive user of the device.  While set, no
 * other channels will be scheduled and all operations on other contexts will
 * be blocked until the exclusive mode is released */
int pme_context_set_exclusive_mode(struct pme_context *, int enable);

/* Causes a NOP to be sent via the specified context. This method will block
 * until the NOP has been processed by the device, guaranteeing that any
 * commands that were in-flight before this call have executed. */
int pme_context_flush(struct pme_context *ctx);

#endif /* defined(__KERNEL__) */

#endif /* !defined(FSL_PME_H) */

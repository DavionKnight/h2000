/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Author: Geoff Thorpe, Geoff.Thorpe@freescale.com
 *
 * Description:
 * This file is the top-level module handler for the pattern-matcher driver. It
 * implements the common-control device and associated functions such as
 * configuration, initialisation, channel-loading, error recovery, etc.
 */

#include "base_private.h"
#include "base_mem.h"

/*
 * ==================
 * Module usage notes
 * ==================
 *
 * There are various module parameters supported to configure the DMA engine,
 * FBM, and PM/Deflate blocks. The module may be set up to function as a
 * "master", in which case it will map and configure the common-control
 * registers that handle global settings and the virtual-to-physical freelist
 * mappings available to the 4 DMA channels. The module may also be configured
 * to use one or more DMA channels, in which case it will map and configure the
 * relevant per-channel registers and expose device and API visibility for the
 * channels. Configuration of the module is via module-parameters, typically
 * provided on the command-line of 'insmod'/'modprobe' or via implicit
 * 'modprobe' settings in /etc/modules.conf.
 *
 * NB: THE MODULE PARAMETERS ARE LISTED IN REVERSE ORDER FOR THE EXPRESS
 * PURPOSE OF APPEASING 'modinfo' WHOSE DAMAGED PSYCHE CAUSES EVERYTHING TO BE
 * BUILT/LISTED IN REVERSE ORDER.
 *
 * The usage notes below elaborate what these module parameters are and how
 * they ought to be used;
 *
 *
 * Master mode
 * -----------
 *
 * master=[0|1]                 (default=1)
 *    This parameter indicates whether the module should map and configure the
 *    common-control registers. The remaining parameters are ignored if this is
 *    set to 0.
 *
 * freelist0=<size>             (default: [0-3]=4096, [4-7]=256)
 * freelist1=<size>
 * ...
 * freelist7=<size>
 *    These parameters control the block-size supported by each of the 8 global
 *    ("physical") freelists. <size> is in bytes, but must be a multiple of 32,
 *    minimum of 128, and no greater than 64K. Which of these freelists is
 *    visible to which DMA channel is configured by the following parameters;
 *
 * freelistA=<4-byte hexmask>   (default=0x00000000)
 * freelistB=<4-byte hexmask>
 *    The "A" register controls which physical freelist is visible as the
 *    virtual "A" freelist for each channel. Each byte represents a channel,
 *    with the least-significant byte representing channel 0 and the
 *    most-significant byte representing channel 3. The lowest-3 bits of each
 *    byte represent the physical freelist (0x00-0x07), the highest bit of each
 *    byte is the "enabled" bit, and the next-highest bit acting as a hint to
 *    the corresponding channel that it should act as the (physical) freelist's
 *    "master" - ie. that it should seed the freelist with buffers when it runs
 *    low, and should drain the freelist when tearing down. So valid "enabled"
 *    non-master virtual freelist values are 0x80-0x87, and valid "enabled"
 *    master virtual freelist values are 0xc0-0xc7. Otherwise the byte should
 *    be set to 0x00 to indicate that the virtual freelist is disabled.
 *
 * simple_reports=[1|0]		(default=1)
 *    This value controls the format of scan reports that are presented to
 *    the user.  If set to 1, an EOP report will be generated after any
 *    reaction reports.  If 0, no EOP reports will be generated
 *
 * sre_rule_num=<0-8192, multiple of 256>  (default=8192)
 *    Defines the configured number of rules in the database
 *
 * sre_session_ctx_size=<32-32768>	(default=32768)
 *    Defines the context size per session in bytes.  Valid values are:
 *     32,64,128,256,512,1024,2048,4096,8192,16384, 32768
 *
 * sre_session_ctx_num=<num contexts>	(default=1024)
 *    Determines the number of SRE Session Contexts
 *
 * dxe_sre_table_size=<num blocks>	(default=65536)
 *    Regex and Stateful Rule Table Size
 */

/***************************/
/* Global register offsets */
/***************************/

/* They're documented as byte offsets but we use u32 pointer additions */
#define ISRCC			(0x000 >> 2)
#define IERCC			(0x004 >> 2)
#define ISDRCC			(0x008 >> 2)
#define IIRCC			(0x00c >> 2)
#define IFRCC			(0x010 >> 2)
#define SCOS			(0x014 >> 2)
#define CSCR			(0x024 >> 2)
#define STNIB			(0x080 >> 2)
#define STNIS			(0x084 >> 2)
#define STNTH1			(0x088 >> 2)
#define STNTH2			(0x08c >> 2)
#define STNTHL			(0x090 >> 2)
#define STNTHS			(0x094 >> 2)
#define STNCH			(0x098 >> 2)
#define SWDB			(0x09c >> 2)
#define KVLTS			(0x0a0 >> 2)
#define KEC			(0x0a4 >> 2)
#define STNPM			(0x100 >> 2)
#define STNS1M			(0x104 >> 2)
#define DRCC			(0x108 >> 2)
#define STNPMR			(0x10c >> 2)
#define PDSRBA_HIGH		(0x120 >> 2)
#define PDSRBA_LOW		(0x124 >> 2)
#define DMCR			(0x128 >> 2)
#define DEC			(0x12c >> 2)
#define STNDSR			(0x180 >> 2)
#define STNESR			(0x184 >> 2)
#define STNS1R			(0x188 >> 2)
#define STNOB			(0x18c >> 2)
#define SCBARH			(0x1a8 >> 2)
#define SCBARL			(0x1ac >> 2)
#define SMCR			(0x1b0 >> 2)
#define SREC			(0x1b4 >> 2)
#define SRRV0			(0x1b8 >> 2)
#define SRRV1			(0x1bc >> 2)
#define SRRV2			(0x1c0 >> 2)
#define SRRV3			(0x1c4 >> 2)
#define SRRV4			(0x1c8 >> 2)
#define SRRV5			(0x1cc >> 2)
#define SRRV6			(0x1d0 >> 2)
#define SRRV7			(0x1d4 >> 2)
#define SRRFI			(0x1d8 >> 2)
#define SRRI			(0x1dc >> 2)
#define SRRR			(0x1e0 >> 2)
#define SRRWC			(0x1e4 >> 2)
#define SFRCC			(0x1e8 >> 2)
#define SEC1			(0x1ec >> 2)
#define SEC2			(0x1f0 >> 2)
#define SEC3			(0x1f4 >> 2)
#define STDBC			(0x200 >> 2)
#define STDBP			(0x204 >> 2)
#define STDWC			(0x208 >> 2)
#define FBL0SIZE		(0x2a0 >> 2)
#define FBL1SIZE		(0x2a4 >> 2)
#define FBL2SIZE		(0x2a8 >> 2)
#define FBL3SIZE		(0x2ac >> 2)
#define FBL4SIZE		(0x2b0 >> 2)
#define FBL5SIZE		(0x2b4 >> 2)
#define FBL6SIZE		(0x2b8 >> 2)
#define FBL7SIZE		(0x2bc >> 2)
#define FBLAAR			(0x2c0 >> 2)
#define FBLABR			(0x2c4 >> 2)
#define FBM_CR			(0x2e0 >> 2)
#define CC_SNOOP_DBG		(0x2f0 >> 2)
#define CC_ISR_COUNT		(0x2f4 >> 2)
#define MIA_BYC			(0xb80 >> 2)
#define MIA_BLC			(0xb84 >> 2)
#define MIA_LSHC0		(0xbb0 >> 2)
#define MIA_LSHC1		(0xbb4 >> 2)
#define MIA_LSHC2		(0xbb8 >> 2)
#define MIA_CWC0		(0xbc0 >> 2)
#define MIA_CWC1		(0xbc4 >> 2)
#define MIA_CWC2		(0xbc8 >> 2)
#define PME_IP_REV1		(0xbf8 >> 2)
#define PME_IP_REV2		(0xbfc >> 2)

/* Register map */
static struct pme_regmap *regs;

/* Any monitor sleepers wait on this waitqueue */
static DECLARE_WAIT_QUEUE_HEAD(monitor_queue);

/* Boolean used by ioctls */
static int is_master;

/* Memory allocations */
static dma_addr_t dxetable_addr;
static size_t dxetable_sz;
static dma_addr_t sretable_addr;
static size_t sretable_sz;

#define MAXDEVNAME 32

/* Everything, including the register layout, depends on there being 4
 * channels. Eg. we also assume each channel has two virtual freelists and this
 * corresponds to a maximum of 8 physical freelists. Make sure we can rely on
 * this assumption. */
#if PME_CHANNEL_MAX != 4
#error "ERROR, PME_CHANNEL_MAX must be 4"
#endif

/* Mapping from physical freelist to virtual channel mapping. */
static struct phys_freelist {
	/* item[ch*2+virt] is zero unless channel 'ch' has virtual freelist
	 * 'virt' mapped, in which case it is the mapping code (0xc* for
	 * master, otherwise 0x8* for user). */
	unsigned char mask_items[2 * PME_CHANNEL_MAX];
	/* number of non-zero items in mask_items */
	unsigned char count;
	/* boolean, set if mask_items contains a 0xc* */
	unsigned char mastered;
} phys_freelists[2 * PME_CHANNEL_MAX];

/* Channel state. Includes mappings from virtual to physical freelists. This is
 * also used for channel load, unload, kill, and reset handling - the
 * ENABLING/DISABLING states give us a way to represent channels that are "in
 * progress" without holding the lock. Attempts to use a channel while it's in
 * transition can then bail gracefully. */
static struct channel_state {
	enum {
		CH_DISABLED,
		CH_ENABLING,
		CH_ENABLED,
		CH_DISABLING
	} enabled;
	/* Reference count on an "ENABLED" channel. The unload ioctl will fail
	 * unless it can hit zero and move to DISABLING for cleanup. This
	 * protects races when kill/reset ioctls block, and gives meaning to
	 * the pme_channel_[get|put] APIs. */
	int refs;
	/* the two virtual freelist mappings */
	unsigned char virt[2];
	/* The character-device + fops */
	char devname[MAXDEVNAME];
	struct file_operations fops;
	struct miscdevice cdev;
	/* The channel itself - NULL unless loaded */
	struct pme_channel *channel;
} channel_state[PME_CHANNEL_MAX];

/* Used to lock accesses to channel_state[x]->enabled */
static DEFINE_SPINLOCK(ch_enable_lock);

/* Predeclare the channel fops */
static int channel_fops_open(struct inode *inode, struct file *f);
static int channel_fops_release(struct inode *inode, struct file *f);
static int channel_fops_ioctl(struct inode *inode, struct file *f,
			unsigned int cmd, unsigned long arg);
static loff_t channel_fops_llseek(struct file *f, loff_t offset, int whence);

static inline int __VALID_MASK_ITEM(unsigned char i)
{
	/* i is 0x00 for disabled, otherwise top bit (0x80) is set. The next
	 * lowest bit is "master" (can be set or not), the lowest three bits
	 * are the physical freelist, the rest (0x38) must be zero. */
	if (!i)
		return 1;
	if (!(i & 0x80))
		return 0;
	if (i & 0x38)
		return 0;
	return 1;
}

static int phys_freelist_update(u32 mask, u8 isB)
{
	unsigned int ch;
	struct channel_state *channel = channel_state;
	isB = isB ? 1 : 0;
	for (ch = 0; ch < PME_CHANNEL_MAX; ch++, channel++) {
		struct phys_freelist *phys;
		unsigned char item = (mask >> (ch * 8)) & 0xFF;
		if (!__VALID_MASK_ITEM(item)) {
			printk(KERN_ERR PMMOD "invalid freelist mask item "
				"0x%02x for channel %d freelist %c\n", item,
				ch, isB ? 'B' : 'A');
			return -EINVAL;
		}
		if (item) {
			phys = phys_freelists + (item & 7);
			phys->mask_items[2 * ch + isB] = item;
			phys->count++;
			phys->mastered += ((item & 0x40) ? 1 : 0);
			channel->virt[isB] = item;
		}
	}
	return 0;
}

static int phys_freelist_check(void)
{
	unsigned int i;
	struct phys_freelist *phys = phys_freelists;
	for (i = 0; i < (2 * PME_CHANNEL_MAX); i++, phys++) {
		if (phys->count) {
			if (!phys->mastered) {
				printk(KERN_ERR PMMOD "freelist %d has %d "
					"users but no master\n", i,
					phys->count);
				return -EINVAL;
			} else if (phys->mastered > 1) {
				printk(KERN_ERR PMMOD "freelist %d has more "
						"than 1 master\n", i);
				return -EINVAL;
			}
		}
	}
	return 0;
}

static int channel_vfreelist_check(unsigned int idx, u8 isB,
				struct pme_ctrl_load *p)
{
	struct channel_state *channel = channel_state + idx;
	unsigned char mask = channel->virt[isB];
	char c = isB ? 'B' : 'A';
	struct __pme_ctrl_load_fb *fb = &p->fb[isB];
	isB = isB ? 1 : 0;
	/* validate master parameters if the master bit is set */
	if (!(mask & 0x40)) {
		if (fb->low || fb->high || fb->max || fb->delta) {
			printk(KERN_ERR PMMOD "master parameters for "
				"non-master channel %d:%c\n", idx, c);
			return -EINVAL;
		}
		return 0;
	}
	if (!fb->low || !fb->delta) {
		printk(KERN_ERR PMMOD "zero low or delta parameter for master "
				"channel %d:%c\n", idx, c);
		return -EINVAL;
	}
	if (fb->starve > fb->low) {
		printk(KERN_ERR PMMOD "starve > low for channel %d:%c\n",
				idx, c);
		return -EINVAL;
	}
	if (fb->high && (fb->low > fb->high)) {
		printk(KERN_ERR PMMOD "low > high for channel %d:%c\n",
				idx, c);
		return -EINVAL;
	}
	if (fb->max && (fb->high > fb->max)) {
		printk(KERN_ERR PMMOD "high > max for channel %d:%c\n",
				idx, c);
		return -EINVAL;
	}
	if (fb->high && (fb->delta >= (fb->high - fb->low))) {
		printk(KERN_ERR PMMOD "delta >= high-low for channel %d:%c\n",
				idx, c);
		return -EINVAL;
	}
	return 0;
}

/**********/
/* ioctls */
/**********/

/* Read global stats */
static int ioctl_stats(void *user_addr)
{
	struct pme_ctrl_stats stats;
	if (copy_from_user(&stats, user_addr, sizeof(stats)))
		return -EFAULT;
	if (stats.flags & PME_CTRL_STNIB)
		stats.stnib = reg_get(regs, STNIB);
	if (stats.flags & PME_CTRL_STNIS)
		stats.stnis = reg_get(regs, STNIS);
	if (stats.flags & PME_CTRL_STNTH1)
		stats.stnth1 = reg_get(regs, STNTH1);
	if (stats.flags & PME_CTRL_STNTH2)
		stats.stnth2 = reg_get(regs, STNTH2);
	if (stats.flags & PME_CTRL_STNTHL)
		stats.stnthl = reg_get(regs, STNTHL);
	if (stats.flags & PME_CTRL_STNCH)
		stats.stnch = reg_get(regs, STNCH);
	if (stats.flags & PME_CTRL_STNPM)
		stats.stnpm = reg_get(regs, STNPM);
	if (stats.flags & PME_CTRL_STNS1M)
		stats.stns1m = reg_get(regs, STNS1M);
	if (stats.flags & PME_CTRL_STNPMR)
		stats.stnpmr = reg_get(regs, STNPMR);
	if (stats.flags & PME_CTRL_STNDSR)
		stats.stndsr = reg_get(regs, STNDSR);
	if (stats.flags & PME_CTRL_STNESR)
		stats.stnesr = reg_get(regs, STNESR);
	if (stats.flags & PME_CTRL_STNS1R)
		stats.stns1r = reg_get(regs, STNS1R);
	if (stats.flags & PME_CTRL_STNOB)
		stats.stnob = reg_get(regs, STNOB);
	if (stats.flags & PME_CTRL_STDBC)
		stats.stdbc = reg_get(regs, STDBC);
	if (stats.flags & PME_CTRL_STDBP)
		stats.stdbp = reg_get(regs, STDBP);
	if (stats.flags & PME_CTRL_STDWC)
		stats.stdwc = reg_get(regs, STDWC);
	if (stats.flags & PME_CTRL_MIA_BYC)
		stats.mia_byc = reg_get(regs, MIA_BYC);
	if (stats.flags & PME_CTRL_MIA_BLC)
		stats.mia_blc = reg_get(regs, MIA_BLC);
	if (stats.flags & PME_CTRL_MIA_LSHC0)
		stats.mia_lshc0 = reg_get(regs, MIA_LSHC0);
	if (stats.flags & PME_CTRL_MIA_LSHC1)
		stats.mia_lshc1 = reg_get(regs, MIA_LSHC1);
	if (stats.flags & PME_CTRL_MIA_LSHC2)
		stats.mia_lshc2 = reg_get(regs, MIA_LSHC2);
	if (stats.flags & PME_CTRL_MIA_CWC0)
		stats.mia_cwc0 = reg_get(regs, MIA_CWC0);
	if (stats.flags & PME_CTRL_MIA_CWC1)
		stats.mia_cwc1 = reg_get(regs, MIA_CWC1);
	if (stats.flags & PME_CTRL_MIA_CWC2)
		stats.mia_cwc2 = reg_get(regs, MIA_CWC2);
	if (stats.flags & PME_CTRL_STNTHS)
		stats.stnths = reg_get(regs, STNTHS);
	stats.overflow = reg_get(regs, SCOS) & stats.flags;
	/* Write-to-clear the overflow bits. Those stats that have been read
	 * have already been reset so it's necessary to do likewise with the
	 * overflow bits. NB, we have sleep interfaces available to allow an
	 * application to check the stats "often", however there's always a
	 * theoretical possibility for wrap-arounds to occur and thus give us
	 * "race" potentials between the stats and their overflow bits (there
	 * is no way to read them together atomically. We read/reset overflow
	 * bits after the stats to give us the least-worst combination. */
	reg_set(regs, SCOS, stats.overflow);
	if (copy_to_user(user_addr, &stats, sizeof(stats)))
		return -EFAULT;
	return 0;
}

/* Sleepers wait on this list */
static LIST_HEAD(sleeper_list);
/* Sleepers are put on this list when woken - ensures a high-priority source of
 * wakeups doesn't repeatedly rewake up the same items who can't get themselves
 * off the list due to starvation */
static LIST_HEAD(yawner_list);
static DEFINE_SPINLOCK(sleeper_lock);
/* sleep */
static int ioctl_sleep(void *user_addr)
{
	int res;
	u32 ms;
	struct stat_sleeper sleeper;
	if (copy_from_user(&ms, user_addr, sizeof(ms)))
		return -EFAULT;
	INIT_LIST_HEAD(&sleeper.list);
	init_waitqueue_head(&sleeper.queue);
	sleeper.woken = 0;
	spin_lock_bh(&sleeper_lock);
	list_add_tail(&sleeper.list, &sleeper_list);
	spin_unlock_bh(&sleeper_lock);
	if (ms) {
		res = wait_event_interruptible_timeout(sleeper.queue,
				sleeper.woken != 0, msecs_to_jiffies(ms));
		if (res > 0)
			res = 0;
		else if (!res)
			res = 1;
	} else
		res = wait_event_interruptible(sleeper.queue,
				sleeper.woken != 0);
	if (res < 0)
		res = -EINTR;
	spin_lock_bh(&sleeper_lock);
	list_del(&sleeper.list);
	spin_unlock_bh(&sleeper_lock);
	return res;
}

/* revision */
static int ioctl_rev(void *user_addr)
{
	struct pme_ctrl_rev rev;
	u32 rev1 = reg_get(regs, PME_IP_REV1);
	u32 rev2 = reg_get(regs, PME_IP_REV2);
	rev.ip_id = rev1 >> 16;
	rev.ip_mj = (rev1 & 0x0000FF00) >> 8;
	rev.ip_mn = rev1 & 0x000000FF;
	rev.ip_int = (rev2 & 0x00FF0000) >> 16;
	rev.ip_cfg = rev2 & 0x000000FF;
	if (copy_to_user(user_addr, &rev, sizeof(rev)))
		return -EFAULT;
	return 0;
}

static inline void __fb_l_set(struct __pme_fb_l *p, u8 val)
{
	p->phys_idx = val & 0x7;
	p->is_enabled = val & 0x80;
	p->is_master = val & 0x40;
}

static int ioctl_fbm(void *user_addr)
{
	int loop;
	u32 a, b;
	struct pme_ctrl_fb fb;
	fb.blocksize[0] = reg_get(regs, FBL0SIZE);
	fb.blocksize[1] = reg_get(regs, FBL1SIZE);
	fb.blocksize[2] = reg_get(regs, FBL2SIZE);
	fb.blocksize[3] = reg_get(regs, FBL3SIZE);
	fb.blocksize[4] = reg_get(regs, FBL4SIZE);
	fb.blocksize[5] = reg_get(regs, FBL5SIZE);
	fb.blocksize[6] = reg_get(regs, FBL6SIZE);
	fb.blocksize[7] = reg_get(regs, FBL7SIZE);
	a = reg_get(regs, FBLAAR);
	b = reg_get(regs, FBLABR);
	for (loop = (PME_CHANNEL_MAX - 1); loop >= 0; loop--) {
		__fb_l_set(&fb.channels[loop].virt[0],
				(a >> (8 * loop))&0xff);
		__fb_l_set(&fb.channels[loop].virt[1],
				(b >> (8 * loop))&0xff);
	}
	if (copy_to_user(user_addr, &fb, sizeof(fb)))
		return -EFAULT;
	return 0;
}

static int ioctl_monitor(void *user_addr)
{
	int res = 0;
	struct pme_monitor_poll p;
	if (copy_from_user(&p, user_addr, sizeof(p)))
		return -EFAULT;
	/* Disable bits as requested */
	if (p.disable_mask)
		reg_set(regs, ISDRCC, reg_get(regs, ISDRCC) | p.disable_mask);
	/* Clear status bits corresponding to those requested as well as those
	 * disabled */
	if (p.clear_mask | p.disable_mask)
		reg_set(regs, ISRCC, p.clear_mask | p.disable_mask);
	/* Re-enable bits we may have cleared in the enable register */
	if (p.clear_mask)
		reg_set(regs, IERCC, reg_get(regs, IERCC) | p.clear_mask);
	if (p.block) {
		if (p.timeout_ms) {
			res = wait_event_interruptible_timeout(monitor_queue,
				reg_get(regs, ISRCC) & ~(p.ignore_mask),
				msecs_to_jiffies(p.timeout_ms));
			if (res > 0)
				res = 0;
			else if (!res)
				res = 1;
		} else
			res = wait_event_interruptible(monitor_queue,
				reg_get(regs, ISRCC) & ~(p.ignore_mask));
		if (res < 0)
			res = -EINTR;
	}
	p.status = reg_get(regs, ISRCC);
	if (copy_to_user(user_addr, &p, sizeof(p)))
		return -EFAULT;
	return res;
}

/* Stateful rule reset */
static atomic_t sre_reset_lock = ATOMIC_INIT(1);
static int ioctl_sre_reset(void *user_addr)
{
	struct pme_sre_reset reset_vals;
	unsigned int i = 0, loop = 100000;
	unsigned long reg_ptr = SRRV0;

	if (copy_from_user(&reset_vals, user_addr, sizeof(reset_vals)))
		return -EFAULT;
	/* Validate ranges */
	if ((reset_vals.rule_index > PME_DMA_SRE_INDEX_MAX) ||
			(reset_vals.rule_increment > PME_DMA_SRE_INC_MAX) ||
			(reset_vals.rule_repetitions > PME_DMA_SRE_REP_MAX) ||
			(reset_vals.rule_reset_interval >
				PME_DMA_SRE_INTERVAL_MAX))
		return -ERANGE;
	/* Check and make sure only one caller is present */
	if (!atomic_dec_and_test(&sre_reset_lock)) {
		/* Someone else is already in this call */
		atomic_inc(&sre_reset_lock);
		return -EBUSY;
	};
	/* All validated.  Run the command */
	for (; i < PME_SRE_RULE_VECTOR_SIZE; i++)
		reg_set(regs, reg_ptr++, reset_vals.rule_vector[i]);
	reg_set(regs, SRRFI, reset_vals.rule_index);
	reg_set(regs, SRRI, reset_vals.rule_increment);
	reg_set(regs, SRRWC, (0xFFF & reset_vals.rule_reset_interval) << 1 |
			(reset_vals.rule_reset_priority ? 1 : 0));
	/* Need to set SRRR last */
	reg_set(regs, SRRR, reset_vals.rule_repetitions);
	do {
		mdelay(PME_DMA_SRE_POLL_MS);
	} while (reg_get(regs, SRRR) || !(loop--));
	atomic_inc(&sre_reset_lock);
	return 0;
}

static int ioctl_load(void *user_addr)
{
	struct pme_ctrl_load params;
	if (copy_from_user(&params, user_addr, sizeof(params)))
		return -EFAULT;
	return pme_channel_load(&params);
}

static int ioctl_channel(void *user_addr)
{
	int err;
	struct pme_ctrl_channel params;
	struct channel_state *state;
	if (copy_from_user(&params, user_addr, sizeof(params)))
		return -EFAULT;
	if (params.channel >= PME_CHANNEL_MAX)
		return -ERANGE;
	if (params.cmd == CHANNEL_UNLOAD)
		return pme_channel_unload(params.channel, params.block);
	state = channel_state + params.channel;
	spin_lock_bh(&ch_enable_lock);
	if (state->enabled != CH_ENABLED) {
		printk(KERN_ERR PMMOD "channel %d is not enabled\n",
				params.channel);
		spin_unlock_bh(&ch_enable_lock);
		return -ENODEV;
	}
	state->refs++;
	spin_unlock_bh(&ch_enable_lock);
	switch (params.cmd) {
	case CHANNEL_KILL:
		err = pme_channel_kill(state->channel, params.block);
		break;
	case CHANNEL_RESET:
		err = pme_channel_reset(state->channel, params.block);
		break;
	default:
		err = -EINVAL;
	}
	spin_lock_bh(&ch_enable_lock);
	state->refs--;
	spin_unlock_bh(&ch_enable_lock);
	return err;
}

static int ioctl_get_opts(void *user_addr)
{
	struct pme_ctrl_opts opts;
	u32 val;

	if (copy_from_user(&opts, user_addr, sizeof(opts)))
		return -EFAULT;
	if (opts.flags & PME_CTRL_OPT_SWDB)
		opts.swdb = reg_get(regs, SWDB);
	if (opts.flags & PME_CTRL_OPT_EOSRP) {
		opts.eosrp = reg_get(regs, SREC);
		opts.eosrp &= PME_DMA_EOSRP_MASK; /* 18 lower bits */
	}
	if (opts.flags & PME_CTRL_OPT_DRCC)
		opts.drcc = reg_get(regs, DRCC);
	if (opts.flags & PME_CTRL_OPT_KVLTS) {
		val = reg_get(regs, KVLTS);
		opts.kvlts = val + 1;
	}
	if (opts.flags & PME_CTRL_OPT_MCL) {
		opts.mcl = reg_get(regs, KEC);
		opts.mcl &= PME_DMA_MCL_MASK; /* 15 lower bits */
	}
	if (copy_to_user(user_addr, &opts, sizeof(opts)))
		return -EFAULT;
	return 0;
};

static int ioctl_set_opts(void *user_addr)
{
	struct pme_ctrl_opts opts;
	u32 val;

	if (copy_from_user(&opts, user_addr, sizeof(opts)))
		return -EFAULT;
	/* Validation */
	if (opts.flags & PME_CTRL_OPT_KVLTS)
		if (opts.kvlts < 1 || opts.kvlts > 16)
			return -EINVAL;
	if (opts.flags & PME_CTRL_OPT_SWDB)
		reg_set(regs, SWDB, opts.swdb);
	if (opts.flags & PME_CTRL_OPT_EOSRP) {
		val = reg_get(regs, SREC);
		/* 18 lower bits */
		val = (val & ~PME_DMA_EOSRP_MASK) |
			(opts.eosrp & PME_DMA_EOSRP_MASK);
		reg_set(regs, SREC, val);
	}
	if (opts.flags & PME_CTRL_OPT_DRCC)
		reg_set(regs, DRCC, opts.drcc);
	if (opts.flags & PME_CTRL_OPT_KVLTS) {
		val = opts.kvlts - 1;
		reg_set(regs, KVLTS, val);
	}
	if (opts.flags & PME_CTRL_OPT_MCL)  {
		val = reg_get(regs, KEC);
		/* 15 lower bits */
		val = (val & ~PME_DMA_MCL_MASK) |
			(opts.mcl & PME_DMA_MCL_MASK);
		reg_set(regs, KEC, val);
	}
	return 0;
};

/*****************/
/* Global device */
/*****************/

/* This isr is invoked if an error occurs at the global level of the device
 * There isn't much we can do other than log the occurance and disable the
 * condition that caused the error */
static irqreturn_t global_isr(int irq, void *dev_id)
{
	u32 status = reg_get(regs, ISRCC);
	u32 enabled = reg_get(regs, IERCC);
	if (!status)
		/* The IRQ wasn't for us.  Let the kernel try the next handler
		 * registered on this irq */
		return IRQ_NONE;
	printk(KERN_ERR PMMOD PME_CTRL_PATH
		": Global Error ISR %d ISRCC is 0x%x\n", irq, status);
	/* Disable these error bits from asserting the interrupt again */
	reg_set(regs, IERCC, enabled & ~status);
	/* We need to be sure the posted write to IERCC has taken affect before
	 * the ISR exits (otherwise it will refire unnecessarily). To this end,
	 * we read the register back and compare, ensuring the write has to
	 * complete first. NB, we don't need smp_mb() here or any such thing
	 * because the CPU can't reorder the initiation of the read/write pair
	 * due to the interdependency of the operations. */
	if (reg_get(regs, IERCC) != (enabled & ~status))
		printk(KERN_ERR PMMOD PME_CTRL_PATH ": IERCC inconsistency\n");
	wake_up_all(&monitor_queue);
	return IRQ_HANDLED;
}

static int global_fops_ioctl(struct inode *inode, struct file *f,
			unsigned int cmd, unsigned long arg)
{
	if (!is_master) {
		/* If we're not master, only allow through certain ioctls */
		switch (cmd) {
		case PME_CTRL_IOCTL_LOAD:
		case PME_CTRL_IOCTL_CHANNEL:
			break;
		default:
			return -ENOSYS;
		}
	}
	switch (cmd) {
	case PME_CTRL_IOCTL_STATS:
		return ioctl_stats((void *)arg);
	case PME_CTRL_IOCTL_SLEEP:
		return ioctl_sleep((void *)arg);
	case PME_CTRL_IOCTL_REV:
		return ioctl_rev((void *)arg);
	case PME_CTRL_IOCTL_FBM:
		return ioctl_fbm((void *)arg);
	case PME_CTRL_IOCTL_SRE_RESET:
		return ioctl_sre_reset((void *)arg);
	case PME_CTRL_IOCTL_SET_OPTS:
		return ioctl_set_opts((void *)arg);
	case PME_CTRL_IOCTL_GET_OPTS:
		return ioctl_get_opts((void *)arg);
	case PME_CTRL_IOCTL_LOAD:
		return ioctl_load((void *)arg);
	case PME_CTRL_IOCTL_CHANNEL:
		return ioctl_channel((void *)arg);
	case PME_CTRL_IOCTL_MONITOR:
		return ioctl_monitor((void *)arg);
	}
	return -ENOSYS;
}

static struct file_operations global_fops = {
	.owner = THIS_MODULE,
	.ioctl = global_fops_ioctl,
	.llseek = channel_fops_llseek
};

static struct miscdevice global_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = PME_CTRL_NODE,
	.fops = &global_fops
};

/*****************/
/* Exported APIs */
/*****************/

struct pme_channel *pme_device_get_channel(struct file *f)
{
	struct pme_channel *ret = NULL;
	struct channel_state *state = container_of(f->f_op,
					struct channel_state, fops);
	size_t diff = (void *)state - (void *)channel_state;
	/* 'state' is only valid if 'f' was in fact a channel file! Check for a
	 * pointer match - if no match, 'f' was some other kind of file and
	 * 'state' is a garbage value. */
	if (diff % sizeof(struct channel_state))
		return NULL;
	if ((diff / sizeof(struct channel_state)) >= PME_CHANNEL_MAX)
		return NULL;
	spin_lock_bh(&ch_enable_lock);
	if (state->enabled == CH_ENABLED) {
		ret = state->channel;
		state->refs++;
	}
	spin_unlock_bh(&ch_enable_lock);
	return ret;
}
EXPORT_SYMBOL(pme_device_get_channel);

int pme_channel_get(struct pme_channel **channel, unsigned int idx)
{
	int ret = 0;
	struct channel_state *state = channel_state + idx;
	if (idx >= PME_CHANNEL_MAX)
		return -ERANGE;
	spin_lock_bh(&ch_enable_lock);
	if (state->enabled == CH_ENABLED) {
		state->refs++;
		*channel = state->channel;
	} else
		ret = -ENODEV;
	spin_unlock_bh(&ch_enable_lock);
	return 0;
}
EXPORT_SYMBOL(pme_channel_get);

/* Helper for pme_channel_[put|up] */
static inline struct channel_state *__find_channel_state(struct pme_channel *c)
{
	unsigned int loop = 0;
	struct channel_state *state = channel_state;
	for (; loop < PME_CHANNEL_MAX; loop++, state++)
		if (state->channel == c)
			return state;
	return NULL;
}

void pme_channel_put(struct pme_channel *channel)
{
	struct channel_state *state;
	spin_lock_bh(&ch_enable_lock);
	state = __find_channel_state(channel);
	state->refs--;
	spin_unlock_bh(&ch_enable_lock);
}
EXPORT_SYMBOL(pme_channel_put);

void pme_channel_up(struct pme_channel *channel)
{
	struct channel_state *state;
	spin_lock_bh(&ch_enable_lock);
	state = __find_channel_state(channel);
	state->refs++;
	spin_unlock_bh(&ch_enable_lock);
}
EXPORT_SYMBOL(pme_channel_up);

int pme_channel_load(struct pme_ctrl_load *params)
{
	int set = 0;
	int err = -EINVAL;
	struct channel_state *state;

	if (params->channel >= PME_CHANNEL_MAX)
		return -ERANGE;
	state = channel_state + params->channel;
	spin_lock_bh(&ch_enable_lock);
	if (state->enabled != CH_DISABLED) {
		printk(KERN_ERR PMMOD "channel %d is not disabled\n",
				params->channel);
		goto end;
	}
	if (!ISEXP2(params->cmd_fifo) || (params->cmd_fifo < 2)) {
		printk(KERN_ERR PMMOD "invalid 'cmd_fifo' %d\n",
				params->cmd_fifo);
		goto end;
	}
	if (!ISEXP2(params->not_fifo) || (params->not_fifo < 2)) {
		printk(KERN_ERR PMMOD "invalid 'not_fifo' %d\n",
				params->not_fifo);
		goto end;
	}
	if (!ISEXP2(params->fb_fifo) || (params->fb_fifo < 2)) {
		printk(KERN_ERR PMMOD "invalid 'fb_fifo' %d\n",
				params->fb_fifo);
		goto end;
	}
	if (params->context_table && !ISEXP2(params->context_table)) {
		printk(KERN_ERR PMMOD "invalid 'context_table' %d\n",
				params->context_table);
		goto end;
	}
	if (params->residue_table && !ISEXP2(params->residue_table)) {
		printk(KERN_ERR PMMOD "invalid 'residue_table' %d\n",
				params->residue_table);
		goto end;
	}
	if ((params->residue_size & 31) || (params->residue_size < 32) ||
			(params->residue_size > 128)) {
		printk(KERN_ERR PMMOD "invalid 'residue_size' %d\n",
				params->residue_size);
		goto end;
	}
	err = channel_vfreelist_check(params->channel, 0, params);
	if (err)
		goto end;
	err = channel_vfreelist_check(params->channel, 1, params);
	if (err)
		goto end;
	snprintf(state->devname, MAXDEVNAME, PME_CHANNEL_NODE,
				params->channel);
	state->fops.owner = THIS_MODULE;
	state->fops.open = channel_fops_open;
	state->fops.release = channel_fops_release;
	state->fops.ioctl = channel_fops_ioctl;
	state->fops.llseek = channel_fops_llseek;
	state->cdev.minor = MISC_DYNAMIC_MINOR;
	state->cdev.name = state->devname;
	state->cdev.fops = &state->fops;
	if (!try_module_get(THIS_MODULE)) {
		err = -ENODEV;
		goto end;
	}
	state->enabled = CH_ENABLING;
	set = 1;
	spin_unlock_bh(&ch_enable_lock);
	err = pme_channel_init(&state->channel, params);
	if (err)
		goto end_lock;
	err = pme_channel_reset(state->channel, 0);
	if (err)
		goto end_lock_kill;
	err = misc_register(&state->cdev);
	if (err)
		printk(KERN_ERR PMMOD "creating device %s failed\n",
				state->devname);
	else
		goto end_lock;
end_lock_kill:
	if (pme_channel_kill(state->channel, 1) ||
				pme_channel_finish(state->channel))
		printk(KERN_ERR PMMOD "Error-recovery failed\n");
end_lock:
	spin_lock_bh(&ch_enable_lock);
end:
	if (err && set) {
		state->enabled = CH_DISABLED;
		module_put(THIS_MODULE);
	} else if (!err) {
		state->enabled = CH_ENABLED;
		state->refs = 1;
	}
	spin_unlock_bh(&ch_enable_lock);
	return err;
}
EXPORT_SYMBOL(pme_channel_load);

int pme_channel_unload(u8 channel, int block)
{
	int err;
	struct channel_state *state;

	state = channel_state + channel;
	spin_lock_bh(&ch_enable_lock);
	if (state->enabled != CH_ENABLED) {
		printk(KERN_ERR PMMOD "channel is not enabled\n");
		spin_unlock_bh(&ch_enable_lock);
		return -ENODEV;
	}
	if (state->refs != 1) {
		spin_unlock_bh(&ch_enable_lock);
		return -EBUSY;
	}
	state->enabled = CH_DISABLING;
	spin_unlock_bh(&ch_enable_lock);
	err = pme_channel_kill(state->channel, block);
	if (!err) {
		err = misc_deregister(&state->cdev);
		if (err)
			printk(KERN_ERR PMMOD "Failed to deregister device %s, "
				"code %d\n", state->cdev.name, err);
		err = pme_channel_finish(state->channel);
		if (err)
			/* Oops, reregister the misc device */
			misc_register(&state->cdev);
	}
	spin_lock_bh(&ch_enable_lock);
	if (err)
		state->enabled = CH_ENABLED;
	else {
		state->enabled = CH_DISABLED;
		state->refs = 0;
		state->channel = NULL;
		module_put(THIS_MODULE);
	}
	spin_unlock_bh(&ch_enable_lock);
	return err;
}
EXPORT_SYMBOL(pme_channel_unload);

u32 pme_ctrl_reg_get(unsigned int offset)
{
	BUG_ON(offset >= (4096>>2));
	return reg_get(regs, offset);
}
EXPORT_SYMBOL(pme_ctrl_reg_get);

void pme_ctrl_reg_set(unsigned int offset, u32 value)
{
	BUG_ON(offset >= (4096>>2));
	reg_set(regs, offset, value);
}
EXPORT_SYMBOL(pme_ctrl_reg_set);

/******************/
/* Channel device */
/******************/

static int channel_fops_open(struct inode *inode, struct file *f)
{
	/* Reuse what we already used in the general case. In this
	 * case, failure should never happen, so we'll check. */
	struct pme_channel *channel = pme_device_get_channel(f);
	f->private_data = channel;
	return 0;
}

static int channel_fops_release(struct inode *inode, struct file *f)
{
	struct pme_channel *channel = f->private_data;
	pme_channel_put(channel);
	return 0;
}

static int channel_fops_ioctl(struct inode *inode, struct file *f,
			unsigned int cmd, unsigned long arg)
{
	struct pme_channel *channel = f->private_data;
	return pme_channel_ioctl(channel, cmd, arg);
}

static loff_t channel_fops_llseek(struct file *f, loff_t offset, int whence)
{
	return -ESPIPE;
}

/*****************/
/* Kernel module */
/*****************/

/* Macros */
#define VALID_FREELIST_SIZE(sz)	\
	({ \
		/* Must be a multiple of 32, minimum 128, maximum 64K */ \
		unsigned int __sz = (sz); \
		(!(__sz & 31) && (__sz <= (64*1024)) && (sz >= 128)); \
	})

/* Defaults */
#define DEFAULT_MASTER			1
#define DEFAULT_SCHEDULING		0x00000000
#define DEFAULT_ENABLED			0
#define DEFAULT_DXE_TABLE_SIZE		65536
#define DEFAULT_SRE_TABLE_SIZE 		1024
#define DEFAULT_SIMPLE_REPORTS		1
#define DEFAULT_SRE_CONTEXT_SIZE	32768
#define DEFAULT_SRE_NUMBER_OF_RULES	8192
#define DEFAULT_PHYSMEM			0
#define DEFAULT_ALT_INC_MODE		0
/* Boot parameters */
DECLARE_GLOBAL(alt_inc_mode, unsigned long, ulong, DEFAULT_ALT_INC_MODE,
		"Set to 1 to enable Alternate Inclusive Mode")
DECLARE_GLOBAL(dxe_sre_table_size, unsigned int, uint, DEFAULT_DXE_TABLE_SIZE,
		"Regex and Stateful Rule table size")
DECLARE_GLOBAL_EXPORTED(sre_session_ctx_num, unsigned int, uint,
		DEFAULT_SRE_TABLE_SIZE,	"Number of SRE session contexts")
DECLARE_GLOBAL(sre_session_ctx_size, unsigned int, uint,
		DEFAULT_SRE_CONTEXT_SIZE,
		"SRE Context Size per session in bytes")
DECLARE_GLOBAL(sre_rule_num, unsigned int, uint, DEFAULT_SRE_NUMBER_OF_RULES,
		"Configured Number of Stateful Rules in Database")
DECLARE_GLOBAL(simple_reports, int, int, DEFAULT_SIMPLE_REPORTS,
		"Returns non-empty no-match reports")
DECLARE_GLOBAL(freelistB, unsigned int, uint, CONFIG_FSL_PME_FREELISTB,
		"Mask of 'B' virtual freelists for the channels")
DECLARE_GLOBAL(freelistA, unsigned int, uint, CONFIG_FSL_PME_FREELISTA,
		"Mask of 'A' virtual freelists for the channels")
DECLARE_GLOBAL(freelist7, unsigned int, uint, 1 << CONFIG_FSL_PME_FREELIST7,
		"Buffer-size for freelist 7")
DECLARE_GLOBAL(freelist6, unsigned int, uint, 1 << CONFIG_FSL_PME_FREELIST6,
		"Buffer-size for freelist 6")
DECLARE_GLOBAL(freelist5, unsigned int, uint, 1 << CONFIG_FSL_PME_FREELIST5,
		"Buffer-size for freelist 5")
DECLARE_GLOBAL(freelist4, unsigned int, uint, 1 << CONFIG_FSL_PME_FREELIST4,
		"Buffer-size for freelist 4")
DECLARE_GLOBAL(freelist3, unsigned int, uint, 1 << CONFIG_FSL_PME_FREELIST3,
		"Buffer-size for freelist 3")
DECLARE_GLOBAL(freelist2, unsigned int, uint, 1 << CONFIG_FSL_PME_FREELIST2,
		"Buffer-size for freelist 2")
DECLARE_GLOBAL(freelist1, unsigned int, uint, 1 << CONFIG_FSL_PME_FREELIST1,
		"Buffer-size for freelist 1")
DECLARE_GLOBAL(freelist0, unsigned int, uint, 1 << CONFIG_FSL_PME_FREELIST0,
		"Buffer-size for freelist 0")
DECLARE_GLOBAL(master, int, int, DEFAULT_MASTER, "Master mode")

static int __init pme_base_init(void)
{
	int err, ret;
	unsigned int tmp, srecontextsize_code, srenumrules_code;

	printk(KERN_INFO PMMOD "loading ...\n");
	err = pme_hal_init();
	if (err) {
		printk(KERN_ERR PMMOD "hal initialisation\n");
		return err;
	}
	err = pme_regmap_init();
	if (err) {
		printk(KERN_ERR PMMOD "register initialisation\n");
		goto master_pci;
	}
	err = update_command_setup();
	if (err) {
		printk(KERN_ERR PMMOD "command update cache initialisation\n");
		goto master_reg;
	}
	err = misc_register(&global_dev);
	if (err) {
		printk(KERN_ERR PMMOD "device %s failed\n", global_dev.name);
		goto master_update;
	}
	printk(KERN_INFO PMMOD "device %s registered\n", global_dev.name);
	/* Make sure we are ready to allocate memory for hardware resources */
	/* Fixup the DMA ops */
	pme_cds_dev_set(global_dev.this_device);
	global_dev.this_device->archdata.dma_ops = &dma_direct_ops;

	pme_mem_init();
	if (master) {
		if (!VALID_FREELIST_SIZE(freelist0) ||
				!VALID_FREELIST_SIZE(freelist1) ||
				!VALID_FREELIST_SIZE(freelist2) ||
				!VALID_FREELIST_SIZE(freelist3) ||
				!VALID_FREELIST_SIZE(freelist4) ||
				!VALID_FREELIST_SIZE(freelist5) ||
				!VALID_FREELIST_SIZE(freelist6) ||
				!VALID_FREELIST_SIZE(freelist7)) {
			printk(KERN_ERR PMMOD "invalid freelist size(s)\n");
			err = -EINVAL;
			goto master_dereg;
		}
		err = phys_freelist_update(freelistA, 0);
		if (!err)
			err = phys_freelist_update(freelistB, 1);
		if (!err)
			err = phys_freelist_check();
		if (err)
			/* it prints its own error message */
			goto master_dereg;
		/* NB: these constants are dictated by the block-guide and not
		 * configurable, hence there is no burning necessity to
		 * symbolise them in base_private.h. */
		if (!sre_session_ctx_size || !ISEXP2(sre_session_ctx_size) ||
				(sre_session_ctx_size < 32) ||
				(sre_session_ctx_size > (32*1024))) {
			printk(KERN_ERR PMMOD "invalid sre_session_ctx_size\n");
			err = -EINVAL;
			goto master_dereg;
		}
		if ((sre_rule_num & 0xFF) || (sre_rule_num < 0) ||
				(sre_rule_num > (8*1024))) {
			printk(KERN_ERR PMMOD "invalid sre_rule_num\n");
			err = -EINVAL;
			goto master_dereg;
		}
		if (dxe_sre_table_size > (256*1024)) {
			printk(KERN_ERR PMMOD "invalid dxe_sre_table_size\n");
			err = -EINVAL;
			goto master_dereg;
		}
		if (!sre_session_ctx_num ||
			(sre_session_ctx_num >=
				((1 << 31) / sre_session_ctx_size))) {
			printk(KERN_ERR PMMOD "invalid sre_session_ctx_num\n");
			err = -EINVAL;
			goto master_dereg;
		}

		/***********************************************************/
		/* Initialise the common-control stuff before the channels */
		/* Context Size per Session
		 *   5->32, 6->64, ... , 14->16384, 15->32768
		 */
		tmp = sre_session_ctx_size >> 6;
		srecontextsize_code = 1;
		while (tmp) {
			srecontextsize_code++;
			tmp >>= 1;
		}
		/* Number of rules in database.
		 *   multiples of 256 (0-32 is 0-8192) */
		srenumrules_code = sre_rule_num >> 8;
		regs = pme_regmap_map(0);
		if (!regs) {
			printk(KERN_ERR PMMOD "CC register map failed\n");
			err = -ENODEV;
			goto master_dereg;
		}
		/* Set up channel scheduling */
		reg_set(regs, CSCR, DEFAULT_SCHEDULING);
		/* Set up cache-awareness */
		reg_set(regs, FBM_CR,
			(PME_DMA_FBM_MTP << 8) |
			(PME_DMA_FBM_SNOOP << 4) |
			(PME_DMA_FBM_RDUNSAFE << 0));
		/* Set virtual freelist mappings */
		reg_set(regs, FBLAAR, freelistA);
		reg_set(regs, FBLABR, freelistB);
		/* Set freelist buffer sizes */
		reg_set(regs, FBL0SIZE, freelist0);
		reg_set(regs, FBL1SIZE, freelist1);
		reg_set(regs, FBL2SIZE, freelist2);
		reg_set(regs, FBL3SIZE, freelist3);
		reg_set(regs, FBL4SIZE, freelist4);
		reg_set(regs, FBL5SIZE, freelist5);
		reg_set(regs, FBL6SIZE, freelist6);
		reg_set(regs, FBL7SIZE, freelist7);
		/* DRCC - DXE Pattern Range Counter Config. */
		reg_set(regs, DRCC, PME_DMA_DXE_DRCC);
		/* Alloc dxe memory if requested */
		dxetable_sz = 128 * dxe_sre_table_size;
		if (dxetable_sz) {
			dxetable_addr = pme_mem_phys_alloc(dxetable_sz);
			if (!dxetable_addr) {
				printk(KERN_ERR PMMOD
					"DXE allocation failed\n");
				err = -ENOMEM;
				goto master_unmap;
			}
		}
		reg_set64(regs, PDSRBA_HIGH, (u64)dxetable_addr);
		/* Setup DXE memory control */
		reg_set(regs, DMCR,
			(PME_DMA_DXE_RDUNSAFE << 4) |
			(PME_DMA_DXE_MTP << 2) |
			(PME_DMA_DXE_SNOOP << 0));
		/* Allocate zeroed space for SRE Table.*/
		sretable_sz = sre_session_ctx_size * sre_session_ctx_num;
		if (sretable_sz) {
			sretable_addr = pme_mem_phys_alloc(sretable_sz);
			if (!sretable_addr) {
				printk(KERN_ERR PMMOD "SRE allocation failed, "
					"size %d\n", (int)sretable_sz);
				err = -ENOMEM;
				goto master_dxe;
			}
		}
		reg_set64(regs, SCBARH, (u64)sretable_addr);
		/* SMCR - SRE Memory Control Register */
		reg_set(regs, SMCR,
			(PME_DMA_SRE_CODE_SNOOP << 0) |
			(PME_DMA_SRE_CODE_MTP << 2) |
			(PME_DMA_SRE_CODE_RDUNSAFE << 4) |
			(PME_DMA_SRE_M_RD_SNOOP << 5) |
			(PME_DMA_SRE_M_RD_MTP << 7) |
			(PME_DMA_SRE_M_WR_SNOOP << 9) |
			(PME_DMA_SRE_M_WR_MTP << 11) |
			(PME_DMA_SRE_M_RDUNSAFE << 13));
		/* SREC - SRE Config */
		reg_set(regs, SREC,
			(PME_DMA_SRE_EOS_PTR << 0) |
			/* Simple Report Enabled */
			((simple_reports ? 1 : 0) << 18) |
			/* Context Size per Session */
			(srecontextsize_code << 19) |
			/* Number of rules in database */
			(srenumrules_code << 23) |
			/* Alternate Inclusive Mode */
			((alt_inc_mode ? 1 : 0) << 29));
		/* KEC - KES Error config */
		reg_set(regs, KEC, PME_DMA_KES_ERRORCONFIG);
		/* DEC - DXE Error config */
		reg_set(regs, DEC, PME_DMA_DXE_ERRORCONFIG);
		/* SEC - SRE Error config 1 through 3 */
		reg_set(regs, SEC1, PME_DMA_SRE_ERRORCONFIG1);
		/* SEC2 and SEC3 are used to validate table indexing */
		reg_set(regs, SEC2, dxe_sre_table_size);
		reg_set(regs, SEC3, sretable_sz/32);
		/* SFRCC : Set to 333 To match bus speed. */
		reg_set(regs, SFRCC, PME_DMA_SRE_FRCCONFIG);
		/* Inhibit interrupts */
		reg_set(regs, IIRCC, 1);
		/* Write-to-clear any existing interrupt sources */
		reg_set(regs, ISRCC, ~(u32)0);
		/* Enable all interrupt sources */
		reg_set(regs, IERCC, ~(u32)0);
		/* Disable no sources */
		reg_set(regs, ISDRCC, 0);
		/* Initialise and bind to the Common Control Interrupt */
		err = REQUEST_IRQ(pme_hal_irq_ctrl, global_isr, 0,
					PME_CTRL_NODE, &global_dev);
		if (err)
			goto master_sre;
		/* Uninhibit interrupts */
		reg_set(regs, IIRCC, 0);
		is_master = 1;
	}
	err = pme_user_init();
	if (err) {
		printk(KERN_ERR PMMOD "user-interface initialisation failed\n");
		goto master_irq;
	}
	printk(KERN_INFO PMMOD "loaded\n");
	return 0;
master_irq:
	if (is_master)
		FREE_IRQ(pme_hal_irq_ctrl, &global_dev);
master_sre:
	if (sretable_addr)
		pme_mem_phys_free(sretable_addr);
master_dxe:
	if (dxetable_addr)
		pme_mem_phys_free(dxetable_addr);
master_unmap:
	pme_regmap_unmap(regs);
master_dereg:
	ret = misc_deregister(&global_dev);
	if (ret)
		printk(KERN_ERR PMMOD "Failed to deregister device %s, "
			"code %d\n", global_dev.name, ret);
master_update:
	update_command_teardown();
master_reg:
	pme_regmap_finish();
master_pci:
	pme_hal_finish();
	return err;
}

static void __exit pme_base_finish(void)
{
	int ret;
	printk(KERN_INFO PMMOD "unloading ...\n");
	pme_user_exit();
	/* NB, the load/unload macros use references on the kernel module, so
	 * as such this handler shouldn't be called unless all channels are
	 * unloaded (rmmod should fail so long as channels are loaded). */
	if (master) {
		is_master = 0;
		/* Disable the interrupts */
		reg_set(regs, IERCC, 0x0);
		reg_set(regs, IIRCC, 0x1);
		FREE_IRQ(pme_hal_irq_ctrl, &global_dev);
		if (sretable_addr)
			pme_mem_phys_free(sretable_addr);
		if (dxetable_addr)
			pme_mem_phys_free(dxetable_addr);
		pme_regmap_unmap(regs);
	}
	pme_mem_finish();
	ret = misc_deregister(&global_dev);
	if (ret)
		printk(KERN_ERR PMMOD "Failed to deregister device %s, "
			"code %d\n", global_dev.name, ret);
	printk(KERN_INFO PMMOD "device %s deregistered\n", global_dev.name);
	update_command_teardown();
	pme_regmap_finish();
	pme_hal_finish();
	printk(KERN_INFO PMMOD "unloaded\n");
}

/* Module data */
MODULE_AUTHOR("Geoff Thorpe <Geoff.Thorpe@freescale.com>");
MODULE_DESCRIPTION("Pattern Matcher Driver");
MODULE_LICENSE("GPL");

/* Module handlers */
module_init(pme_base_init);
module_exit(pme_base_finish);

/****************/
/* Internal API */
/****************/

void pme_wake_stats(void)
{
	spin_lock_bh(&sleeper_lock);
	while (!list_empty(&sleeper_list)) {
		struct stat_sleeper *item = list_entry(sleeper_list.next,
						struct stat_sleeper, list);
		list_del(&item->list);
		list_add_tail(&item->list, &yawner_list);
		item->woken = 1;
		wake_up(&item->queue);
	}
	spin_unlock_bh(&sleeper_lock);
}

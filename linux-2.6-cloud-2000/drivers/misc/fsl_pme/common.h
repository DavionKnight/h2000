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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE US:E OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Author: Geoff Thorpe, Geoff.Thorpe@freescale.com
 *
 * Description:
 * This file provides common definitions for the pattern-matcher driver. The
 * base_*** and user_*** files used to belong to two kernel modules, the former
 * the low-level driver and the latter the memory-mapping/device interface to
 * user-space. They're now one, and this header joins them.
 *
 */

/******************/
/* Kernel headers */
/******************/

#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/sysctl.h>
#include <linux/miscdevice.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/smp_lock.h>
#include <linux/rbtree.h>
#include <linux/pagemap.h>
#include <linux/dma-mapping.h>
#include <linux/uio.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/workqueue.h>
#include <linux/highmem.h>
#include <linux/proc_fs.h>
#include <linux/uio.h>
#include <linux/poll.h>
#include <linux/bootmem.h>
#include <linux/uaccess.h>
#include <linux/mman.h>
#include <linux/log2.h>
#include <linux/vmalloc.h>
#include <sysdev/fsl_soc.h>
#include <asm/mmu.h>

#include <asm/fsl_pme.h>

#define PMMOD	"fsl_pm.ko: "

/******************/
/* base/user join */
/******************/

/* Historically, there was a kernel module called pme_base which provided the
 * low-level device driver, including the control device and the channel
 * devices it allowed you to dynamically create. It provided all admin
 * functions, h/w support and management, and a kernel API for manipulating the
 * driver, allocating and configuring contexts, and performing I/O (for
 * pattern-matcher database manipulations and data-processing for decompression
 * and/or pattern-matching). There was also a kernel module called pme_user
 * which provided user-interface interfaces to the kernel API's I/O functions,
 * including blocking and non-blocking (event completion) interfaces and
 * zero-copy features. Now, they're a single driver - hence the base_*** and
 * user_*** prefixes to the files.
 *
 * These declarations glue the two together for module init/exit. */

int pme_user_init(void);
void pme_user_exit(void);

/*******************/
/* General-purpose */
/*******************/

/* We use bitfields in at least one place and harness the (assembly-optimised)
 * kernel routines for them. The following macros help with the fact these are
 * (unsigned long)-based. Also, a quick expression macro to determine if an
 * argument is a power of 2. */
#define BYTE_WIDTH	sizeof(unsigned long)
#define BIT_WIDTH	(sizeof(unsigned long) * 8)
#define ISEXP2(n)	({ \
			unsigned int __n = (n); \
			__n && !(__n & (__n - 1)); \
			})

/* PS: this driver is for an SoC block whose interface is 32-bit specific. */
static inline u32 ptr_to_u32(void *ptr)
{
	return (u32)ptr;
}
static inline void *u32_to_ptr(u32 val)
{
	return (void *)val;
}
static inline u64 ptr_to_u64(void *ptr)
{
	return (u64)(u32)ptr;
}
static inline void *u64_to_ptr(u64 val)
{
	BUG_ON(val >> 32);
	return (void *)(u32)val;
}

#define DECLARE_GLOBAL(name, t, mt, def, desc) \
	static t name = def; \
	module_param(name, mt, 0444); \
	MODULE_PARM_DESC(name, desc ", default: " __stringify(def));

#define DECLARE_GLOBAL_EXPORTED(name, t, mt, def, desc) \
	t name = def; \
	module_param(name, mt, 0444); \
	MODULE_PARM_DESC(name, desc ", default: " __stringify(def));

#define DECLARE_GLOBAL_STRING(name, maxlen, def, desc) \
	static char name[maxlen] = def; \
	module_param_string(name, name, maxlen, 0444); \
	MODULE_PARM_DESC(name, desc ", default: " __stringify(def));

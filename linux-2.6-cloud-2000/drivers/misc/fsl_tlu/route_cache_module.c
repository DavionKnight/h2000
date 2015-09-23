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
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains the driver module for IP forwarding.
 */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <net/route.h>
#include "route_cache.h"

MODULE_AUTHOR("Zhichun Hua <zhichun.hua@freescale.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TLU Route Cache Driver");

#define TLU_RC_DEVICE_NAME "tlu_rc"
#define TLU_RC_GET_STATS 	1

#ifdef ROUTE_HOOK
static int tlu_rc_major = -1;
static struct file_operations tlu_rc_file_ops;
#endif

int tlu_rc_open(struct inode *inode, struct file *fp)
{
	/* Must return 0. Otherwise may cause kernel segmentation fault. */
	return 0;
}

int tlu_rc_close(struct inode *inode, struct file *fp)
{
	return 0;
}

int tlu_rc_driver_get_stats(void *result)
{
	struct route_cache_stats *stats;

	if (!access_ok(VERIFY_WRITE, result, sizeof(stats)))
		return -1;

	stats = route_cache_get_stats();
	copy_to_user(result, stats, sizeof(struct route_cache_stats));
	return 0;
}

int tlu_rc_ioctl(struct inode *inode, struct file *fp, unsigned int cmd,
		unsigned long param)
{
	switch (cmd) {
	case TLU_RC_GET_STATS:
		return tlu_rc_driver_get_stats((void *)param);
	default:
		printk(KERN_INFO "Unknown ioctl command (%0x8).\n", cmd);
		return -1;
   }
   return 0;
}

int tlu_rc_driver_init(void)
{
#ifdef ROUTE_HOOK
	if (route_cache_init() < 0)
		return 0;

	tlu_rc_file_ops.open = tlu_rc_open;
	tlu_rc_file_ops.release = tlu_rc_close;
	tlu_rc_file_ops.ioctl = tlu_rc_ioctl;

	route_hook_register(route_cache_add,
		route_cache_del, route_cache_lookup);

	tlu_rc_major = register_chrdev(0, TLU_RC_DEVICE_NAME, &tlu_rc_file_ops);
	if (tlu_rc_major < 0) {
		printk(KERN_ERR "ERROR: registration of device %s failed "
			"(err = %d)\n", TLU_RC_DEVICE_NAME, tlu_rc_major);
		route_hook_unregister();
		route_cache_free();
		return 0;
	}
	printk(KERN_INFO "   device %s registered\n", TLU_RC_DEVICE_NAME);
	return 0;
#else
	printk(KERN_ERR "ERROR: Route hook patch does not exist. driver is not loaded\n");
	return -1;
#endif
}

void tlu_rc_driver_cleanup(void)
{
#ifdef ROUTE_HOOK
	if (tlu_rc_major < 0) {
		/* The driver is down */
		return;
	}
	route_hook_unregister();
	route_cache_free();
	unregister_chrdev(tlu_rc_major, TLU_RC_DEVICE_NAME);
	tlu_rc_major = -1;
	printk(KERN_INFO "   device %s deregistered\n", TLU_RC_DEVICE_NAME);
#else
	printk(KERN_ERR "ERROR: Route hook patch does not exist. driver was not loaded\n");
#endif
}

/* Module load/unload handlers */
module_init(tlu_rc_driver_init);
module_exit(tlu_rc_driver_cleanup);

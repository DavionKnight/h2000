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
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Wed Sep 19 2007
 *
 * Description:
 * This file contains linux dirver functions for TLU test software.
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/crt.h>
#include <asm/htk.h>
#include <asm/tlu_driver.h>
#include "test.h"
#include "perf.h"

#define TLU_TEST_DEVICE_NAME "tlu_test"

MODULE_AUTHOR("Zhichun Hua <zhichun.hua@freescale.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TLU Test Driver");

static char *debug = "crt htk acc";
static int test_case;

module_param(debug, charp, 0);
module_param(test_case, int, 0);

static int tlu_test_major;
static const struct file_operations tlu_test_file_ops;
static int stats;

static uint32_t tlu_parse_debug_str(char *str)
{
	uint32_t debug;

	debug = TLU_LOG_CRIT | TLU_LOG_WARN;
	if (strstr(str, "crt"))
		debug |= TLU_LOG_CRT;

	if (strstr(str, "htk"))
		debug |= TLU_LOG_HTK;

	if (strstr(str, "acc"))
		debug |= TLU_LOG_ACC;

	if (strstr(str, "test"))
		debug |= TLU_LOG_TEST;

	if (strstr(str, "stat"))
		stats = 1;

	return debug;
}

int test(struct tlu *tlu)
{
	tlu_log_level = tlu_parse_debug_str(debug);
	printk(KERN_INFO "Debug Level = %08x\n", tlu_log_level);

	switch (test_case) {
	case 0:
		if (tlu_test_basic(tlu, 0, 128, 32, 16) < 0)
			return 0;

		if (tlu_test_htk(tlu) < 0)
			return 0;

		if (tlu_test_crt(tlu, 0, 0) < 0)
			return 0;

		if (tlu_test_crt(tlu, 0, 1) < 0)
			return 0;

		if (tlu_test_crt(tlu, 1, 0) < 0)
			return 0;

		if (tlu_test_crt(tlu, 1, 1) < 0)
			return 0;

		break;
	case 1:
		tlu_test_basic(tlu, 0, 128, 32, 16);
		break;
	case 2:
		tlu_test_htk(tlu);
		break;
	case 3:
		tlu_test_crt(tlu, 0, 0);
		break;
	case 4:
		tlu_test_crt(tlu, 0, 1);
		break;
	case 5:
		tlu_test_crt(tlu, 1, 0);
		break;
	case 6:
		tlu_test_crt(tlu, 1, 1);
		break;
	case 10:
		tlu_perf_test(tlu, 0, 0, stats);
		tlu_perf_test_async(tlu, 0, 0, stats);
		break;
	default:
		printk(KERN_INFO "Invalid test case No. %d\n", test_case);
		break;
	}

	return 0;
}

int tlu_test_driver_init(void)
{
	struct tlu *tlu1, *tlu2;

	tlu_test_major = register_chrdev(0, TLU_TEST_DEVICE_NAME,
			&tlu_test_file_ops);
	if (tlu_test_major < 0) {
		printk(KERN_ERR "ERROR: registration of device %s"
				"failed (err = %d)\n",
				TLU_TEST_DEVICE_NAME, tlu_test_major);
		return 0;
	}
	printk(KERN_INFO "   device %s registered\n", TLU_TEST_DEVICE_NAME);

	tlu1 = tlu_get(0);
	if (tlu1) {
		printk(KERN_INFO " == == == Testing TLU1 ...\n");
		if (test(tlu1) < 0) {
			printk(KERN_ERR "ERROR: test failed\n");
			return 0;
		}
	} else {
		printk(KERN_ERR "ERROR: TLU1 does not exist\n");
	}
	tlu2 = tlu_get(1);
	if (test_case < 10 && tlu2) {
		printk(KERN_INFO " == == == Testing TLU2 ...\n");
		if (test(tlu2) < 0) {
			printk(KERN_ERR "ERROR: test failed\n");
			return 0;
		}
	} else if (tlu1 && tlu2) {
		tlu_perf_test_async_interleave(tlu1, tlu2, 0, 0, stats);
	}
	return 0;
}

void tlu_test_driver_cleanup(void)
{
	unregister_chrdev(tlu_test_major, TLU_TEST_DEVICE_NAME);
	printk(KERN_INFO "   device %s deregistered\n", TLU_TEST_DEVICE_NAME);
}

/* Module load/unload handlers */
module_init(tlu_test_driver_init);
module_exit(tlu_test_driver_cleanup);

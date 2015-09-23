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
 * This file contains the driver of the TLU software.
 */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <asm/tlu.h>

MODULE_AUTHOR("Zhichun Hua <zhichun.hua@freescale.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TLU driver");

#define TLU_DEVICE_NAME "tlu"
#define TLU_DRIVER_MAX_TLU_NUM 2
#define TLU_DRIVER_MAX_BANK_NUM 4

/* Configuration parameters. Maximum two TLUs are supported */
static unsigned int log_level;
static unsigned long tlu0_addr;
static unsigned long tlu0_bank0_addr;
static unsigned int tlu0_bank0_size;
static unsigned int tlu0_bank0_parity;
static unsigned int tlu0_bank0_type;
static unsigned long tlu0_bank1_addr;
static unsigned int tlu0_bank1_size;
static unsigned int tlu0_bank1_parity;
static unsigned int tlu0_bank1_type;
static unsigned long tlu0_bank2_addr;
static unsigned int tlu0_bank2_size;
static unsigned int tlu0_bank2_parity;
static unsigned int tlu0_bank2_type;
static unsigned long tlu0_bank3_addr;
static unsigned int tlu0_bank3_size;
static unsigned int tlu0_bank3_parity;
static unsigned int tlu0_bank3_type;

static unsigned long tlu1_addr;
static unsigned long tlu1_bank0_addr;
static unsigned int tlu1_bank0_size;
static unsigned int tlu1_bank0_parity;
static unsigned int tlu1_bank0_type;
static unsigned long tlu1_bank1_addr;
static unsigned int tlu1_bank1_size;
static unsigned int tlu1_bank1_parity;
static unsigned int tlu1_bank1_type;
static unsigned long tlu1_bank2_addr;
static unsigned int tlu1_bank2_size;
static unsigned int tlu1_bank2_parity;
static unsigned int tlu1_bank2_type;
static unsigned long tlu1_bank3_addr;
static unsigned int tlu1_bank3_size;
static unsigned int tlu1_bank3_parity;
static unsigned int tlu1_bank3_type;

module_param(log_level, int, 0);
module_param(tlu0_addr, ulong, 0);
module_param(tlu0_bank0_addr, ulong, 0);
module_param(tlu0_bank0_size, int, 0);
module_param(tlu0_bank0_parity, int, 0);
module_param(tlu0_bank0_type, int, 0);
module_param(tlu0_bank1_addr, ulong, 0);
module_param(tlu0_bank1_size, int, 0);
module_param(tlu0_bank1_parity, int, 0);
module_param(tlu0_bank1_type, int, 0);
module_param(tlu0_bank2_addr, ulong, 0);
module_param(tlu0_bank2_size, int, 0);
module_param(tlu0_bank2_parity, int, 0);
module_param(tlu0_bank2_type, int, 0);
module_param(tlu0_bank3_addr, ulong, 0);
module_param(tlu0_bank3_size, int, 0);
module_param(tlu0_bank3_parity, int, 0);
module_param(tlu0_bank3_type, int, 0);

module_param(tlu1_addr, ulong, 0);
module_param(tlu1_bank0_addr, ulong, 0);
module_param(tlu1_bank0_size, int, 0);
module_param(tlu1_bank0_parity, int, 0);
module_param(tlu1_bank0_type, int, 0);
module_param(tlu1_bank1_addr, ulong, 0);
module_param(tlu1_bank1_size, int, 0);
module_param(tlu1_bank1_parity, int, 0);
module_param(tlu1_bank1_type, int, 0);
module_param(tlu1_bank2_addr, ulong, 0);
module_param(tlu1_bank2_size, int, 0);
module_param(tlu1_bank2_parity, int, 0);
module_param(tlu1_bank2_type, int, 0);
module_param(tlu1_bank3_addr, ulong, 0);
module_param(tlu1_bank3_size, int, 0);
module_param(tlu1_bank3_parity, int, 0);
module_param(tlu1_bank3_type, int, 0);

static int tlu_major;
static const struct file_operations tlu_file_ops;

struct tlu tlu[TLU_DRIVER_MAX_TLU_NUM];
static int tlu_num;

struct tlu *tlu_get(int id)
{

	if (id >= tlu_num || tlu[id].handle == 0)
		return NULL;

	return tlu + id;
}
EXPORT_SYMBOL(tlu_get);

/* Initialize TLU0 */
int tlu_driver_init_tlu0(void)
{
	struct tlu_bank_param cfg[TLU_DRIVER_MAX_BANK_NUM];
	int bank_num;

	bank_num = 0;
	if (tlu0_addr == 0)
		return 0;

	printk(KERN_INFO "Initializing TLU 0 [%08lx] ....\n", tlu0_addr);
	tlu_num++;
	if (tlu0_bank0_size) {
		cfg[0].addr = tlu0_bank0_addr;
		cfg[0].size = tlu0_bank0_size;
		cfg[0].parity = tlu0_bank0_parity;
		cfg[0].type = tlu0_bank0_type;
		bank_num++;
	}
	if (tlu0_bank1_size) {
		cfg[1].addr = tlu0_bank1_addr;
		cfg[1].size = tlu0_bank1_size;
		cfg[1].parity = tlu0_bank1_parity;
		cfg[1].type = tlu0_bank1_type;
		bank_num++;
	}
	if (tlu0_bank2_size) {
		cfg[2].addr = tlu0_bank2_addr;
		cfg[2].size = tlu0_bank2_size;
		cfg[2].parity = tlu0_bank2_parity;
		cfg[2].type = tlu0_bank2_type;
		bank_num++;
	}
	if (tlu0_bank3_size) {
		cfg[3].addr = tlu0_bank3_addr;
		cfg[3].size = tlu0_bank3_size;
		cfg[3].parity = tlu0_bank3_parity;
		cfg[3].type = tlu0_bank3_type;
		bank_num++;
	}
	if (bank_num == 0) {
		printk(KERN_ERR "ERROR: No bank memory is specified for "
				"TLU0\n");
		return -1;
	}
	if ((tlu_init(tlu, tlu0_addr, bank_num, cfg)) < 0)
		return -1;

	return 1;
}

/* Initialize TLU1 */
int tlu_driver_init_tlu1(void)
{
	struct tlu_bank_param cfg[TLU_DRIVER_MAX_BANK_NUM];
	int bank_num;

	bank_num = 0;
	if (tlu1_addr == 0)
		return 0;

	printk(KERN_INFO "Initializing TLU 1 [%08lx] ....\n", tlu1_addr);
	tlu_num++;
	if (tlu1_bank0_size) {
		cfg[0].addr = tlu1_bank0_addr;
		cfg[0].size = tlu1_bank0_size;
		cfg[0].parity = tlu1_bank0_parity;
		cfg[0].type = tlu1_bank0_type;
		bank_num++;
	}
	if (tlu1_bank1_size) {
		cfg[1].addr = tlu1_bank1_addr;
		cfg[1].size = tlu1_bank1_size;
		cfg[1].parity = tlu1_bank1_parity;
		cfg[1].type = tlu1_bank1_type;
		bank_num++;
	}
	if (tlu1_bank2_size) {
		cfg[2].addr = tlu1_bank2_addr;
		cfg[2].size = tlu1_bank2_size;
		cfg[2].parity = tlu1_bank2_parity;
		cfg[2].type = tlu1_bank2_type;
		bank_num++;
	}
	if (tlu1_bank3_size) {
		cfg[3].addr = tlu1_bank3_addr;
		cfg[3].size = tlu1_bank3_size;
		cfg[3].parity = tlu1_bank3_parity;
		cfg[3].type = tlu1_bank3_type;
		bank_num++;
	}
	if (bank_num == 0) {
		printk(KERN_ERR "ERROR: No bank memory is specified for "
				"TLU1\n");
		return -1;
	}
	if ((tlu_init(tlu + 1, tlu1_addr, bank_num, cfg)) < 0)
		return -1;

	return 1;
}

int tlu_driver_init(void)
{
	int rc;

	tlu_log_level = log_level;
	tlu_num = 0;
	rc = tlu_driver_init_tlu0();
	if (rc <= 0) {
		if (rc < 0) {
			printk(KERN_ERR "ERROR: TLU0 initialize failed\n");
			return 0;
		} else if (rc == 0) {
			printk(KERN_ERR "ERROR: TLU0 is not present. Driver "
					"is not loaded.\n");
			return 0;
		}
	}

	rc = tlu_driver_init_tlu1();
	if (rc <= 0) {
		if (rc < 0) {
			printk(KERN_ERR "ERROR: TLU1 initialize failed\n");
			return 0;
		}
	}

	tlu_major = register_chrdev(0, TLU_DEVICE_NAME, &tlu_file_ops);
	if (tlu_major < 0) {
		printk(KERN_ERR "ERROR: registration of device %s failed "
				"(err = %d)\n", TLU_DEVICE_NAME, tlu_major);
		return 0;
	}
	printk(KERN_INFO "   device %s registered\n", TLU_DEVICE_NAME);

	return 0;
}

void tlu_driver_cleanup(void)
{
	int i;
	for (i = 0; i < tlu_num; i++)
		tlu_free(tlu + i);

	unregister_chrdev(tlu_major, TLU_DEVICE_NAME);
	printk(KERN_INFO "   device %s deregistered\n", TLU_DEVICE_NAME);
}

/* Module load/unload handlers */
module_init(tlu_driver_init);
module_exit(tlu_driver_cleanup);

/*
 *
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 * TDM Test Module.
 * This TDM test module is a small test module which registers with the
 * TDM framework and transfer and receive data via UCC1.
 *
 * Author: Kai Jiang <Kai.Jiang@freescale.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/param.h>
#include <linux/tdm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/tdm.h>

#define DRV_DESC "Test Module for Freescale Platforms with TDM support"
#define DRV_NAME "ucc_tdm_test"

MODULE_LICENSE("GPL");
MODULE_AUTHOR(" Kai Jiang <Kai.Jiang@freescale.com>");
MODULE_DESCRIPTION(DRV_DESC);

static struct task_struct *tdm_thread_task;
static struct tdm_driver test_tdmdev_driver;
static int tdm_thread_state;
static int ucc_num = -1;
static int fun_num;
module_param(ucc_num, int, S_IRUGO);
module_param(fun_num, int, S_IRUGO);
#define READ_LEN (72*32)	/* BD buffer max len */
#define WRITE_LEN 0x80
#define READ_COUNT 4		/* BD ring len */

static int tdm_check_data(void)
{
	int i, j;
	u8 buf[READ_LEN];

	for (j = 0; j < READ_COUNT; j++) {

		memset(buf, 0, READ_LEN);
		tdm_read_direct(test_tdmdev_driver.adapter, buf, READ_LEN);
		for (i = 0; (i < READ_LEN); i++) {
			if (buf[i] != 0xff) {
				for (j = 0; j < WRITE_LEN; j += 4)
					printk(KERN_INFO "%x\t%x\t%x\t%x\n",
						buf[i+j], buf[i+j+1],
						buf[i+j+2], buf[i+j+3]);

				i = i + WRITE_LEN - 1;
			}
		}
	}

	return 0;
}


static int tdm_thread(void *ptr)
{
	u8 buf[READ_LEN];
	int i;

	tdm_thread_state = 1;

	/* start TDM */
	tdm_master_enable(&test_tdmdev_driver);
	/* write data */
	if (fun_num == 1) {
		while (fun_num == 1) {
			memset(buf, 0, READ_LEN);
			tdm_read_direct(test_tdmdev_driver.adapter, buf,
					READ_LEN);
			tdm_write_direct(test_tdmdev_driver.adapter, buf,
					READ_LEN);
		}
	} else {
		for (i = 0; i < WRITE_LEN; i++)
			buf[i] = i;
		tdm_write_direct(test_tdmdev_driver.adapter, buf,
				WRITE_LEN);
		/* read and print data */
		tdm_check_data();
	}
	/* stop TDM */
	tdm_master_disable(&test_tdmdev_driver);

	tdm_thread_state = 0;
	return 0;
}
static int test_attach_adapter(struct tdm_adapter *adap)
{
	pr_debug("tdm-dev: adapter [%s] registered as minor %d\n",
		 adap->name, adap->id);
	tdm_thread_state = 0;
	tdm_thread_task = kthread_run(tdm_thread, NULL, "tdm_thread");

	return 0;
}

static int test_detach_adapter(struct tdm_adapter *adap)
{
	if (tdm_thread_state)
		kthread_stop(tdm_thread_task);

	pr_debug("tdm-dev: adapter [%s] unregistered\n", adap->name);

	return 0;
}

static const struct tdm_device_id test_ucc_tdm_id[] = {
	{ "tdm_ucc_1", 0 },
	{ }
};

static struct tdm_driver test_tdmdev_driver = {
	.attach_adapter	= test_attach_adapter,
	.detach_adapter	= test_detach_adapter,
	.id_table = test_ucc_tdm_id,
};

static int __init tdm_test_init(void)
{
	int ret;
	pr_info("TDM_TEST: " DRV_DESC "\n");

	if ((ucc_num >= 0) && (ucc_num < 8))
		sprintf((char *)test_ucc_tdm_id[0].name, "%s%d",
			"tdm_ucc_", ucc_num);

	test_tdmdev_driver.id = 1;

	/* create a binding with TDM driver */
	ret = tdm_add_driver(&test_tdmdev_driver);
	if (ret == 0)
		pr_info("TDM_TEST module installed\n");
	else
		pr_err("%s tdm_port_init failed\n", __func__);

	return ret;

}

static void __exit tdm_test_exit(void)
{
	fun_num = -1;
	tdm_del_driver(&test_tdmdev_driver);
	pr_info("TDM_TEST module un-installed\n");
}

module_init(tdm_test_init);
module_exit(tdm_test_exit);

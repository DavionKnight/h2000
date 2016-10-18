/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:Kevin_fzs
*  File Name        	:/home/kevin/works/projects/H20PN-2000/drivers/test\drv_log2file.c
*  Create Date        	:2016/07/27 15:37
*  Last Modified      	:2016/07/27 15:37
*  Description    	:
*/


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>	//for copy_to_user
#include <linux/gpio.h>

struct cdev log2file_cdev;
extern int log2file_flag;

#define LOG_FILE  "/home/log/logfile"

#define LOG2FILE_ENABLE		0x1
#define LOG2FILE_DISABLE	0x2 

static long log2file_fs_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        int  retval = 0;

	switch (cmd)
        {
                case LOG2FILE_ENABLE:
        		log2file_flag = 0x55;
        		log2file_flag = 0x55;
                        break;

                case LOG2FILE_DISABLE:
        		log2file_flag = 0x0;
        		log2file_flag = 0x0;
                        break;
                default:
                 retval = -EINVAL;
                break;
        }
        return retval;
}

static struct file_operations log2file_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = log2file_fs_ioctl
};


static struct class *log2file_cls;
int major;
dev_t dev_id;
static int __init log2file_init(void)
{

     	printk(KERN_ALERT "log2file init...");

	alloc_chrdev_region(&dev_id, 0, 1, "log2file");
	major = MAJOR(dev_id);
	
	cdev_init(&log2file_cdev, &log2file_fops);
	cdev_add(&log2file_cdev, dev_id, 1);

	log2file_cls = class_create(THIS_MODULE, "log2file");

	device_create(log2file_cls, NULL, dev_id, NULL, "log2file");	

     	printk(KERN_ALERT "Done\n");

     	return 0;
}

static void __exit log2file_exit(void)
{
	/*disable printk to file*/
	log2file_flag = 0;	
	log2file_flag = 0;	

	device_destroy(log2file_cls, MKDEV(major, 0));
    	class_destroy(log2file_cls);
 
	cdev_del(&log2file_cdev);
 
	unregister_chrdev_region(MKDEV(major, 0), 1);
}

module_init(log2file_init);
module_exit(log2file_exit);
MODULE_LICENSE("GPL");



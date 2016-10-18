/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:Kevin_fzs
*  File Name        	:/home/kevin/works/projects/H20PN-2000/drivers/drv_fileoper\drv_fileoper.c
*  Create Date        	:2016/07/13 15:06
*  Last Modified      	:2016/07/13 15:06
*  Description    	:
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/err.h>

#include <linux/string.h>
#include <linux/errno.h>

#include <asm/fcntl.h>
#include <asm/processor.h>
#include <asm/uaccess.h>

extern void dump_stack(void);

int add_integar(int a,int b)
{ 
	return (a + b);
}

int sub_integar(int a,int b)
{
	return (a - b);
}

int __init iinit_module(void)
{
	int result = 0;
	struct task_struct *task = NULL;
	char *path = NULL,*ptr = NULL;
	char read_buf[100];
	struct file* filp = NULL;
	mm_segment_t old_fs;


	task = current;

	memset(read_buf,'a',100);

	//==================test  read write log file=====================
#define MY_FILE  "/home/log_file.log"
	filp = filp_open(MY_FILE,O_CREAT|O_RDWR,0600);   //创建文件
	if (filp)
	{
		old_fs = get_fs();
		set_fs(get_ds());

		result = filp->f_op->write(filp,read_buf,100,&filp->f_pos);  //写文件
		if (result)
		{
			printk("New Log Write OK Length:%d \n",result);
		}
		else
		{
			printk("Write Log File Error \n");
		}

		set_fs(old_fs);

		filp_close(filp,NULL);
	}
	else
	{
		printk("Create New Log file failtrue!!\n");
	}
	dump_stack();
	printk(KERN_INFO"Loading the module ...KK\n");
	return 0;
}

void __exit icleanup_module(void)
{
	printk(KERN_INFO"Unloading the module...KK...\n");

	return ;
}


MODULE_LICENSE("Dual BSD/GPL");
module_init(iinit_module);
module_exit(icleanup_module);


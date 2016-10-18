/*********************************
 *Author:     zhangjj@bjhuahuan.com
 *date:	      2015-10-20
 *Modified:
 ********************************/
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

extern int wdtCount;
int flag=0;

struct task_stuct *task;

int func(void *data)
{
	while(1)
	{	
		msleep(250);
		printk("ERROR:continue error, flag=%d!!!\n",flag);
	}
}

static int __init wdttest_init(void)
{

     	printk(KERN_ALERT "test driver init...");

	task = kthread_create(func, NULL, "test");	

	wake_up_process(task);

     	printk(KERN_ALERT "Done\n");

     	return 0;
}

static void __exit wdttest_exit(void)
{
     	printk(KERN_ALERT "test driver exit\n");

}

module_init(wdttest_init);
module_exit(wdttest_exit);
MODULE_LICENSE("GPL");



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
		msleep(2000);
		if(wdtCount<=5)
		{
			if(0 < ++flag)
				printk("ERROR:continue error, flag=%d!!!\n",flag);
			printk(KERN_ALERT "error,wdtCount=%d\n",wdtCount);
//			return 0;
		}
//			printk("wdtCount=%d\n",wdtCount);
		flag = 0;
		wdtCount = 0;
	}
}

static int __init wdttest_init(void)
{

     	printk(KERN_ALERT "wdt test driver init...");

	task = kthread_create(func, NULL, "test wdt");	

	wake_up_process(task);

     	printk(KERN_ALERT "Done\n");

     	return 0;
}

static void __exit wdttest_exit(void)
{
     	printk(KERN_ALERT "wdt test driver exit\n");

}

module_init(wdttest_init);
module_exit(wdttest_exit);
MODULE_LICENSE("GPL");



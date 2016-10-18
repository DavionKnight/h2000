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

extern int LOGFILE_EN;
static int __init crash_init(void)
{
        int i = 0;
	int a[10]={0};
	int *p=NULL;

     	printk(KERN_ALERT "crash init...");
	for(i=0;i<10000000;i++)
	{
	*p = 100;
	a[i]=100;	
	a[1000]=1;	
	a[200]=1;	
	a[300]=1;	
	a[400]=1;	
	}
	i = a[100]/0;

     	printk(KERN_ALERT "Done\n");

     	return 0;
}

static void __exit crash_exit(void)
{
}

module_init(crash_init);
module_exit(crash_exit);
MODULE_LICENSE("GPL");



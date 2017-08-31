/*
 * @file	exirq.c
 * @author	fzs
 * @date	2016-12-13
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/nmi.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <asm/irq.h>

#define GET_MASTER_SLAVE_INT 		0x4	
#define GET_BOARD_PLUG_INT 			0x6	

struct irq16_share{
	int id;
	unsigned int *pic_vaddr;
	wait_queue_head_t intsem;
	atomic_t semval;	
	struct irq16_share *next;
};
struct irq16_share *irq16_head = NULL;

extern phys_addr_t get_immrbase(void);

#define MPC85xx_PIC_EIVPR_ADDR(x)	(0x50000 + 0x20*(x))

int add_int_node(struct irq16_share * irq_new)
{
	struct irq16_share *irq16_tmp;

	if(NULL == irq_new)
		return 0;

	if(NULL == irq16_head)
	{
		irq16_head = irq_new;
		return 0;
	}

	irq16_tmp = irq16_head;

	while(NULL != irq16_tmp->next)
	{
		irq16_tmp = irq16_tmp->next;
	}
	irq16_tmp->next = irq_new;
	
	return 0;
	
}
struct irq16_share * get_int_node_by_id(int id)
{
	struct irq16_share *irq16_tmp;

	irq16_tmp = irq16_head;

	while(NULL != irq16_tmp)
	{
		if(irq16_tmp->id == id)
			return irq16_tmp;
		irq16_tmp = irq16_tmp->next;
	}
	
	return NULL;
	
}
int free_int_node()
{
	struct irq16_share *irq16_tmp1,*irq16_tmp2;

	irq16_tmp1 = irq16_head;

	while(NULL != irq16_tmp1)
	{
		irq16_tmp2 = irq16_tmp1;
		irq16_tmp1 = irq16_tmp1->next;
		kfree(irq16_tmp2);
	}
	
	return 0;
	
}

static irqreturn_t exirq_process ( int q, void *dev_id )
{
	u32 regdata;
	struct irq16_share *irq = irq16_head;
	
	while(NULL != irq)	
	{
			regdata = in_be32(irq->pic_vaddr);

			if(regdata & 0x40000000)
			{
		printk("get_int  %d\n",irq->id);
					atomic_set(&irq->semval, 1);
					wake_up_interruptible(&irq->intsem);	
			}
	}

	return IRQ_RETVAL(1);
}

static long exirq_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
		struct irq16_share *irq = NULL;

		if(cmd > 16)
				return 0;

		irq = get_int_node_by_id(cmd);
		
		printk("get_int_node_by_id %d\n",cmd);
		if(NULL == irq)
		{
			printk("arg = %d err\n", cmd);
			return 0;
		}	
		printk("get_int_node_by_id succ, before wait %d\n",cmd);

		wait_event_interruptible(irq->intsem,atomic_read(&irq->semval)!=0);
		atomic_set(&irq->semval, 0);
		printk("get_int_node_by_id succ,af wait %d\n",cmd);

		return 0;
}

static struct file_operations exirq_fops = {
         .owner = THIS_MODULE,
         .unlocked_ioctl = exirq_ioctl
};

struct cdev exirq_cdev; 
int major;
int exirq_int_add(int id)
{
	unsigned int regdata = 0;
	struct irq16_share *int16 = NULL;

	int16 = (struct irq16_share *)kmalloc(sizeof(struct irq16_share),GFP_KERNEL);
	if(NULL == int16)
	{
		printk("exirq_int_add kmalloc err\n");
		return -1;
	}
	int16->id = id;
	init_waitqueue_head(&int16->intsem);
	atomic_set(&int16->semval, 0);
	int16->next = NULL;

	int16->pic_vaddr =(unsigned int *)ioremap(get_immrbase() + MPC85xx_PIC_EIVPR_ADDR(id), 64);

	add_int_node(int16);

#if 1
	/* init EIVPR4 to 0x80000 */
	regdata = 0x80000; /* priority=8; vector=0 */
	out_be32(int16->pic_vaddr, regdata);

	/* init EIDR4 to let core process the int*/
	regdata  = 2;
	out_be32((int16->pic_vaddr+4), regdata);
#endif

	return 0;
}

static struct class *exirq_cls;
dev_t dev_id;
static int __init exirq_init(void)  
{  
		int ret = 0;
		printk("exirq init...\n");

		exirq_int_add(4);
		exirq_int_add(6);

		ret = request_irq(16, exirq_process, 0x80, "IRQ16",1); 
		if (ret < 0) 
				printk("irq 16 request fail.\n");
		else 
				printk("irq 16 request success.\n");

		alloc_chrdev_region(&dev_id, 0, 1, "exirq");
		major = MAJOR(dev_id);

		cdev_init(&exirq_cdev, &exirq_fops);
		cdev_add(&exirq_cdev, dev_id, 1);

		exirq_cls = class_create(THIS_MODULE, "exirq");

		device_create(exirq_cls, NULL, dev_id, NULL, "exirq");	

		return 0;  
}

static void __exit exirq_exit(void)  
{
		printk(KERN_ALERT "exirq driver exit\n");

		free_int_node();

		device_destroy(exirq_cls, MKDEV(major, 0));
		class_destroy(exirq_cls);

		/* 3.3 删除cdev*/
		cdev_del(&exirq_cdev);

		/* 3.4 释放设备号*/
		unregister_chrdev_region(MKDEV(major, 0), 1);
		free_irq(16,1);
}  

module_init(exirq_init);  
module_exit(exirq_exit);  
MODULE_LICENSE("GPL");  
MODULE_AUTHOR("fzs");



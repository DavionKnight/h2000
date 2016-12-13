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

extern void open_softirq(int nr, void (*action)(struct softirq_action *));
static atomic_t ex_interrupt_has_happen = ATOMIC_INIT(0);
static wait_queue_head_t ex_interrupt;

#define MPC85xx_PIC_EIVPR4		0x50080
#define MPC85xx_PIC_EIVPR5		0x500A0

#define WAIT_FOR_EXTERN_INTERRUPT	0x1

#if 0
static irqreturn_t int4_irq_process ( int irq, void *dev_id )
{
    //struct irq_info *i = dev_id;
    int handled = 0;
    u32 regdata;

    regdata = in_be32(pic_vaddr);
    //printk("pic_eivpr5 value = %x \n", regdata);
    if(regdata & 0x40000000)
        queue_work(int_process_wq4, &fpga_irq4);  


    //spin_lock(&i->lock);
    handled = 1;

    //spin_unlock(&i->lock);

    return IRQ_RETVAL(handled);
}
#endif

static long exirq_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
                                                                                       
        switch (cmd)                                                                      
        {
		case WAIT_FOR_EXTERN_INTERRUPT:
			init_waitqueue_head(&ex_interrupt);
			printk("wait for extern interrupt\n");
			wait_event_interruptible(ex_interrupt,
				atomic_read(&ex_interrupt_has_happen)!=0);
			atomic_set(&ex_interrupt_has_happen, 0);
			printk("get an interrupt\n");
		break;
		default:
			printk("cmd=%d error\n",cmd);
		break;
	}
	return 0;
}
static struct timer_list test_timer;
static void timer_action(unsigned long aa)
{
	atomic_set(&ex_interrupt_has_happen, 1);
	wake_up_interruptible(&ex_interrupt);	
	
	test_timer.expires = jiffies+(HZ*10);
	add_timer(&test_timer);
}

static struct file_operations exirq_fops = {
         .owner = THIS_MODULE,
         .unlocked_ioctl = exirq_ioctl
};

struct cdev exirq_cdev; 
int major;
int exirq_int_init(void)
{
	dev_t dev_id;

	alloc_chrdev_region(&dev_id, 0, 1, "exirq");                                   
	major = MAJOR(dev_id);                                                            

	cdev_init(&exirq_cdev, &exirq_fops);                                        
	cdev_add(&exirq_cdev, dev_id, 1); 


	init_waitqueue_head(&ex_interrupt);
#if 0
	pic_vaddr = ioremap(get_immrbase() + MPC85xx_PIC_EIVPR4, 64);

	/* init EIVPR4 to 0x80000 */
	regdata = 0x80000; /* priority=8; vector=0 */
	out_be32(pic_vaddr, regdata);

	/* init EIDR4 to let core process the int*/
	regdata  = 2;
	out_be32((pic_vaddr+4), regdata);
#endif
#if 0
	pic_vaddr = ioremap(get_immrbase() + MPC85xx_PIC_EIVPR5, 64);

	/* init EIVPR5 to 0x80000 */
	regdata = 0x80000; /* priority=8; vector=0 */
	out_be32(pic_vaddr, regdata);

	/* init EIDR5 to let core process the int*/
	regdata  = 2;
	out_be32((pic_vaddr+4), regdata);
#endif
#if 0
	/* we must use IRQF_SHARED mode. here IRQF_SHARED=0x80 */
	ret = request_irq(16, int4_irq_process, 0x80, "IRQ4",1); 
	if (ret < 0) 
		printk("irq4 request fail.\n");
	else 
		printk("irq4 request success.\n");
#else
	init_timer(&test_timer);

	test_timer.function = timer_action;

	test_timer.expires = jiffies + (HZ * 10);

	add_timer(&test_timer);
#endif
	return 0;
}

static int __init exirq_init(void)  
{  
  	printk("exirq init...\n");

  	exirq_int_init();
	
	/* for test */
	/*
	fpga_int_register_callback(0,test1);
	fpga_int_register_callback(1,test2);
	*/


  	return 0;  
}  

static void __exit exirq_exit(void)  
{
	
}  

module_init(exirq_init);  
module_exit(exirq_exit);  
MODULE_LICENSE("GPL");  
MODULE_AUTHOR("fzs");



#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/types.h>

extern int unitboard_fpga_write(unsigned char slot, unsigned short addr, unsigned short *wdata);
extern int unitboard_fpga_read(unsigned char slot, unsigned short addr, unsigned short *wdata);
extern int unitboard_fpga_read2(unsigned char slot, unsigned short addr, unsigned short *wdata, size_t count);


#if 0
int flash_thread(void *d)
{
	unsigned short i=0,j = 0;
	loff_t addr = 0x80000;
	unsigned char data_w[256], data_r[256];

	printk("before erase\n");

	return 0;
}
#endif

static int __init hello_init(void)
{
     	printk(KERN_ALERT "driver init!\n");
	
	unsigned short data;
#if 0
	unitboard_fpga_read(2,0,&data);	
	printk("version:0x%x\n",data);
	unitboard_fpga_read(2,2,&data);	
	printk("hwversion:0x%x\n",data);
	unitboard_fpga_read(2,0x84,&data);	
	printk("1data:0x%x\n",data);
	data ^= 0xffff;
	printk("data:0x%x\n",data);

	unitboard_fpga_write(2,0x84,&data);
	unitboard_fpga_read(2,0x84,&data);	
	printk("2data:0x%x\n",data);
#endif
	unsigned short wdata[16] = {0};
	unitboard_fpga_read2(2,0x80,wdata,4);
	printk("0=0x%x\n",wdata[0]);	
	printk("1=0x%x\n",wdata[1]);	
	printk("2=0x%x\n",wdata[2]);	
	printk("3=0x%x\n",wdata[3]);	
	printk("4=0x%x\n",wdata[4]);	
	memset(wdata,0,16*2);
	unitboard_fpga_read2(2,0x80,wdata,5);
	printk("0=0x%x\n",wdata[0]);	
	printk("1=0x%x\n",wdata[1]);	
	printk("2=0x%x\n",wdata[2]);	
	printk("3=0x%x\n",wdata[3]);	
	printk("4=0x%x\n",wdata[4]);	
	printk("5=0x%x\n",wdata[5]);	
	

     	return 0;
}

static void __exit hello_exit(void)
{
     printk(KERN_ALERT "hello driver exit\n");
}

module_init(hello_init);
module_exit(hello_exit);




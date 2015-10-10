#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/types.h>

//extern int dpll_spi_read(unsigned short addr, unsigned char *data, size_t count);
//extern int dpll_spi_write(unsigned short addr, unsigned char *data, size_t count);

//extern int fpga_spi_read(unsigned short addr, unsigned char *data, size_t count);
//extern int fpga_spi_write(unsigned short addr, unsigned char *data, size_t count);
extern int flash_spi_read(loff_t addr, unsigned char *data, size_t count);
extern int flash_spi_write(loff_t addr, unsigned char *data, size_t count);
extern int flash_spi_sector_erase(unsigned int addr);
extern int flash_spi_chip_erase();

static struct task_struct *test_task;
//static struct task_struct *test_task1;
#if 0
int dpll_thread(void *d){
	unsigned short i=0,data = 0;
	printk("come in dpll thread\n");
	dpll_spi_read(0x03,(unsigned char *)&data,2);
	printk("03 = 0x%x\n",data);
	while(i>=0)
	{
		if(-1 == dpll_spi_write(0x04,(unsigned char *)&i,2))
		{
			printk("dpll write error\n");
		}
	
		if(-1 ==dpll_spi_read(0x04,(unsigned char *)&data,2))
		{
			printk("dpll read error\n");
		}
		if(i!=data)
		{
			printk("dpll ERROR,i=0x%x,data=0x%x\n",i,data);
			break;
		}
		i++;
		if(i>0xfff0)
			i = 0;
		msleep(2);
	}
	return 0;
}
#endif
int flash_thread(void *d)
{
	unsigned short i=0,j = 0;
	loff_t addr = 0x80000;
	unsigned char data_w[256], data_r[256];

	printk("before erase\n");

	flash_spi_sector_erase(0x80000);

	while(i < 257)
	{
		memset(data_w,0,sizeof(data_w));
		memset(data_r,0,sizeof(data_r));

		for(j=0;j<256;j++)
		{
			if(1%2)
			{
				data_w[j] = j;
			}
			else
			{
				data_w[j] = 256 - j;
			}
		}
		if(-1 == flash_spi_write(addr,data_w, (size_t)sizeof(data_w)))
		{
			printk("fpga write error\n");
		}
		msleep(10);
		if(-1 == flash_spi_read(addr,data_r,(size_t)sizeof(data_r)))
		{
			printk("fpga read error\n");
		}
		if(memcmp(data_w,data_r,sizeof(data_w))||(i == 0))
		{
			printk("\naddr=0x%x, i = %d\n",addr,i);
			printk("\nmemcmp=0x%d\n",memcmp(data_w,data_r,sizeof(data_w)));
			printk("write data:\n");
			for(j = 0;j<256;j++)
			{
				printk("0x%02X ",data_w[j]);
				if((j+1)%16==0)
					printk("\n");
			}
			printk("read data:\n");
			for(j = 0;j<256;j++)
			{
				printk("0x%02X ",data_r[j]);
				if((j+1)%16==0)
					printk("\n");
			}
			if(i!=0)
			{
				printk("Error,i=%d\n",i);
				break;
			}
		}
		i++;
		addr+=256;
		msleep(2);
	}
	printk("\nend\n");
	return 0;
}

static int __init hello_init(void)
{
     	printk(KERN_ALERT "driver init!\n");

	test_task = kthread_create(flash_thread,NULL , "test_task");
//	test_task1 = kthread_create(fpga_thread,NULL , "test1_task");
    	wake_up_process(test_task);

     	return 0;
}

static void __exit hello_exit(void)
{
     printk(KERN_ALERT "hello driver exit\n");
}

module_init(hello_init);
module_exit(hello_exit);




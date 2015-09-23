
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include "fpga.h"
#include <linux/semaphore.h>


struct semaphore spi_sem;
#define DEBUG_FPGA 

#ifdef DEBUG_FPGA
#define debugk(fmt,args...) printk(fmt ,##args)
#else
#define debugk(fmt,args...)
#endif

#define FPGA_REG_ADDR_MAX	0x0fff
#define MULTI_REG_LEN_MAX		12

/*fpga  device struct */
struct fpga_dev_t {
	struct cdev *pdev;
	struct spi_device *spi;
	int devno;
};

static struct fpga_dev_t *fpga = NULL;

int spi_flag=0xff;

static ssize_t fpga_cdev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	unsigned int addr = *f_pos;
	unsigned char data[MULTI_REG_LEN_MAX] = {0};

	if (count > MULTI_REG_LEN_MAX || (addr + count) > (FPGA_REG_ADDR_MAX + 1)) {
		debugk("fpga spi write out of range.\n");
		return 0;
	}
	
	if (fpga_spi_read((unsigned short)addr, data, count) < 0) {
		debugk("fpga spi read failed.\n");
	
		return 0;
	}

	
	copy_to_user(buf, &data[0], count);

	return count;
}

static ssize_t fpga_cdev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	unsigned int addr = *f_pos;
	unsigned char data[MULTI_REG_LEN_MAX] = {0};

	if (count > MULTI_REG_LEN_MAX || (addr + count) > (FPGA_REG_ADDR_MAX + 1)) {
		debugk("fpga spi write out of range.\n");
		return 0;
	}
		
	copy_from_user(data, buf, count);
	
	if (fpga_spi_write((unsigned short)addr, data, count) < 0) {
		debugk("fpga spi write failed.\n");
		
		return 0;
	}
	
	return count;
}

static loff_t fpga_cdev_lseek (struct file *file, loff_t offset, int origin)
{
	if (offset < 0 || offset > FPGA_REG_ADDR_MAX)
		return (off_t)-1;

	file->f_pos = offset;
	debugk("fpga spi fpos move to 0x%04x\n", (unsigned int)file->f_pos);
	
	return file->f_pos;
}

static struct file_operations fpga_cdev_fops = 
{
	owner:		THIS_MODULE,
	read:		fpga_cdev_read,
	write:		fpga_cdev_write,
	llseek:		fpga_cdev_lseek,
};

int fpga_spi_write(unsigned short addr, unsigned char *data, size_t count)
{
	int val;
	unsigned short address = 0;
	unsigned char buf[MULTI_REG_LEN_MAX + 2] = {0};

	if (!data || count > MULTI_REG_LEN_MAX)
		return -EINVAL;

        if(1 == count)
        {
            buf[0] = SPI_FPGA_WR_SINGLE;
        }
        else
        {
            buf[0] = SPI_FPGA_WR_BURST;
        }	
	address = addr;
	buf[1] = (unsigned char)((address >> 8) & 0xff);
	buf[2] = (unsigned char)((address) & 0xff);

	if( addr == 0x12)
	{
		
		down(&spi_sem);
		spi_flag = data[1];
	}

	memcpy(&buf[3], data, count);

#ifdef DEBUG_FPGA
	{
		int i;
		debugk("spi write %d bytes:", count );
		for (i = 0; i < (count+1 ); i++) {
			debugk(" %02x", buf[i]);
		}		
		debugk("\n");
	}
#endif
	//down(&spi_sem);
	val = spi_write(fpga->spi, buf, count+3 );

	//up(&spi_sem);
	return val;
}
EXPORT_SYMBOL(fpga_spi_write);
EXPORT_SYMBOL(spi_flag);
EXPORT_SYMBOL(spi_sem);

int fpga_spi_read(unsigned short addr, unsigned char *data, size_t count)
{
	int ret;
	struct spi_message message;
	struct spi_transfer xfer;

	unsigned short address = 0;
	unsigned char buf[MULTI_REG_LEN_MAX + 2] = {0};
	unsigned char rx_buf[MULTI_REG_LEN_MAX + 2] = {0};

	if (!data || count > MULTI_REG_LEN_MAX)
		return -EINVAL;

	address = addr;
        if(1 == count)
        {
            buf[0] = SPI_FPGA_RD_SINGLE;        
        }
        else 
        {
            buf[0] = SPI_FPGA_RD_BURST;
        }
	buf[1] = (unsigned char)((address >> 8) & 0xff);
	buf[2] = (unsigned char)((address) & 0xff);


	memcpy(&buf[3], data, count);
	//down(&spi_sem);
	/* Build our spi message */
	spi_message_init(&message);
	memset(&xfer, 0, sizeof(xfer));
	/*xfer.len = count + 2; */
	xfer.len = count ;
	/* Can tx_buf and rx_buf be equal? The doc in spi.h is not sure... */
	xfer.tx_buf = buf;
	xfer.rx_buf = rx_buf;

#ifdef DEBUG_FPGA
	{
		int i;
		debugk("spi write %d bytes:", xfer.len+1);
		for (i = 0; i < xfer.len+1; i++) {
			debugk(" %02x", buf[i]);
		}
		debugk("\n");
	}
#endif

	spi_message_add_tail(&xfer, &message);

	ret = spi_sync(fpga->spi, &message);

#ifdef DEBUG_FPGA
	{
		int i;
		debugk("spi read %d bytes:", xfer.len);
		for (i = 0; i < xfer.len; i++) {
			debugk(" %02x", rx_buf[i]);
		}
		debugk("\n");
	}
#endif
	
	memcpy(data, &rx_buf[0], count);
	//up(&spi_sem);
	return ret;
}
EXPORT_SYMBOL(fpga_spi_read);
static int __devinit fpga_probe(struct spi_device *spi)
{
	int fpga_dev_no;
	struct cdev *pdev;

	
	alloc_chrdev_region(&fpga_dev_no, 0, 1, "fpga");

	#ifdef DEBUG_FPGA
	debugk("FPGA reg alloced major num: %u\n", MAJOR(fpga_dev_no));
	#endif

	
	pdev = cdev_alloc();
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	pdev->ops = &fpga_cdev_fops;
	cdev_add(pdev, fpga_dev_no, 1);

	spi->mode = SPI_MODE_3;
	spi->bits_per_word = 8;
	spi->max_speed_hz = 2000000;
	spi->chip_select =  0;// 0 fpga 1 dpll
	spi_setup(spi);

	fpga = kzalloc(sizeof *fpga, GFP_KERNEL);
	if (!fpga) {
		cdev_del(pdev);
		unregister_chrdev_region(fpga_dev_no, 1);
		return -ENOMEM;
	}
	fpga->spi = spi;
	fpga->pdev = pdev;
	fpga->devno = fpga_dev_no;
	dev_set_drvdata(&spi->dev, fpga);

	/* T.B.D read id to check if connected */

	return 0;
}

static int __devexit fpga_remove(struct spi_device *spi)
{
	if (fpga) {
	
		if (fpga->pdev)
			cdev_del(fpga->pdev);

		if (fpga->devno)
			unregister_chrdev_region(fpga->devno, 1);
			
		kfree(fpga);
	}

	return 0;
}

static struct spi_driver fpga_driver = {
	.driver = {
		.name	= "spi-fpga",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe	= fpga_probe,
	.remove = __devexit_p(fpga_remove),
};

static __init int fpga_init(void)
{
	int val=1;
	sema_init(&spi_sem,val);		
	debugk("fpga spi driver init\n");
	spi_register_driver(&fpga_driver);

	//iounmap(gpio);
#if 0
	if (init_input_clock() != 0)
		printk("dpll init input clock failed.\n");
#endif
	return 0;
}
module_init(fpga_init);

static __exit void fpga_exit(void)
{
	debugk("fpga spi driver exit\n");
	spi_unregister_driver(&fpga_driver);
}
module_exit(fpga_exit);

MODULE_DESCRIPTION ("FPGA spi driver");
MODULE_AUTHOR ("tianzy@huahuan.com");
MODULE_LICENSE ("GPL");


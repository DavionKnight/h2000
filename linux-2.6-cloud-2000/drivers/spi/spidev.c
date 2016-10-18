/*
 * spidev.c -- simple synchronous userspace interface to SPI devices
 *
 * Copyright (C) 2006 SWAPP
 *	Andrea Paterniani <a.paterniani@swapp-eng.it>
 * Copyright (C) 2007 David Brownell (simplification, cleanup)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/delay.h>

#include <asm/uaccess.h>
#include "w25p16.h"
#include <asm/uaccess.h>
#include "w25p16.h"
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>

/*
 * This supports acccess to SPI devices using normal userspace I/O calls.
 * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
 * and often mask message boundaries, full SPI support requires full duplex
 * transfers.  There are several kinds of internal message boundaries to
 * handle chipselect management and other protocol options.
 *
 * SPI has a character major number assigned.  We allocate minor numbers
 * dynamically using a bitmask.  You must use hotplug tools, such as udev
 * (or mdev with busybox) to create and destroy the /dev/spidevB.C device
 * nodes, since there is no fixed association of minor numbers with any
 * particular SPI bus or device.
 */
#define SPIDEV_MAJOR			153	/* assigned */
#define N_SPI_MINORS			32	/* ... up to 256 */

static DECLARE_BITMAP(minors, N_SPI_MINORS);


/* Bit masks for spi_device.mode management.  Note that incorrect
 * settings for some settings can cause *lots* of trouble for other
 * devices on a shared bus:
 *
 *  - CS_HIGH ... this device will be active when it shouldn't be
 *  - 3WIRE ... when active, it won't behave as it should
 *  - NO_CS ... there will be no explicit message boundaries; this
 *	is completely incompatible with the shared bus model
 *  - READY ... transfers may proceed when they shouldn't.
 *
 * REVISIT should changing those flags be privileged?
 */
#define SPI_MODE_MASK		(SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
				| SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
				| SPI_NO_CS | SPI_READY)

struct spidev_data {
	dev_t			devt;
	spinlock_t		spi_lock;
	struct spi_device	*spi;
	struct list_head	device_entry;

	/* buffer is NULL unless this device is open (users > 0) */
	struct mutex		buf_lock;
	unsigned		users;
	u8			*buffer;
};

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static unsigned bufsiz = 4096;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

/*-------------------------------------------------------------------------*/
static struct spidev_data	 *spidev;
#define DS31400_REG_ADDR_MAX	0xffff
#define MULTI_REG_LEN_MAX		512
//#define MULTI_REG_LEN_MAX		2

#define FLASH_FPGA              2
#define DS31400_CHIP	    1
#define FPGA_CHIP			0
#define GPIO_FPGAFLASH         12

#define SPI_FPGA_WR_SINGLE 0x01
#define SPI_FPGA_WR_BURST  0x02
#define SPI_FPGA_RD_BURST  0x03
#define SPI_FPGA_RD_SINGLE 0x05

static struct mutex			 chip_sel_lock;
static unsigned short chip_select=  DS31400_CHIP;
#undef DEBUG_MIX
//#define DEBUG_MIX
#ifdef DEBUG_MIX
#define debugk(fmt,args...) printk(fmt ,##args)
#else
#define debugk(fmt,args...)
#endif

/*-------------------------------------------------------------------------*/
struct w25p flash;

/*
 * We can't use the standard synchronous wrappers for file I/O; we
 * need to protect against async removal of the underlying spi_device.
 */
static void spidev_complete(void *arg)
{
	complete(arg);
}

static ssize_t
spidev_sync(struct spidev_data *spidev, struct spi_message *message)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int status;

	message->complete = spidev_complete;
	message->context = &done;

	spin_lock_irq(&spidev->spi_lock);
	if (spidev->spi == NULL)
		status = -ESHUTDOWN;
	else
		status = spi_async(spidev->spi, message);
	spin_unlock_irq(&spidev->spi_lock);

	if (status == 0) {
		wait_for_completion(&done);
		status = message->status;
		if (status == 0)
			status = message->actual_length;
	}
	return status;
}

static inline ssize_t
spidev_sync_write_mix(struct spidev_data *spidev,  u8 *buffer, size_t len)
{
	struct spi_transfer	t = {
			.tx_buf		=  buffer,
			.len		= len,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

static inline ssize_t
spidev_sync_write(struct spidev_data *spidev, size_t len)
{
	struct spi_transfer	t = {
			.tx_buf		= spidev->buffer,
			.len		= len,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

static inline ssize_t
spidev_sync_read(struct spidev_data *spidev, size_t len)
{
	struct spi_transfer	t = {
			.rx_buf		= spidev->buffer,
			.len		= len,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}


/*-------------------------------------------------------------------------*/

int mix_spi_read(struct spi_device *spi,unsigned short addr, unsigned char *data, size_t count)
{
	int ret;
	struct spi_message message;
	struct spi_transfer xfer;

	unsigned short address = 0;
	unsigned char buf[MULTI_REG_LEN_MAX + 2] = {0};
	unsigned char rx_buf[MULTI_REG_LEN_MAX + 2] = {0};
	
	//unsigned short chip_select=E1_CHIP;
	//chip_select = FPGA_CHIP;
	
	if (!data || count > MULTI_REG_LEN_MAX)
		return -EINVAL;
	
	//printk("chip_select = %d\n", chip_select);

	
	if(chip_select == DS31400_CHIP)
	{
		//printk("\n +++++++++++++++ ds314000++++++++++++++++++\n");
		address = (addr << 1) & 0x7ffe;

		buf[0] = (unsigned char)((address >> 8) & 0xff);
		buf[1] = (unsigned char)((address) & 0xff);

		/* MSB must be '1' to read */
		buf[0] |= 0x80;
		/* LSB must be '1' to burst read */
		if (count > 1)
			buf[1] |= 0x01;

		memcpy(&buf[2], data, count);

		/* Build our spi message */
		spi_message_init(&message);
		memset(&xfer, 0, sizeof(xfer));
		xfer.len = count + 2; 
		//xfer.len = count ;
		/* Can tx_buf and rx_buf be equal? The doc in spi.h is not sure... */
		xfer.tx_buf = buf;
		xfer.rx_buf = rx_buf;
	}
	else if(chip_select == FPGA_CHIP)
	{
	//	printk("\n +++++++++++++++ fpga++++++++++++++++++\n");
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
		buf[3] = 0;

		memcpy(&buf[4], data, count);
		/* Build our spi message */
		spi_message_init(&message);
		memset(&xfer, 0, sizeof(xfer));
		
	//	xfer.bits_per_word = 8;
		xfer.len = count + 4;
	//	xfer.speed_hz = 4000000;
		
		//xfer.len = count ;
		/* Can tx_buf and rx_buf be equal? The doc in spi.h is not sure... */
		//memcpy((unsigned char *)xfer.tx_buf, buf,  xfer.len);
		xfer.tx_buf = buf;
		xfer.rx_buf = rx_buf;	
#if 0	
		dev_printk(KERN_INFO, &spidev->spi->dev,
			"  xfer len %zd %s%s%s%dbits %u usec %uHz\n",
			xfer.len,
			xfer.rx_buf ? "rx " : "",
			xfer.tx_buf ? "tx " : "",
			xfer.cs_change ? "cs " : "",
			xfer.bits_per_word ? : spidev->spi->bits_per_word,
			xfer.delay_usecs,
			xfer.speed_hz ? : spidev->spi->max_speed_hz);
#endif
	}

#ifdef DEBUG_MIX
	{
		int i;
		printk("spi write %d bytes:", xfer.len);
		for (i = 0; i < xfer.len; i++) {
			printk(" %02x", buf[i]);
		}
		printk("\n");
	}
#endif

	spi_message_add_tail(&xfer, &message);

	ret = spi_sync(spi, &message);
	//ret = spidev_sync(spi, &message);
	//ret = spi_write_then_read(spi, buf, 3 ,  rx_buf, count);


#ifdef DEBUG_MIX
	{
		int i;
		debugk("spi read %d bytes:", xfer.len);
		for (i = 0; i < xfer.len; i++) {
			debugk(" %02x", rx_buf[i]);
		}
		debugk("\n");
	}
#endif

	{
#if 0
		int i;
		printk("spi read %d bytes: chipsetl  = %d", xfer.len, chip_select);
		for (i = 0; i < xfer.len; i++) {
			printk(" %02x", rx_buf[i]);
		}
		printk("\n");
#endif
	}
	/* memcpy(data, &rx_buf[2], count); */
	if(chip_select  ==  DS31400_CHIP)
	   memcpy(data, &rx_buf[2], count);
	else if(chip_select  ==  FPGA_CHIP)
	{
	   memcpy(data, &rx_buf[4], count);
	}
	return ret;
}
//EXPORT_SYMBOL(mix_spi_read);
//#endif


int mix_spi_write(struct spi_device *spi ,unsigned short addr, unsigned char *data, size_t count)
{
	unsigned short address = 0;
	unsigned char buf[MULTI_REG_LEN_MAX + 2] = {0};
	//unsigned short chip_select=E1_CHIP;

	if (!data || count > MULTI_REG_LEN_MAX)
		return -EINVAL;

	//fpga_spi_read(0x12, &chip_select,2);

	if(chip_select == DS31400_CHIP)
	{
		address = (addr << 1) & 0x7ffe;

		buf[0] = (unsigned char)((address >> 8) & 0xff);
		buf[1] = (unsigned char)((address) & 0xff);

		/* MSB must be '0' to write */
		buf[0] &= ~0x80;
		/* LSB must be '1' to burst write */
		if (count > 1)
			buf[1] |= 0x01;

		memcpy(&buf[2], data, count);
	
#ifdef DEBUG_MIX
		{
			int i;
			debugk("spi write %d bytes:", count );
			//printk("spi write %d bytes:", count );
			for (i = 0; i < (count ); i++) {
				debugk(" %02x", buf[i]);
			}		
			debugk("\n");
			//printk("\n");
		}
#endif

		return spi_write(spi, buf, count+2 );
		//return spidev_sync_write_mix(spidev,  buf, (size_t)(count + 2) );
	}
	else if(chip_select == FPGA_CHIP)
	{
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


		memcpy(&buf[3], data, count);

#ifdef DEBUG_MIX
		{
			int i;
			debugk("spi write %d bytes:", count );
			for (i = 0; i < (count+3 ); i++) {
				debugk(" %02x", buf[i]);
			}		
			debugk("\n");
		}
#endif

		return spi_write(spi, buf, count+4 );	
	}
 	return 0;
}
//EXPORT_SYMBOL(mix_spi_write);

int dpll_spi_write(unsigned short addr, unsigned char *data, size_t count)
{
	int ret;
	struct spi_device	*spi;
        unsigned char chip_se = 0;
        unsigned short chip_se_bak = 0;

	spin_lock_irq(&spidev->spi_lock);
	spi = spi_dev_get(spidev->spi);
	spin_unlock_irq(&spidev->spi_lock);

	mutex_lock(&chip_sel_lock);
        chip_se = spi->chip_select;
        chip_se_bak = chip_select;

#if 0
	//only a few chance to alloc memory
	if (!spidev->buffer) {
			spidev->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (!spidev->buffer) {
				dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
				return -ENOMEM;
			}
			flag = 1;
		}

	//mutex_lock(&chip_sel_lock);
#endif
	  spi->chip_select = 1; // 0 fpga 1 dpll
//		   spi_setup(spi);
	       chip_select = DS31400_CHIP;

//	mutex_lock(&spidev->buf_lock);	
	
	//printk("now in dpll_write.++++++++++++\n");
	ret = mix_spi_write(spi, addr, data, count);
	
//	mutex_unlock(&spidev->buf_lock);	

#if 0	
	if(flag == 1)
     		kfree(spidev->buffer);	
#endif
          spi->chip_select = chip_se; // 0 fpga 1 dpll
          chip_select = chip_se_bak;

	mutex_unlock(&chip_sel_lock);
	
	 
	return ret;
}
EXPORT_SYMBOL(dpll_spi_write);

int dpll_spi_read(unsigned short addr, unsigned char *data, size_t count)
{	
	int ret = 0;
	struct spi_device	*spi;
        unsigned char chip_se = 0;
        unsigned short chip_se_bak = 0;

	//printk("now in dpll_read.+1+++++++++++\n");
	spin_lock_irq(&spidev->spi_lock);
	spi = spi_dev_get(spidev->spi);
	spin_unlock_irq(&spidev->spi_lock);
	//printk("now in dpll_read.++++2++++++++\n");

	mutex_lock(&chip_sel_lock);
        chip_se = spi->chip_select;
        chip_se_bak = chip_select;

#if 0
	//only a few chance to alloc memory
	if (!spidev->buffer) {
			spidev->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (!spidev->buffer) {
				dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
				return -ENOMEM;
			}
			flag = 1;
		}
#endif

#if 1
	//mutex_lock(&chip_sel_lock);
	  spi->chip_select = 1; // 0 fpga 1 dpll
//           spi_setup(spi);
	  chip_select = DS31400_CHIP;
#endif	
    //mutex_lock(&spidev->buf_lock);	
	//printk("now in dpll_read.++++++++++++\n");
	{
		int i;
		int loop;
	
		if( (count % 2 ) == 0 )	
		  	loop = count / 2;
		else
			loop = (count + 1) / 2;				
			
		//printk("\n ++++++++++++%d+ ++++++++++++++\n", loop);
		for(i = 0; i < loop; i++)
		{	
			//addr = addr  + 2*i;
			//printk("\n ++++++++++++0x%04x+ ++++++++++++++\n", addr);
		 	if (mix_spi_read(spi, (unsigned short)(addr + 2 * i), data + 2 *i, 2) < 0) {
				printk("dpll-mix spi read failed.!!!!!!!!!!!!!!\n");
	//			return -1;
			}
		}
	}
#if 0
	//ret = mix_spi_read(addr, data, count);
	mutex_unlock(&spidev->buf_lock);	
	if(flag ==1 )	
		kfree(spidev->buffer);
#endif
          spi->chip_select = chip_se; // 0 fpga 1 dpll
          chip_select = chip_se_bak;

	mutex_unlock(&chip_sel_lock);
	
	return ret;
}
EXPORT_SYMBOL(dpll_spi_read);



int fpga_spi_write(unsigned short addr, unsigned char *data, size_t count)
{
	int ret;
	struct spi_device	*spi;
        unsigned char chip_se = 0;
        unsigned short chip_se_bak = 0;

	spin_lock_irq(&spidev->spi_lock);
	spi = spi_dev_get(spidev->spi);
	spin_unlock_irq(&spidev->spi_lock);

	mutex_lock(&chip_sel_lock);

        chip_se = spi->chip_select;
        chip_se_bak = chip_select;

#if 0
	//only a few chance to alloc memory
	if (!spidev->buffer) {
			spidev->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (!spidev->buffer) {
				dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
				return -ENOMEM;
			}
			flag = 1;
		}

	//mutex_lock(&chip_sel_lock);
#endif
	  spi->chip_select = 0; // 0 fpga 1 dpll
//		   spi_setup(spi);
	       chip_select = FPGA_CHIP;

//	mutex_lock(&spidev->buf_lock);	
	
	//printk("now in dpll_write.++++++++++++\n");
	ret = mix_spi_write(spi, addr, data, count);
	
//	mutex_unlock(&spidev->buf_lock);	
          spi->chip_select = chip_se; // 0 fpga 1 dpll
          chip_select = chip_se_bak;
	
	mutex_unlock(&chip_sel_lock);
#if 0	
	if(flag == 1)
     		kfree(spidev->buffer);	
	mutex_unlock(&chip_sel_lock);
#endif	
	 
	return ret;
}
EXPORT_SYMBOL(fpga_spi_write);

int fpga_spi_read(unsigned short addr, unsigned char *data, size_t count)
{	
	int ret = 0;
	unsigned char chip_se = 0;	
	unsigned short chip_se_bak = 0;

	struct spi_device	*spi;
	//printk("now in dpll_read.+1+++++++++++\n");
	spin_lock_irq(&spidev->spi_lock);
	spi = spi_dev_get(spidev->spi);
	spin_unlock_irq(&spidev->spi_lock);
	//printk("now in dpll_read.++++2++++++++\n");

	mutex_lock(&chip_sel_lock);

	chip_se = spi->chip_select;
	chip_se_bak = chip_select;
#if 0
	//only a few chance to alloc memory
	if (!spidev->buffer) {
			spidev->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (!spidev->buffer) {
				dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
				return -ENOMEM;
			}
			flag = 1;
		}
#endif
	//mutex_lock(&chip_sel_lock);
	  spi->chip_select = 0; // 0 fpga 1 dpll
//           spi_setup(spi);
	  chip_select = FPGA_CHIP;
	
    //mutex_lock(&spidev->buf_lock);	
#if 0
	//printk("now in dpll_read.++++++++++++\n");
	{
		int i;
		int loop;
	
		if( (count % 2 ) == 0 )	
		  	loop = count / 2;
		else
			loop = (count + 1) / 2;				
			
		//printk("\n ++++++++++++%d+ ++++++++++++++\n", loop);
		for(i = 0; i < loop; i++)
		{	
			//addr = addr  + 2*i;
			//printk("\n ++++++++++++0x%04x+ ++++++++++++++\n", addr);
		 	if (mix_spi_read(spi, (unsigned short)(addr + 2 * i), data + 2 *i, 2) < 0) {
				printk("fpga-mix spi read failed.!!!!!!!!!!!!\n");
				return -1;
			}
		}
	}
#else
	mix_spi_read(spi, (unsigned short)addr, data, count);
#endif
//	ret = mix_spi_read(spi, addr, data, count);
//	mutex_unlock(&spidev->buf_lock);	
#if 0
	if(flag ==1 )	
		kfree(spidev->buffer);

#endif
          spi->chip_select = chip_se; // 0 fpga 1 dpll
          chip_select = chip_se_bak;

	mutex_unlock(&chip_sel_lock);
	
	return ret;
}
EXPORT_SYMBOL(fpga_spi_read);

static struct mutex			unitboard_lock;
#if 1
#define UNIT_REG_BASE			0x2000
#define CTRL_STATUS_REG			(UNIT_REG_BASE+0)
#define READ_OVER_FLAG			(UNIT_REG_BASE+0x0002)
#define WRITE_ONCE_REG			(UNIT_REG_BASE+0x0010)
#define READ_ONCE_REG			(UNIT_REG_BASE+0x0020)
#define BUFFER_ADDR_400			(UNIT_REG_BASE+0x0200)
#define WORDSIZE			4
//#define IDTDEBUG

void pdata(unsigned char *pdata, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		printk(" %02x", pdata[i]);
	}
	printk("\n");
}
/*write a short data*/
int unitboard_fpga_write(unsigned char slot, unsigned short addr, unsigned short *wdata)
{
	unsigned char data[32] = {0};

	data[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
	data[1] = addr & 0xff;
	data[2] = (*wdata & 0xff00) >> 8;
	data[3] = *wdata & 0xff;
#ifdef IDTDEBUG
	printk("Write Data:\n");
	pdata(data,4);
#endif	
	fpga_spi_write(WRITE_ONCE_REG, data, WORDSIZE);
	//usleep(1);

	return 0;
}
EXPORT_SYMBOL(unitboard_fpga_write);
#if 0   //as the unitboard buff and cmd reassigned ,disable read opt
/*read a short data*/
int unitboard_fpga_read_one(unsigned char slot, unsigned short addr, unsigned short* wdata)
{
	unsigned char data[32] = {0}, mode = 1;
	unsigned int read_flag = 0;
        unsigned int delay_count = 1000;
	unsigned short bufaddr = 0x400;

	data[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
	data[1] = addr & 0xff;
	data[2] = ((mode & 0x07) << 5) | ((bufaddr&0x1f00) >> 8);
	data[3] = bufaddr & 0xff;
#ifdef IDTDEBUG
	printk("Read data:\n");
	pdata(data,4);
#endif
	mutex_lock(&unitboard_lock);

	fpga_spi_write(READ_ONCE_REG, data, WORDSIZE);

	do{
		fpga_spi_read(READ_OVER_FLAG, &read_flag, WORDSIZE);
		if((read_flag == 0)||(delay_count <1))
			break;
	}while(delay_count--);
	memset(data,0,32);
	fpga_spi_read((UNIT_REG_BASE + ((bufaddr)>>1)), data, 4);
	mutex_unlock(&unitboard_lock);
	//usleep(1);
#ifdef IDTDEBUG
	printk("Return Data:\n");	
	pdata(data,4);
#endif
	if((bufaddr)%2)
		*wdata = data[0]<<8 | data[1];
	else
		*wdata = data[2]<<8 | data[3];
	return 0;
}
EXPORT_SYMBOL(unitboard_fpga_read_one);
/*read 1~128 short data*/
int unitboard_fpga_read(unsigned char slot, unsigned short addr, unsigned short *wdata, size_t count)
{
	unsigned char data[WORDSIZE] = {0}, mode;
	unsigned int read_flag = 0, i = 0;
        unsigned int delay_count = 1000;
	unsigned short bufaddr = 0x400;

#if 0
	if(count == 1)
	{
		mode = 1;
		data[3] = bufaddr & 0xff;
	}
	else 
#endif
	if(count <= 16)
	{
		mode = 2;
		data[3] = (bufaddr & 0xf0) | (count & 0xf);
	}
	else if(count <= 128)
	{
		mode = 3;
		data[3] = (bufaddr & 0x80) | (count & 0x7f);
	}
	else
	{
		printk("read count error\n");
		return 0;
	}
	data[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
	data[1] = addr & 0xff;
	data[2] = ((mode & 0x07) << 5) | ((bufaddr&0x1f00) >> 8);

#ifdef IDTDEBUG
	printk("Read data:\n");
	pdata(data,4);
#endif

	mutex_lock(&unitboard_lock);
	fpga_spi_write(READ_ONCE_REG, data, WORDSIZE);

	do{
		fpga_spi_read(READ_OVER_FLAG, &read_flag, WORDSIZE);
		if((read_flag == 0)||(delay_count <1))
			break;
	}while(delay_count--);
	memset(data,0,WORDSIZE);
	
	for(i = 0; i<(count/2 + count%2); i++)
	{
		fpga_spi_read((UNIT_REG_BASE + ((bufaddr)>>1) + i), data, WORDSIZE);
		*(wdata+i*2) = data[2]<<8 | data[3];
		if(count>=(i+1)*2)
		*(wdata+i*2+1) = data[0]<<8 | data[1];
	}
	mutex_unlock(&unitboard_lock);
	//usleep(1);
#ifdef IDTDEBUG
	printk("Return Data:\n");	
	pdata(wdata,2*count);
#endif
	return 0;
}
EXPORT_SYMBOL(unitboard_fpga_read);
#endif
#endif
typedef struct spi_rdwr_argv
{
	unsigned char 	cs;
	unsigned short 	addr;
	unsigned short 	len;
	unsigned char 	buff[64];
}spi_rdwr;

/*-------------------------------------------------------------------------*/

/* Read-only message with current device setup */
static ssize_t
spidev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	mutex_lock(&chip_sel_lock);
	spi_rdwr sopt;

	struct spidev_data	*spidev;
	struct spi_device 	*spi;//remmerber to chang last
		
	spidev = filp->private_data;
	spi = spidev->spi;	

	copy_from_user(&sopt, buf, sizeof(sopt));
	if(0 == sopt.cs)	//fpga
	{
		spi->chip_select = 0;
		chip_select = FPGA_CHIP;
	}
	else if(1 == sopt.cs)
	{
		spi->chip_select = 1;
                chip_select = DS31400_CHIP;
	}
	else
	{
		printk("read:error cs=%d\n",sopt.cs);
		goto END;
	}
	if (sopt.len > 64) {
		debugk("mix spi write out of range.\n");
		goto END;
	}
	mutex_lock(&spidev->buf_lock);
	{
		mix_spi_read(spi, (unsigned short)sopt.addr, sopt.buff, sopt.len);
	}
	
	copy_to_user(buf, &sopt, sizeof(sopt));
	mutex_unlock(&spidev->buf_lock);

END:
	mutex_unlock(&chip_sel_lock);

	return sopt.len;
}

/* Write-only message with current device setup */
static ssize_t
spidev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	mutex_lock(&chip_sel_lock);
	spi_rdwr sopt;

	struct spidev_data	*spidev;
	struct spi_device 	*spi;//remmerber to chang last
		
	spidev = filp->private_data;
	spi = spidev->spi;		

	copy_from_user(&sopt, buf, sizeof(sopt));
	
	if(0 == sopt.cs)	//fpga
	{
		spi->chip_select = 0;
		chip_select = FPGA_CHIP;
	}
	else if(1 == sopt.cs)
	{
		spi->chip_select = 1;
                chip_select = DS31400_CHIP;
	}
	else
	{
		printk("read:error cs=%d\n",sopt.cs);
		goto END;
	}
	if (sopt.len > 64) {
		debugk("mix spi write out of range.\n");
		goto END;
	}



	mutex_lock(&spidev->buf_lock);	

	if (mix_spi_write(spi,(unsigned short)sopt.addr, sopt.buff, sopt.len) < 0) {
		debugk("mix spi write failed.\n");
	}

	mutex_unlock(&spidev->buf_lock);
END:
	mutex_unlock(&chip_sel_lock);

	return sopt.len;
}

static int spidev_message(struct spidev_data *spidev,
		struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
	struct spi_message	msg;
	struct spi_transfer	*k_xfers;
	struct spi_transfer	*k_tmp;
	struct spi_ioc_transfer *u_tmp;
	unsigned		n, total;
	u8			*buf;
	int			status = -EFAULT;

	spi_message_init(&msg);
	k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
	if (k_xfers == NULL)
		return -ENOMEM;

	/* Construct spi_message, copying any tx data to bounce buffer.
	 * We walk the array of user-provided transfers, using each one
	 * to initialize a kernel version of the same transfer.
	 */
	buf = spidev->buffer;
	total = 0;
	for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
			n;
			n--, k_tmp++, u_tmp++) {
		k_tmp->len = u_tmp->len;

		total += k_tmp->len;
		if (total > bufsiz) {
			status = -EMSGSIZE;
			goto done;
		}

		if (u_tmp->rx_buf) {
			k_tmp->rx_buf = buf;
			if (!access_ok(VERIFY_WRITE, (u8 __user *)
						(uintptr_t) u_tmp->rx_buf,
						u_tmp->len))
				goto done;
		}
		if (u_tmp->tx_buf) {
			k_tmp->tx_buf = buf;
			if (copy_from_user(buf, (const u8 __user *)
						(uintptr_t) u_tmp->tx_buf,
					u_tmp->len))
				goto done;
		}

#if 0		
	{

		int i;
		for(i = 0; i <u_tmp->len; i ++)
		{
			printk(" i =  %d ,k_tem.tx_buf[i]  =  0x%04x ", i, buf[i]  );
		}
		printk("\n  ok");
	}
#endif
		buf += k_tmp->len;

		k_tmp->cs_change = !!u_tmp->cs_change;
		k_tmp->bits_per_word = u_tmp->bits_per_word;
		k_tmp->delay_usecs = u_tmp->delay_usecs;
		k_tmp->speed_hz = u_tmp->speed_hz;
#ifdef VERBOSE
		dev_dbg(&spidev->spi->dev,
			"  xfer len %zd %s%s%s%dbits %u usec %uHz\n",
			u_tmp->len,
			u_tmp->rx_buf ? "rx " : "",
			u_tmp->tx_buf ? "tx " : "",
			u_tmp->cs_change ? "cs " : "",
			u_tmp->bits_per_word ? : spidev->spi->bits_per_word,
			u_tmp->delay_usecs,
			u_tmp->speed_hz ? : spidev->spi->max_speed_hz);
#endif
#if 0
		dev_printk(KERN_INFO, &spidev->spi->dev,
			"  xfer len %zd %s%s%s%dbits %u usec %uHz\n",
			k_tmp->len,
			k_tmp->rx_buf ? "rx " : "",
			k_tmp->tx_buf ? "tx " : "",
			k_tmp->cs_change ? "cs " : "",
			k_tmp->bits_per_word ? : spidev->spi->bits_per_word,
			k_tmp->delay_usecs,
			k_tmp->speed_hz ? : spidev->spi->max_speed_hz);
#endif
		spi_message_add_tail(k_tmp, &msg);
	}

	status = spidev_sync(spidev, &msg);
	if (status < 0)
		goto done;

	/* copy any rx data out of bounce buffer */
	buf = spidev->buffer;
	for (n = n_xfers, u_tmp = u_xfers; n; n--, u_tmp++) {
		if (u_tmp->rx_buf) {
			if (__copy_to_user((u8 __user *)
					(uintptr_t) u_tmp->rx_buf, buf,
					u_tmp->len)) {
				status = -EFAULT;
				goto done;
			}
		}
		buf += u_tmp->len;
	}
	status = total;
{
#if 0
	unsigned char data[2]={0, 0}; 
	mix_spi_read1(spidev, 0x00,  data, 2);
#endif
}
done:
	kfree(k_xfers);
	return status;
}

static long
spidev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int			err = 0;
	int			retval = 0;
	struct spidev_data	*spidev;
	struct spi_device	*spi;
	u32			tmp;
	unsigned		n_ioc;
	struct spi_ioc_transfer	*ioc;

	w25_rw_date_t  w25p16_date;
	size_t retlen = 0;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
		return -ENOTTY;

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	spidev = filp->private_data;
	spin_lock_irq(&spidev->spi_lock);
	spi = spi_dev_get(spidev->spi);
	spin_unlock_irq(&spidev->spi_lock);

	if (spi == NULL)
		return -ESHUTDOWN;

	/* use the buffer lock here for triple duty:
	 *  - prevent I/O (from us) so calling spi_setup() is safe;
	 *  - prevent concurrent SPI_IOC_WR_* from morphing
	 *    data fields while SPI_IOC_RD_* reads them;
	 *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
	 */
	//mutex_lock(&spidev->buf_lock);

	switch (cmd) {
	/* read requests */
	case SPI_IOC_RD_MODE:
		retval = __put_user(spi->mode & SPI_MODE_MASK,
					(__u8 __user *)arg);
		break;
	case SPI_IOC_RD_LSB_FIRST:
		retval = __put_user((spi->mode & SPI_LSB_FIRST) ?  1 : 0,
					(__u8 __user *)arg);
		break;
	case SPI_IOC_RD_BITS_PER_WORD:
		retval = __put_user(spi->bits_per_word, (__u8 __user *)arg);
		break;
	case SPI_IOC_RD_MAX_SPEED_HZ:
		retval = __put_user(spi->max_speed_hz, (__u32 __user *)arg);
		break;

	/* write requests */
	case SPI_IOC_WR_MODE:
		retval = __get_user(tmp, (u8 __user *)arg);
		if (retval == 0) {
			u8	save = spi->mode;

			if (tmp & ~SPI_MODE_MASK) {
				retval = -EINVAL;
				break;
			}

			tmp |= spi->mode & ~SPI_MODE_MASK;
			spi->mode = (u8)tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "spi mode %02x\n", tmp);
		}
		break;
	case SPI_IOC_WR_LSB_FIRST:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u8	save = spi->mode;

			if (tmp)
				spi->mode |= SPI_LSB_FIRST;
			else
				spi->mode &= ~SPI_LSB_FIRST;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "%csb first\n",
						tmp ? 'l' : 'm');
		}
		break;
	case SPI_IOC_WR_BITS_PER_WORD:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u8	save = spi->bits_per_word;

			spi->bits_per_word = tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->bits_per_word = save;
			else
				dev_dbg(&spi->dev, "%d bits per word\n", tmp);
		}
		break;
	case SPI_IOC_WR_MAX_SPEED_HZ:
		retval = __get_user(tmp, (__u32 __user *)arg);
		if (retval == 0) {
			u32	save = spi->max_speed_hz;

			spi->max_speed_hz = tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->max_speed_hz = save;
			else
				dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
		}
		break;
#if 0
	//new add
	case SPI_IOC_OPER_FPGA:
	       	spi->chip_select = 0; // 0 fpga 1 dpll
	       	chip_select = FPGA_CHIP;
		retval = 0;
	       break;
	case SPI_IOC_OPER_FPGA_DONE:
		retval = 0;
		break;
	case SPI_IOC_OPER_DPLL:
//	       	mutex_lock(&chip_sel_lock);
	       	spi->chip_select = 1; // 0 fpga 1 dpll
	       	chip_select = DS31400_CHIP;
	       	retval = 0;
	       	break;
	case SPI_IOC_OPER_DPLL_DONE:
		spi->chip_select = 0; // 0 fpga 1 dpll
	    	chip_select = FPGA_CHIP;
//		mutex_unlock(&chip_sel_lock);
		retval=0;
		break;
#endif
        case SPI_IOC_OPER_FLASH:
                mutex_lock(&chip_sel_lock);
                spi->chip_select = 2;
                spi_setup(spi);
                gpio_direction_output(GPIO_FPGAFLASH,0);
                chip_select = FLASH_FPGA;
                retval = 0;
                break;
        case SPI_IOC_OPER_FLASH_DONE:
                spi->chip_select = 0;
                spi_setup(spi);
                chip_select = FPGA_CHIP;
                gpio_direction_output(GPIO_FPGAFLASH, 1);
                mutex_unlock(&chip_sel_lock);
                retval=0;
                break;

	case W25_ERASE_CHIP:
		//printk("\nnow erase chip\n");
		retval = erase_chip(&flash);
		break;
	case W25_ERASE_SECTOR:
		retval = __get_user(tmp, (__u32 __user *)arg);
		if(retval == 0)
		  {	
			//printk("\n ++++tmp = 0x%08x+++++\n", tmp); 
			retval = erase_sector(&flash, tmp); 
		  }
		break;

	case W25P16_READ:
		retval =  copy_from_user(&w25p16_date, (w25_rw_date_t *)arg, sizeof(w25_rw_date_t));
		if(retval != 0)
			break;	
		//printk("\nnow read chip:w25p16_date.addr = 0x%08x, w25p16_date.len = 0x%08x \n", (u32)w25p16_date.addr,w25p16_date.len);	    
		retval  =  w25p16_read(&flash , w25p16_date.addr, w25p16_date.len, &retlen, w25p16_date.buf);
		#if 0
			ret =  w25p16_read(&flash , 0x100000, 0xff, &retlen, buf_t);
        		if(ret)
	   			printk("\n+++++++++\erase error ret = %d ++++\n", ret);
                #endif
		#if 0
		   {
		   	int i = 0;
			for(i = 0; i < 0x10; i++)
	 		{
				printk(" buf[%d ] = 0x%02x  ",i,   w25p16_date.buf[i]);
				if(i % 8 == 0)
				printk("\n");
         		 }

		   }
		#endif
		if(retval == 0)
		{
			retval = copy_to_user((w25_rw_date_t *)arg, &w25p16_date, sizeof(w25_rw_date_t));
		}
                msleep(1);      /*this is needed,else cost a long time 2015-8-20 zhangjj */

		break;

	case W25P16_WRITE:
		retval =  copy_from_user(&w25p16_date, (w25_rw_date_t *)arg, sizeof(w25_rw_date_t));
		if(retval != 0)
			break;	
		retval  =  w25p16_write(&flash , w25p16_date.addr, w25p16_date.len, &retlen, w25p16_date.buf);
		if(retval == 0)
		{
			retval = copy_to_user((w25_rw_date_t *)arg, &w25p16_date, sizeof(w25_rw_date_t));
		}
                msleep(1);      /*this is needed,else cost a long time 2015-8-20 zhangjj */

		break;
	case W25P1165_ID:
		 spi->chip_select = 2; // 0 fpga 1 dpll
		 spi_setup(spi);
		 w25p16_read_id(spi);
		// w25p16_read_test(spi);

	default:
		/* segmented and/or full-duplex I/O request */
		if (_IOC_NR(cmd) != _IOC_NR(SPI_IOC_MESSAGE(0))
				|| _IOC_DIR(cmd) != _IOC_WRITE) {
			retval = -ENOTTY;
			break;
		}

		tmp = _IOC_SIZE(cmd);
		if ((tmp % sizeof(struct spi_ioc_transfer)) != 0) {
			retval = -EINVAL;
			break;
		}
		n_ioc = tmp / sizeof(struct spi_ioc_transfer);
		if (n_ioc == 0)
			break;

		/* copy into scratch area */
		ioc = kmalloc(tmp, GFP_KERNEL);
		if (!ioc) {
			retval = -ENOMEM;
			break;
		}
		if (__copy_from_user(ioc, (void __user *)arg, tmp)) {
			kfree(ioc);
			retval = -EFAULT;
			break;
		}

		/* translate to spi_message, execute */
		retval = spidev_message(spidev, ioc, n_ioc);
		kfree(ioc);
		break;
	}

	//mutex_unlock(&spidev->buf_lock);
	spi_dev_put(spi);
	return retval;
}

static int spidev_open(struct inode *inode, struct file *filp)
{
	struct spidev_data	*spidev;
	int			status = -ENXIO;

	mutex_lock(&device_list_lock);

	list_for_each_entry(spidev, &device_list, device_entry) {
		if (spidev->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}
	if (status == 0) {
		if (!spidev->buffer) {
			spidev->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (!spidev->buffer) {
				dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
				status = -ENOMEM;
			}
		}
		if (status == 0) {
			spidev->users++;
			filp->private_data = spidev;
			//nonseekable_open(inode, filp);
		}
	} else
		pr_debug("spidev: nothing for minor %d\n", iminor(inode));

	mutex_unlock(&device_list_lock);
	return status;
}

static int spidev_release(struct inode *inode, struct file *filp)
{
	struct spidev_data	*spidev;
	int			status = 0;

	mutex_lock(&device_list_lock);
	spidev = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	spidev->users--;
	if (!spidev->users) {
		int		dofree;

		kfree(spidev->buffer);
		spidev->buffer = NULL;

		/* ... after we unbound from the underlying device? */
		spin_lock_irq(&spidev->spi_lock);
		dofree = (spidev->spi == NULL);
		spin_unlock_irq(&spidev->spi_lock);

		if (dofree)
			kfree(spidev);
	}
	mutex_unlock(&device_list_lock);

	return status;
}

static loff_t spidev__llseek(struct file *file, loff_t offset, int origin)
{
	//printk("\n ++++++++++++++++spidev__llseek++++++++++++++++++++++.\n");

	if (offset < 0 || offset > DS31400_REG_ADDR_MAX)
		return (off_t)-1;

	file->f_pos = offset;
	debugk("mix spi fpos move to 0x%04x\n", (unsigned int)file->f_pos);
	
	return file->f_pos;

	//return 0;
}

static const struct file_operations spidev_fops = {
	.owner =	THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.  It'll simplify things
	 * too, except for the locking.
	 */
	.write =	spidev_write,
	.read =		spidev_read,
	.unlocked_ioctl = spidev_ioctl,
	.open =		spidev_open,
	.release =	spidev_release,
	.llseek	      = spidev__llseek,
};

/*-------------------------------------------------------------------------*/

/* The main reason to have this class is to make mdev/udev create the
 * /dev/spidevB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */

static struct class *spidev_class;

/*-------------------------------------------------------------------------*/

static int __devinit spidev_probe(struct spi_device *spi)
{
	//struct spidev_data	*spidev;
	int			status;
	unsigned long		minor;

	mutex_init(&chip_sel_lock); 
	mutex_init(&unitboard_lock); 
	
	/* Allocate driver data */
	spidev = kzalloc(sizeof(*spidev), GFP_KERNEL);
	if (!spidev)
		return -ENOMEM;

	/* Initialize the driver data */
	spidev->spi = spi;
	spin_lock_init(&spidev->spi_lock);
	mutex_init(&spidev->buf_lock);

	INIT_LIST_HEAD(&spidev->device_entry);

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		spidev->devt = MKDEV(SPIDEV_MAJOR, minor);
		//dev = device_create(spidev_class, &spi->dev, spidev->devt,
		//		    spidev, "spidev%d.%d",
		//		    spi->master->bus_num, spi->chip_select);
		dev = device_create(spidev_class, &spi->dev, spidev->devt,
				    spidev, "spidev0.0");
		//printk("\n *****************now probe************\n");
		status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	} else {
		dev_dbg(&spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}
	if (status == 0) {
		set_bit(minor, minors);
		list_add(&spidev->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);

	if (status == 0)
		spi_set_drvdata(spi, spidev);
	else
		kfree(spidev);

	{
		unsigned char data[2]={0, 0};//used to test suxq fun
	
		spi->max_speed_hz = 6500000;	//the real rate 6.25M
		spi->chip_select =  2;// 0 fpga 1 dpll
		spi->mode =  SPI_MODE_3;
		spi_setup(spi);
//		dpll_spi_read(0x00, data, 2);

//		spidev_message(spidev,  NULL, 0);
	//	dpll_spi_read(0x00, data, 2 );
	//	printk(" data[0] = %2x, data[1] = %2x ++++++++++++\n", data[0], data[1]);

		//fpga_spi_read(0x00, data, 2 );
		//printk(" data[0] = %2x, data[1] = %2x ++++++++++++\n", data[0], data[1]);

		//data[0] = 0x25;
		//data[1] = 0x25;
		//fpga_spi_write(0x32 ,data, 2);

	//	fpga_spi_read(0x32, data, 2 );
		//printk(" data[0] = %2x, data[1] = %2x ++++++++++++\n", data[0], data[1]);
		w25p16_read_id(spi);
	}

	flash.spi = spi;
	flash.mtd.size = 0x7fffff;
	mutex_init(&(flash.lock));

	return status;
}

static int __devexit spidev_remove(struct spi_device *spi)
{
	struct spidev_data	*spidev = spi_get_drvdata(spi);

	/* make sure ops on existing fds can abort cleanly */
	spin_lock_irq(&spidev->spi_lock);
	spidev->spi = NULL;
	spi_set_drvdata(spi, NULL);
	spin_unlock_irq(&spidev->spi_lock);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	list_del(&spidev->device_entry);
	device_destroy(spidev_class, spidev->devt);
	clear_bit(MINOR(spidev->devt), minors);
	if (spidev->users == 0)
		kfree(spidev);
	mutex_unlock(&device_list_lock);

	return 0;
}

static struct spi_driver spidev_spi_driver = {
	.driver = {
		.name =		"fsl,spi-ds31400",
		.owner =	THIS_MODULE,
	},
	.probe =	spidev_probe,
	.remove =	__devexit_p(spidev_remove),

	/* NOTE:  suspend/resume methods are not necessary here.
	 * We don't do anything except pass the requests to/from
	 * the underlying controller.  The refrigerator handles
	 * most issues; the controller driver handles the rest.
	 */
};

/*-------------------------------------------------------------------------*/

static int __init spidev_init(void)
{
	int status;

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(SPIDEV_MAJOR, "spi", &spidev_fops);
	if (status < 0)
		return status;

	spidev_class = class_create(THIS_MODULE, "spidev");
	if (IS_ERR(spidev_class)) {
		unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
		return PTR_ERR(spidev_class);
	}

	status = spi_register_driver(&spidev_spi_driver);
	if (status < 0) {
		class_destroy(spidev_class);
		unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
	}
	return status;
}
#if 0
module_init(spidev_init);

static void __exit spidev_exit(void)
{
	spi_unregister_driver(&spidev_spi_driver);
	class_destroy(spidev_class);
	unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
}
module_exit(spidev_exit);

MODULE_AUTHOR("Andrea Paterniani, <a.paterniani@swapp-eng.it>");
MODULE_DESCRIPTION("User mode SPI device interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:spidev");
#endif

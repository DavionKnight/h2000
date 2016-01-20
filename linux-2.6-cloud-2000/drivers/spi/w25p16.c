/*
 * MTD SPI driver for ST M25Pxx (and similar) serial flash chips
 *
 * Author: Mike Lavender, mike@steroidmicros.com
 *
 * Copyright (c) 2005, Intec Automation Inc.
 *
 * Some parts are based on lart.c by Abraham Van Der Merwe
 *
 * Cleaned up and generalized based on mtd_dataflash.c
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/math64.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include "w25p16.h"
#include <linux/delay.h>
/****************************************************************************/



static inline struct w25p *mtd_to_w25p(struct mtd_info *mtd)
{
	return container_of(mtd, struct w25p, mtd);
}

/****************************************************************************/

/*
 * Internal helper functions
 */

/*
 * Read the status register, returning its value in the location
 * Return the status register value.
 * Returns negative if error occurred.
 */
static int read_sr(struct w25p *flash)
{
	ssize_t retval;
	u8 code = OPCODE_RDSR;
	u8 val;

	//retval = spi_write_then_read(flash->spi, &code, 1, &val, 1);

	{
		int ret;
		struct spi_message message;
		struct spi_transfer xfer;

		unsigned short address = 0;
		unsigned char buf[14] = {0};
		unsigned char rx_buf[14] = {0};

	
		buf[0] = code;
	
		

		/* Build our spi message */
		spi_message_init(&message);
		memset(&xfer, 0, sizeof(xfer));
		xfer.len = 2; 
		//xfer.len = count ;
		/* Can tx_buf and rx_buf be equal? The doc in spi.h is not sure... */
		xfer.tx_buf = buf;
		xfer.rx_buf = rx_buf;

		spi_message_add_tail(&xfer, &message);

		ret = spi_sync(flash->spi, &message);


		if (ret < 0) {
		dev_err(&flash->spi->dev, "error %d reading SR\n",
				(int) ret);
		return ret;
	        }

	//	printk("\n   rx_buf[1]0x%08x,  0x%08x \n", rx_buf[1], rx_buf[0]);
		return rx_buf[1];
	}

	if (retval < 0) {
		dev_err(&flash->spi->dev, "error %d reading SR\n",
				(int) retval);
		return retval;
	}

	return val;
}

/*
 * Write status register 1 byte
 * Returns negative if error occurred.
 */
static int write_sr(struct w25p *flash, u8 val)
{
	flash->command[0] = OPCODE_WRSR;
	flash->command[1] = val;

	return spi_write(flash->spi, flash->command, 2);
}

/*
 * Set write enable latch with Write Enable command.
 * Returns negative if error occurred.
 */
static inline int write_enable(struct w25p *flash)
{
	u8	code = OPCODE_WREN;

	//return spi_write_then_read(flash->spi, &code, 1, NULL, 0);
	return spi_write(flash->spi, &code, 1);
}


/*
 * Service routine to read status register until ready, or timeout occurs.
 * Returns non-zero if error.
 */
static int wait_till_ready(struct w25p *flash)
{
	int count;
	int sr;

	/* one chip guarantees max 5 msec wait here after page writes,
	 * but potentially three seconds (!) after page erase.
	 */
	for (count = 0; count < MAX_READY_WAIT_COUNT; count++) {
		if ((sr = read_sr(flash)) < 0)
			break;
		else if (!(sr & SR_WIP))
			return 0;
		msleep(1000);

		/* REVISIT sometimes sleeping would be best */
	}

	return 1;
}

/*
 * Erase the whole flash memory
 *
 * Returns 0 if successful, non-zero otherwise.
 */
int erase_chip(struct w25p *flash)
{
	DEBUG(MTD_DEBUG_LEVEL3, "%s: %s %lldKiB\n",
	      dev_name(&flash->spi->dev), __func__,
	      (long long)(flash->mtd.size >> 10));

	/* Wait until finished previous write command. */
	if (wait_till_ready(flash))
		return 1;
	
	/* Send write enable, then erase commands. */
	write_enable(flash);

	/* Set up command buffer. */
	flash->command[0] = OPCODE_CHIP_ERASE;

	spi_write(flash->spi, flash->command, 1);

	return 0;
}

/*
 * Erase one sector of flash memory at offset ``offset'' which is any
 * address within the sector which should be erased.
 *
 * Returns 0 if successful, non-zero otherwise.
 */
int erase_sector(struct w25p *flash, u32 offset)
{
	DEBUG(MTD_DEBUG_LEVEL3, "%s: %s %dKiB at 0x%08x\n",
			dev_name(&flash->spi->dev), __func__,
			flash->mtd.erasesize / 1024, offset);

	/* Wait until finished previous write command. */
	if (wait_till_ready(flash))
		return 1;

	/* Send write enable, then erase commands. */
	write_enable(flash);

	/* Set up command buffer. */
	flash->command[0] = OPCODE_SE;
	flash->command[1] = offset >> 16;
	flash->command[2] = offset >> 8;
	flash->command[3] = offset;

	spi_write(flash->spi, flash->command, CMD_SIZE);

	return 0;
}

/****************************************************************************/

/*
 * MTD implementation
 */

/*
 * Erase an address range on the flash chip.  The address range may extend
 * one or more erase sectors.  Return an error is there is a problem erasing.
 */
static int w25p16_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct w25p *flash = mtd_to_w25p(mtd);
	u32 addr,len;
	uint32_t rem;

	DEBUG(MTD_DEBUG_LEVEL2, "%s: %s %s 0x%llx, len %lld\n",
	      dev_name(&flash->spi->dev), __func__, "at",
	      (long long)instr->addr, (long long)instr->len);

	/* sanity checks */
	if (instr->addr + instr->len > flash->mtd.size)
		return -EINVAL;
	div_u64_rem(instr->len, mtd->erasesize, &rem);
	if (rem)
		return -EINVAL;

	addr = instr->addr;
	len = instr->len;

	mutex_lock(&flash->lock);

	/* whole-chip erase? */
	if (len == flash->mtd.size && erase_chip(flash)) {
		instr->state = MTD_ERASE_FAILED;
		mutex_unlock(&flash->lock);
		return -EIO;

	/* REVISIT in some cases we could speed up erasing large regions
	 * by using OPCODE_SE instead of OPCODE_BE_4K.  We may have set up
	 * to use "small sector erase", but that's not always optimal.
	 */

	/* "sector"-at-a-time erase */
	} else {
		while (len) {
			if (erase_sector(flash, addr)) {
				instr->state = MTD_ERASE_FAILED;
				mutex_unlock(&flash->lock);
				return -EIO;
			}

			addr += mtd->erasesize;
			len -= mtd->erasesize;
		}
	}

	mutex_unlock(&flash->lock);

	instr->state = MTD_ERASE_DONE;
	mtd_erase_callback(instr);

	return 0;
}

/*
 * Read an address range from the flash chip.  The address range
 * may be any size provided it is within the physical boundaries.
 */
int w25p16_read(struct w25p *flash , loff_t from, size_t len,
	size_t *retlen, u_char *buf)
{
	struct spi_transfer t[2];
	struct spi_message m;

	 u_char rx_buf[256];
	 u_char tx_buf[256];

	DEBUG(MTD_DEBUG_LEVEL2, "%s: %s %s 0x%08x, len %zd\n",
			dev_name(&flash->spi->dev), __func__, "from",
			(u32)from, len);

	/* sanity checks */
	if (!len)
		return 0;

	if (from + len > flash->mtd.size)
		return -EINVAL;

	spi_message_init(&m);
	memset(t, 0, (sizeof t));

	/* NOTE:
	 * OPCODE_FAST_READ (if available) is faster.
	 * Should add 1 byte DUMMY_BYTE.
	 */

	tx_buf[0] = OPCODE_READ;
	tx_buf[1] = from >> 16;
	tx_buf[2] = from >> 8;
	tx_buf[3] = from;
	t[0].tx_buf =tx_buf;
	t[0].len = CMD_SIZE + FAST_READ_DUMMY_BYTE + len;
	t[0].rx_buf = rx_buf;
	
	spi_message_add_tail(&t[0], &m);

//	t[1].rx_buf = buf;
//	t[1].len = len;
//	spi_message_add_tail(&t[1], &m);

	/* Byte count starts at zero. */
	if (retlen)
		*retlen = 0;

	mutex_lock(&flash->lock);

	/* Wait till previous write/erase is done. */
	if (wait_till_ready(flash)) {
		/* REVISIT status return?? */
		mutex_unlock(&flash->lock);
		return 1;
	}

	/* FIXME switch to OPCODE_FAST_READ.  It's required for higher
	 * clocks; and at this writing, every chip this driver handles
	 * supports that opcode.
	 */

	/* Set up the write data buffer. */
	flash->command[0] = OPCODE_READ;
	flash->command[1] = from >> 16;
	flash->command[2] = from >> 8;
	flash->command[3] = from;

	spi_sync(flash->spi, &m);

	*retlen = m.actual_length - CMD_SIZE - FAST_READ_DUMMY_BYTE;
	
	memcpy(buf, rx_buf + 4, len);
	

	mutex_unlock(&flash->lock);

	return 0;
}

/*
 * Write an address range to the flash chip.  Data must be written in
 * FLASH_PAGESIZE chunks.  The address range may be any size provided
 * it is within the physical boundaries.
 */
int w25p16_write(struct w25p *flash , loff_t to, size_t len,
	size_t *retlen, const u_char *buf)
{
	u32 page_offset, page_size;
	struct spi_transfer t[2];
	struct spi_message m;

	unsigned char tx_buf[256] = {0};
	//unsigned char rx_buf[256] = {0};
	t[0].tx_buf  = tx_buf;
	

	DEBUG(MTD_DEBUG_LEVEL2, "%s: %s %s 0x%08x, len %zd\n",
			dev_name(&flash->spi->dev), __func__, "to",
			(u32)to, len);

	if (retlen)
		*retlen = 0;

	/* sanity checks */
	if (!len)
		return(0);

	if (to + len > flash->mtd.size)
		return -EINVAL;

	spi_message_init(&m);
	memset(t, 0, (sizeof t));

	t[0].tx_buf = tx_buf;
	t[0].len = CMD_SIZE;
	spi_message_add_tail(&t[0], &m);

	//t[1].tx_buf = buf;
	//spi_message_add_tail(&t[1], &m);

	mutex_lock(&flash->lock);

	/* Wait until finished previous write command. */
	if (wait_till_ready(flash)) {
		mutex_unlock(&flash->lock);
		return 1;
	}

	write_enable(flash);

	/* Set up the opcode in the write buffer. */
	tx_buf [0] = OPCODE_PP;
	tx_buf [1] = to >> 16;
	tx_buf [2] = to >> 8;
	tx_buf [3] = to;

	/* what page do we start with? */
	page_offset = to % FLASH_PAGESIZE;

	/* do all the bytes fit onto one page? */
	if (page_offset + len <= FLASH_PAGESIZE) {
		//t[1].len = len;
	//	t[0].tx_buf + 4  = buf;
		t[0].len = CMD_SIZE + len ;
		memcpy(tx_buf + 4 , buf, len);

		spi_sync(flash->spi, &m);

		*retlen = m.actual_length - CMD_SIZE;
	} else {
		u32 i;
		
		/* the size of data remaining on the first page */
		page_size = FLASH_PAGESIZE - page_offset;

		t[1].len = page_size;
		spi_sync(flash->spi, &m);

		*retlen = m.actual_length - CMD_SIZE;

		/* write everything in PAGESIZE chunks */
		for (i = page_size; i < len; i += page_size) {
			page_size = len - i;
			if (page_size > FLASH_PAGESIZE)
				page_size = FLASH_PAGESIZE;

			/* write the next page to flash */
			flash->command[1] = (to + i) >> 16;
			flash->command[2] = (to + i) >> 8;
			flash->command[3] = (to + i);

			t[1].tx_buf = buf + i;
			t[1].len = page_size;

			wait_till_ready(flash);

			write_enable(flash);

			spi_sync(flash->spi, &m);

			if (retlen)
				*retlen += m.actual_length - CMD_SIZE;
		}
	}

	mutex_unlock(&flash->lock);

	return 0;
}
void w25p16_read_id(struct spi_device *spi)
{

	int ret;
	struct spi_message message;
	struct spi_transfer xfer;

	unsigned short address = 0;
	unsigned char buf[14] = {0};
	unsigned char rx_buf[14] = {0};

	
	buf[0] = 0x90;
	buf[1] = 0x00;
	buf[2]=0x00;
	buf[3]=0x00;
	
		

		/* Build our spi message */
		spi_message_init(&message);
		memset(&xfer, 0, sizeof(xfer));
		xfer.len = 6; 
		//xfer.len = count ;
		/* Can tx_buf and rx_buf be equal? The doc in spi.h is not sure... */
		xfer.tx_buf = buf;
		xfer.rx_buf = rx_buf;

		spi_message_add_tail(&xfer, &message);

		ret = spi_sync(spi, &message);
     {
		int i;
		
		for (i = 0; i < xfer.len; i++) {
			printk(" %02x", rx_buf[i]);
		}
		printk("\n");
    }	


	//printk("now test begin \n");
	
}

void w25p16_read_test(struct spi_device *spi)
{

	int ret;
	struct spi_message message;
	struct spi_transfer xfer;

	unsigned short address = 0;
	unsigned char buf[14] = {0};
	unsigned char rx_buf[14] = {0};

	
	buf[0] = 0x30;
	buf[1] = 0x00;
	buf[2]=0x10;
	buf[3]=0x00;
	
		

		/* Build our spi message */
		spi_message_init(&message);
		memset(&xfer, 0, sizeof(xfer));
		xfer.len = 6; 
		//xfer.len = count ;
		/* Can tx_buf and rx_buf be equal? The doc in spi.h is not sure... */
		xfer.tx_buf = buf;
		xfer.rx_buf = rx_buf;

		spi_message_add_tail(&xfer, &message);

		ret = spi_sync(spi, &message);
     {
		int i;
		
		for (i = 0; i < xfer.len; i++) {
			printk(" %02x", rx_buf[i]);
		}
		printk("\n");
    }	


	//printk("now test begin \n");
	
}


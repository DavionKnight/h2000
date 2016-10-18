/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:Kevin_fzs
*  File Name        	:/home/kevin/works/projects/H20PN-2000/drivers/spidrv/spi.c
*  Create Date        	:2016/09/23 02:10
*  Last Modified      	:2016/09/23 02:10
*  Description    	:
*/
#include <stdio.h>
#include <stddef.h>

#include "spi.h"

/* SPI Controller mode register definitions */
#define CSMODE_CI_INACTIVEHIGH	(1 << 31)
#define CSMODE_CP_BEGIN_EDGECLK	(1 << 30)
#define CSMODE_REV		(1 << 29)
#define CSMODE_DIV16		(1 << 28)
#define CSMODE_PM(x)		((x) << 24)
#define CSMODE_POL_1		(1 << 20)
#define CSMODE_LEN(x)		((x) << 16)
#define SPMODE_ENABLE		(1 << 31)

/* SPIM register values */
#define SPIM_NE		0x00000200	/* Not empty */
#define SPIM_NF		0x00000100	/* Not full */

#define SPI_EV_NE	(0x80000000 >> 22)	/* Receiver Not Empty */
#define SPI_EV_NF	(0x80000000 >> 23)	/* Transmitter Not Full */

#define SPI_MODE_LOOP	(0x80000000 >> 1)	/* Loopback mode */
#define SPI_MODE_REV	(0x80000000 >> 5)	/* Reverse mode - MSB first */
#define SPI_MODE_MS	(0x80000000 >> 6)	/* Always master */
#define SPI_MODE_EN	(0x80000000 >> 7)	/* Enable interface */

#define SPI_TIMEOUT		1000

#define CSMODE_PM_MAX		(0xF)
#define CSMODE_PM_MIN		(0x2)

static void spi_reg_read(const volatile unsigned *addr, unsigned int *val)
{
	__asm__ __volatile__("lwz%U1%X1 %0,%1; twi 0,%0,0; isync"
			     : "=r" (*val) : "m" (*addr));
}

static void spi_reg_write(volatile unsigned *addr, unsigned int val)
{
	__asm__ __volatile__("stw%U0%X0 %1,%0; sync"
			     : "=m" (*addr) : "r" (val));
}

int spi_transfer(struct spi_device *spidev, unsigned char *txbuf, unsigned char *rxbuf, int len)
{
	unsigned int txword = 0, rxword	 = 0;
	unsigned int event = 0, command = 0;
	int isRead = 0, tm = 0, i = 0;

	command = (spidev->chip_select<<30)|(len - 1);
	/*enable chipselect and tell spi the length of data*/
	spi_reg_write(&spidev->spi_reg->command, command);

	/*enable rx ints*/
	spi_reg_write(&spidev->spi_reg->mask, SPIM_NE);
#if 0
	int len_count = len;
	do{
		spi_reg_read(&spidev->spi_reg->event, &event);
		if(event & SPI_EV_NF)
		{
			txword = 0;
			txword = *(unsigned int *)txbuf;	
			txbuf += 4;

			spi_reg_write(&spidev->spi_reg->transmit, txword);

			len_count -= 4;
		}
	}while(len_count>0);
usleep(1);
		/*here wait tx data over*/
	len_count = len;
	do{
		for(tm = 0, isRead = 0; tm < SPI_TIMEOUT; tm++)
		{
			spi_reg_read(&spidev->spi_reg->event, &event);
			if (event & SPI_EV_NE) {
				spi_reg_read(&spidev->spi_reg->receive, &rxword);
//				printf("11rxword=0x%08x, event=0x%08x\n",rxword,event);

				*(unsigned int *) rxbuf = rxword;
				rxbuf += 4;

				break;
			}
		}
		if (tm >= SPI_TIMEOUT)
		{
			printf("spi error:Time out when spi transfer,event=0x%08x\n",event);
			return -1;
		}
		len_count = len_count - 4;
	}while(len_count>0);
#else
	do{
		spi_reg_read(&spidev->spi_reg->event, &event);
		if(event & SPI_EV_NF)
		{
			txword = 0;
			txword = *(unsigned int *)txbuf;	
			txbuf += 4;

			spi_reg_write(&spidev->spi_reg->transmit, txword);
		}
		/*here wait tx data over*/
//		usleep(5);
//		printf("txword=0x%08x, len=%d\n",txword, len);
		for(tm = 0, isRead = 0; tm < SPI_TIMEOUT; tm++)
		{
			spi_reg_read(&spidev->spi_reg->event, &event);
			if (event & SPI_EV_NE) {
				/*here wait rx over*/
				for(i = 0; i < SPI_TIMEOUT; i++)
				{
					spi_reg_read(&spidev->spi_reg->event, &event);
					if(((event>>24)&0x3f) >= 4)
						break;
				}
				spi_reg_read(&spidev->spi_reg->receive, &rxword);
//				printf("in transfer rxword=0x%08x event=0x%08x\n",rxword,event);
				isRead = 1;

				*(unsigned int *) rxbuf = rxword;
				rxbuf += 4;
			}
			/*
			 * Only bail when we've had both NE and NF events.
			 * This will cause timeouts on RO devices, so maybe
			 * in the future put an arbitrary delay after writing
			 * the device.  Arbitrary delays suck, though...
			 */
			if (isRead)
				break;
		}
		len -= 4;
	}while(len>0);
	if (tm >= SPI_TIMEOUT)
	{
		printf("spi error:Time out when spi transfer\n");
		return -1;
	}
#endif

	/*disable rx ints*/
	spi_reg_write(&spidev->spi_reg->mask, 0);
//	spi_cs_invalid(spidev->chip_select);

	return 0;
	
}

#define SPIMODE_TXTHR(x)	((x) << 8)
#define SPIMODE_RXTHR(x)	((x) << 0)
#define SPMODE_INIT_VAL (SPIMODE_TXTHR(4) | SPIMODE_RXTHR(3))
#define SPMODE_ENABLE		(1 << 31)

#define CSMODE_POL_1		(1 << 20)
#define CS_BEF(x)		((x) << 12)
#define CS_AFT(x)		((x) << 8)
#define CS_CG(x)		((x) << 3)
#define CSMODE_CI_INACTIVEHIGH	(1 << 31)
/*
 * Default for SPI Mode:
 * 	SPI MODE 0 (inactive low, phase middle, MSB, 8-bit length, slow clk
 */
#define CSMODE_INIT_VAL (CSMODE_POL_1 | CS_BEF(0) | CS_AFT(0) | CS_CG(1))  | CSMODE_CI_INACTIVEHIGH


int spi_dev_init(struct spi_device *spidev)
{
	unsigned int i = 0;

	if(NULL == spidev->spi_reg)
		return -1;
	
	/* SPI controller initializations */
	spi_reg_write(&spidev->spi_reg->mode, 0);
	spi_reg_write(&spidev->spi_reg->mask, 0);
	spi_reg_write(&spidev->spi_reg->command, 0);
	spi_reg_write(&spidev->spi_reg->event, 0xffffffff);

	spi_reg_write(&spidev->spi_reg->mode, SPMODE_INIT_VAL | SPMODE_ENABLE); 
	/* init CS mode interface */
	for (i = 0; i < 4; i++)
		spi_reg_write(&spidev->spi_reg->csmode[i],CSMODE_INIT_VAL);

	return 0;
}

int spi_setup(struct spi_device *spidev)
{
	unsigned int hw_mode;
	unsigned long flags;
	unsigned int regval;
	unsigned char bits_per_word, pm, cs_sel = spidev->chip_select;
	unsigned int hz = 0;
	unsigned int spibrg = 198000000;

	if(!spidev->max_speed_hz)
		return -1;

	if(!spidev->bits_per_word)
		spidev->bits_per_word = 8;

	spi_reg_read(&spidev->spi_reg->csmode[spidev->chip_select], &hw_mode);
	hw_mode &= ~(CSMODE_CP_BEGIN_EDGECLK | CSMODE_CI_INACTIVEHIGH | CSMODE_REV); 

	if (spidev->mode & SPI_CPHA)
		hw_mode |= CSMODE_CP_BEGIN_EDGECLK;
	if (spidev->mode & SPI_CPOL)
		hw_mode |= CSMODE_CI_INACTIVEHIGH;
	if (!(spidev->mode & SPI_LSB_FIRST))
		hw_mode |= CSMODE_REV;

	if (!hz)
		hz = spidev->max_speed_hz;

	/* mask out bits we are going to set */
	hw_mode &= ~(CSMODE_LEN(0xF) | CSMODE_DIV16 | CSMODE_PM(0xF));

	//hw_mode |= CSMODE_LEN(bits_per_word) | CSMODE_INIT_VAL;
	hw_mode |= CSMODE_LEN(spidev->bits_per_word-1); 

	if ((spibrg / hz) > 32) {
		hw_mode |= CSMODE_DIV16;
		pm = spibrg / (hz * 32);
		if (pm > CSMODE_PM_MAX) {
			pm = CSMODE_PM_MAX;
			printf("Requested speed is too low: %d Hz. Will use %d Hz instead.\n",
				hz, spibrg / 32 * 16);
		}
	} else {
		pm = spibrg / (hz * 2);
/*delete by zhangjj 2015-11-12 change spi max HZ*/
#if 0
		if (pm < CSMODE_PM_MIN)
			pm = CSMODE_PM_MIN;
#endif
/*delete end*/
	}

	hw_mode |= CSMODE_PM(pm);
	/* Reset the hw mode */
	spi_reg_read(&spidev->spi_reg->mode, &regval);

	/* Turn off SPI unit prior changing mode */
	spi_reg_write(&spidev->spi_reg->mode, regval & ~SPMODE_ENABLE);
	spi_reg_write(&spidev->spi_reg->csmode[cs_sel], hw_mode);
	spi_reg_write(&spidev->spi_reg->mode, regval);
#if 0
	unsigned int mmode=0,eevent = 0, mmask = 0,command = 0,ccmode=0;
	spi_reg_read(&spidev->spi_reg->mode, &mmode);
	spi_reg_read(&spidev->spi_reg->event, &eevent);
	spi_reg_read(&spidev->spi_reg->mask, &mmask);
	spi_reg_read(&spidev->spi_reg->command, &command);
	spi_reg_read(&spidev->spi_reg->csmode[cs_sel], &ccmode);
	printf("spi cs_sel=%d mode=0x%08x event=0x%08x mask=0x%08x command=0x%08x csmode=0x%08x\n",
		cs_sel,mmode,eevent,mmask,command, ccmode);
#endif
	return 0;	
}





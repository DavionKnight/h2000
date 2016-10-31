/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:fzs
*  File Name        	:/home/kevin/works/projects/H20RN-2000/drivers/spidrv/epcsdrv.c
*  Create Date        	:2016/10/27 06:51
*  Last Modified      	:2016/10/27 06:51
*  Description    	:
*/

#include <stdio.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

#include "spi.h"
#include "spidrv.h"
#include "spidrv_common.h"
#include "gpiodrv.h"

extern struct spi_device spidev;
extern struct sembuf spidrv_sembufLock, spidrv_sembufUnlock;
extern int spidrv_semid;

/* Status Register bits. */
#define	SR_WIP			1	/* Write in progress */
#define	SR_WEL			2	/* Write enable latch */

#define	MAX_READY_WAIT_COUNT	100000

static int epcs_wait_ready(struct spi_device *spidev)
{
	int count;
	unsigned char sr = 0;

	/* one chip guarantees max 5 msec wait here after page writes,
	 * but potentially three seconds (!) after page erase.
	 */
	for (count = 0; count < MAX_READY_WAIT_COUNT; count++) {
		if (spidrv_epcs_opt(spidev, OPCODE_RDSR, 0, &sr, 0) < 0)
			break;
		else if (!(sr & SR_WIP))
			return 0;
		usleep(1000);

		/* REVISIT sometimes sleeping would be best */
	}

	return 1;
}

static int epcs_write_enable(struct spi_device *spidev)
{
	unsigned char data;

	return spidrv_epcs_opt(spidev, OPCODE_WREN, 0, &data, 0);
}

int epcs_erase_chip()
{
	unsigned char data;
	int ret = 0;

	semop(spidrv_semid, &spidrv_sembufLock, 1);
	
	gpio_direction_output(12, 0);
	spidev.max_speed_hz = 6500000;//the real rate 6.25M
	spidev.chip_select = CHIPSELECT_EPCS;
	spidev.mode = SPI_MODE_3;
	spidev.bits_per_word = 8;  /*need verify*/

	spi_setup(&spidev);

	/* Wait until finished previous write command. */
	if (epcs_wait_ready(&spidev))
	{
		ret = -1;
		goto END;
	}
	
	/* Send write enable, then erase commands. */
	epcs_write_enable(&spidev);

	spidrv_epcs_opt(&spidev, OPCODE_CHIP_ERASE, 0, &data, 0);

END:
	gpio_direction_output(12, 1);
	semop(spidrv_semid, &spidrv_sembufUnlock, 1);

	return ret;
}

int epcs_erase_sector(unsigned int offset)
{
	int ret = 0;
	int data = 0;

	semop(spidrv_semid, &spidrv_sembufLock, 1);
	
	gpio_direction_output(12, 0);
	spidev.max_speed_hz = 6500000;//the real rate 6.25M
	spidev.chip_select = CHIPSELECT_EPCS;
	spidev.mode = SPI_MODE_3;
	spidev.bits_per_word = 8;  /*need verify*/

	spi_setup(&spidev);
	/* Wait until finished previous write command. */

	if (epcs_wait_ready(&spidev))
	{
		ret = -1;
		goto END;
	}

	/* Send write enable, then erase commands. */
	epcs_write_enable(&spidev);

	spidrv_epcs_opt(&spidev, OPCODE_ERASE_SECTOR, offset, &data, 0);

END:
	gpio_direction_output(12, 1);
	semop(spidrv_semid, &spidrv_sembufUnlock, 1);

	return ret;
}


int epcs_spi_read(unsigned int addr, unsigned char *data, size_t count)
{
	int ret = 0;
	unsigned int len = 0;

	if(NULL == data)
		return -1;

	semop(spidrv_semid, &spidrv_sembufLock, 1);
	
	gpio_direction_output(12, 0);
	spidev.max_speed_hz = 6500000;//the real rate 6.25M
	spidev.chip_select = CHIPSELECT_EPCS;
	spidev.mode = SPI_MODE_3;
	spidev.bits_per_word = 8;  /*need verify*/

	spi_setup(&spidev);

	if (epcs_wait_ready(&spidev))
	{
		ret = -1;
		goto END;
	}

	if (spidrv_epcs_opt(&spidev, OPCODE_READ, addr, data, count) < 0) 
	{
		printf("epcs_spi_read failed.\n");
		ret = -1;
	}

END:
	gpio_direction_output(12, 1);
	semop(spidrv_semid, &spidrv_sembufUnlock, 1);

	return ret;
	
}

int epcs_spi_write(unsigned short addr, unsigned char *data, size_t count)
{
	int ret = 0;
	unsigned int len = 0;
	unsigned short address = 0;

	if(NULL == data)
		return -1;

	semop(spidrv_semid, &spidrv_sembufLock, 1);
	
	gpio_direction_output(12, 0);
	spidev.max_speed_hz = 6500000;//the real rate 6.25M
	spidev.chip_select = CHIPSELECT_EPCS;
	spidev.mode = SPI_MODE_3;
	spidev.bits_per_word = 8;  /*need verify*/

	spi_setup(&spidev);

	if (epcs_wait_ready(&spidev))
	{
		ret = -1;
		goto END;
	}

	/* Send write enable, then erase commands. */
	epcs_write_enable(&spidev);

	if(spidrv_epcs_opt(&spidev, OPCODE_WRITE, addr, data, count)< 0)
	{
		printf("epcs_spi_write failed.\n");
		ret = -1;
	}
END:
	gpio_direction_output(12, 1);
	semop(spidrv_semid, &spidrv_sembufUnlock, 1);

	return ret;
}





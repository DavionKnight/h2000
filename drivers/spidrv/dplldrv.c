/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:fzs
*  File Name        	:/home/kevin/works/projects/H20RN-2000/drivers/spidrv/dplldrv.c
*  Create Date        	:2016/10/27 05:47
*  Last Modified      	:2016/10/27 05:47
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

extern struct spi_device spidev;
extern struct sembuf spidrv_sembufLock, spidrv_sembufUnlock;
extern int spidrv_semid;

int dpll_spi_read(unsigned short addr, unsigned char *data, size_t count)
{
	int ret = 0;
	unsigned int len = 0;

	semop(spidrv_semid, &spidrv_sembufLock, 1);
	
	spidev.max_speed_hz = 6500000;//the real rate 6.25M
	spidev.chip_select = CHIPSELECT_DPLL;
	spidev.mode = SPI_MODE_3;
	spidev.bits_per_word = 8;  /*need verify*/

	spi_setup(&spidev);

	{
		int i;
		int loop;
	
		if( (count % 2 ) == 0 )	
		  	loop = count / 2;
		else
			loop = (count + 1) / 2;				
			
		for(i = 0; i < loop; i++)
		{	
		 	if (spidrv_mix_read(&spidev, (unsigned short)(addr + 2 * i), data + 2 *i, 2) < 0) {
				printf("dpll spi read failed.\n");
				ret = -1;
			}
		}
	}

	semop(spidrv_semid, &spidrv_sembufUnlock, 1);

	return ret;
}
int dpll_spi_write(unsigned short addr, unsigned char *data, size_t count)
{
	int ret = 0;
	unsigned int len = 0;
	unsigned short address = 0;

	semop(spidrv_semid, &spidrv_sembufLock, 1);
	
	spidev.max_speed_hz = 6500000;//the real rate 6.25M
	spidev.chip_select = CHIPSELECT_DPLL;
	spidev.mode = SPI_MODE_3;
	spidev.bits_per_word = 8;  /*need verify*/

	spi_setup(&spidev);

	if(spidrv_mix_write(&spidev, (unsigned short)addr, data, count) < 0)
	{
		printf("dpll spi write failed.\n");
		ret = -1;
	}

	semop(spidrv_semid, &spidrv_sembufUnlock, 1);

	return ret;
}




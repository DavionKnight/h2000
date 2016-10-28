/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:fzs
*  File Name        	:/home/kevin/works/projects/H20PN-2000/drivers/spidrv/spidrv.c
*  Create Date        	:2016/09/22 16:14
*  Last Modified      	:2016/09/22 16:14
*  Description    	:
*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <sys/ipc.h>  
#include <sys/sem.h>  
#include <sys/types.h>	
#include <sys/mman.h>	
#include "spi.h"
#include "spidrv.h"
#include "spidrv_common.h"


#define MAP_SIZE 		4096UL
#define MAP_MASK 		(MAP_SIZE - 1)


static int initialized = 0;

struct spi_device spidev;
struct sembuf spidrv_sembufLock, spidrv_sembufUnlock;
int spidrv_semid;

#define MULTI_REG_LEN_MAX		512
#define FLASH_PAGESIZE		256

int epcs_spi_opt(struct spi_device *spi, int opcode, unsigned int addr, unsigned char *data, unsigned int count)
{
	unsigned char rxbuf[MULTI_REG_LEN_MAX] = {0};
	unsigned char txbuf[MULTI_REG_LEN_MAX + 2] = {0};
	unsigned int len = 0;
	int ret = 0;

	if(NULL == data)	
		return -1;

	if(OPCODE_RDSR == opcode)
	{
		txbuf[0] = opcode;
		len = 2;
	}
	else if(OPCODE_WREN == opcode)
	{
		txbuf[0] = opcode;
		len = 1;
	}
	else if(OPCODE_CHIP_ERASE == opcode)
	{
		txbuf[0] = opcode;
		len = 1;
	}
	else if(OPCODE_ERASE_SECTOR == opcode)
	{
		txbuf[0] = opcode;
		txbuf[1] = *data >> 16; 
		txbuf[2] = *data >> 8; 
		txbuf[3] = *data; 
		len = 4;
	}
	else if(OPCODE_READ == opcode)
	{
		txbuf[0] = opcode;
		txbuf[1] = addr >> 16; 
		txbuf[2] = addr >> 8; 
		txbuf[3] = addr; 
		len = 4 + count;
	
	}
	else if(OPCODE_WRITE == opcode)
	{
		unsigned int page_offset = addr%FLASH_PAGESIZE;
		unsigned int page_size = 0;

		txbuf[0] = opcode;
		txbuf[1] = addr >> 16; 
		txbuf[2] = addr >> 8; 
		txbuf[3] = addr; 
		if(page_offset+count <= FLASH_PAGESIZE)
		{
			len = 4 + count;
			memcpy(&txbuf[4], data, count);
		}
		else{
			printf("addr%256!=0 or count%256!=0 error\n");
			return -1;
		}
	
	}
	else
		return -1;

	if(ret = spi_transfer(spi, txbuf, rxbuf, len) < 0)
	{
		printf("epcs_spi_opt spi_transfer error\n");
		return -1;
	}
	if(OPCODE_RDSR == opcode)
	{
	   memcpy(data, &rxbuf[1], 1);
	}
	else if(OPCODE_READ == opcode)
	{
		memcpy(data, &rxbuf[4], count);
	}

	return 0;
}

int mix_spi_write(struct spi_device *spi,unsigned short addr, unsigned char *data, size_t count)
{
	int ret = 0;
	unsigned int len = 0;
	unsigned char rxbuf[MULTI_REG_LEN_MAX + 2] = {0};
	unsigned char txbuf[MULTI_REG_LEN_MAX + 2] = {0};
	unsigned short address = 0;

	if (!data || count > MULTI_REG_LEN_MAX)
		return -1;

	if(CHIPSELECT_FPGA == spi->chip_select)
	{
		if(1 == count)
		{
			txbuf[0] = SPI_FPGA_WR_SINGLE;        
		}
		else 
		{
			txbuf[0] = SPI_FPGA_WR_BURST;
		}
		txbuf[1] = (unsigned char)((addr >> 8) & 0xff);
		txbuf[2] = (unsigned char)((addr) & 0xff);

		/*fill txbuf len equal to rx count*/
		memcpy(&txbuf[3], data, count);

		/* Build our spi message */
		len = count + 4;
	}
	else if(CHIPSELECT_DPLL == spi->chip_select)
	{
		address = (addr << 1) & 0x7ffe;

		txbuf[0] = (unsigned char)((address >> 8) & 0xff);
		txbuf[1] = (unsigned char)((address) & 0xff);

		/* MSB must be '1' to read */
		txbuf[0] &= ~0x80;
		/* LSB must be '1' to burst read */
		if (count > 1)
			txbuf[1] |= 0x01;

		/*fill txbuf len equal to rx count*/
		memcpy(&txbuf[2], data, count);

		/* Build our spi message */
		len = count + 2;
	
	}
	else
	{
		printf("mix spi read error,chip select=%d\n",spi->chip_select);
		return -1;
	}
	if(ret = spi_transfer(spi, txbuf, rxbuf, len) < 0)
	{
		printf("spi transfer error\n");
		return -1;
	}

	return 0;
}
int mix_spi_read(struct spi_device *spi,unsigned short addr, unsigned char *data, size_t count)
{
	int ret = 0;
	int len = 0;
	unsigned char rxbuf[MULTI_REG_LEN_MAX + 2] = {0};
	unsigned char txbuf[MULTI_REG_LEN_MAX + 2] = {0};
	unsigned short address = 0;

	if (!data || count > MULTI_REG_LEN_MAX)
		return -1;

	if(CHIPSELECT_FPGA == spi->chip_select)
	{
		if(1 == count)
		{
			txbuf[0] = SPI_FPGA_RD_SINGLE;        
		}
		else 
		{
			txbuf[0] = SPI_FPGA_RD_BURST;
		}
		txbuf[1] = (unsigned char)((addr >> 8) & 0xff);
		txbuf[2] = (unsigned char)((addr) & 0xff);
		txbuf[3] = 0;

		/* Build our spi message */
		len = count + 4;
	}
	else if(CHIPSELECT_DPLL == spi->chip_select)
	{
		address = (addr << 1) & 0x7ffe;

		txbuf[0] = (unsigned char)((address >> 8) & 0xff);
		txbuf[1] = (unsigned char)((address) & 0xff);

		/* MSB must be '1' to read */
		txbuf[0] |= 0x80;
		/* LSB must be '1' to burst read */
		if (count > 1)
			txbuf[1] |= 0x01;

		/* Build our spi message */
		len = count + 2;
	
	}
	else
	{
		printf("mix spi read error,chip select=%d\n",spi->chip_select);
		return -1;
	}
	if(ret = spi_transfer(spi, txbuf, rxbuf, len) < 0)
	{
		printf("spi transfer error\n");
		return -1;
	}

	/* memcpy(data, &rx_buf[2], count); */
	if(CHIPSELECT_FPGA == spi->chip_select)
	{
	   memcpy(data, &rxbuf[4], count);
	}
	else if(CHIPSELECT_DPLL == spi->chip_select)
	{
	   memcpy(data, &rxbuf[2], count);
	}
	else 
	{
		printf("mix spi read error\n");
		return -1;
	}

	return 0;

	
}

static int spidrv_semlock_init()  
{  
	union semun arg;

	spidrv_semid = semget(0x0812, 1, IPC_CREAT|IPC_EXCL|0666);
	if(-1 == spidrv_semid)
	{
		initialized = 1;
		spidrv_semid = semget(0x0812, 1, IPC_CREAT|0666);
		if (spidrv_semid == -1){  
			return -1;  
		}  
	}
	else
	{
//		printf("sem created\n");
		initialized = 0;
		arg.val = 1;
		semctl(spidrv_semid, 0, SETVAL, arg);  
	}

	spidrv_sembufLock.sem_num = 0;  
	spidrv_sembufLock.sem_op = -1;  
	spidrv_sembufLock.sem_flg = SEM_UNDO;  

	spidrv_sembufUnlock.sem_num = 0;  
	spidrv_sembufUnlock.sem_op = 1;  
	spidrv_sembufUnlock.sem_flg = SEM_UNDO;  

	return 0;
}

#define SPI_REGISTER_BASE 0xffe07000
int fd_mmap;
static int spidrv_mmap_init()
{

	if((fd_mmap = open("/dev/mem", O_RDWR | O_SYNC)) == -1) 
	{
		return -1;
	}
	spidev.spi_reg = (struct spi_reg_t *)mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_mmap, SPI_REGISTER_BASE);
	if(NULL == spidev.spi_reg)
		return -1;
	return 0;
}

static void spidrv_mmap_exit()
{
	munmap((void *)spidev.spi_reg,MAP_SIZE);
	close(fd_mmap);
}

static int spidrv_setup_init()
{
	int i = 0;

	semop(spidrv_semid, &spidrv_sembufLock, 1);

	spi_dev_init(&spidev);

	for(i = 0; i <4; i++)
	{
		spidev.max_speed_hz = 6500000;//the real rate 6.25M
		spidev.chip_select = i;
		spidev.mode = SPI_MODE_3;
		spidev.bits_per_word = 8;

		spi_setup(&spidev);
	}

	semop(spidrv_semid, &spidrv_sembufUnlock, 1);

	return 0;
}
int spidrv_init()
{
	memset(&spidev, 0, sizeof(spidev));

	if(spidrv_semlock_init())
	{
		SPIDRV_PRINT("spidrv semlock init err\n");
		return -1;
	}
	if(spidrv_mmap_init())
	{
		SPIDRV_PRINT("spidrv mmap init err\n");
		return -1;
	}
	/*need sem lock, I think*/
	if(0 == initialized)
	{
		if(spidrv_setup_init())
		{
			SPIDRV_PRINT("spidrv spi setup init error\n");	
			return -1;
		}
		SPIDRV_PRINT("Initialize spidrv successfully\n");	
	}


	return 0;
}
int spidrv_exit()
{
	spidrv_mmap_exit();
//	spidrv_semlock_exit();
	return 0;
}








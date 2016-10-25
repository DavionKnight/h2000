/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:Kevin_fzs
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

#define CHIPSELECT_FPGA		0
#define CHIPSELECT_DPLL		1
#define CHIPSELECT_EPCS		2

#define GPIO_FPGAFLASH         12

#define SPI_FPGA_WR_SINGLE 0x01
#define SPI_FPGA_WR_BURST  0x02
#define SPI_FPGA_RD_BURST  0x03
#define SPI_FPGA_RD_SINGLE 0x05

#define MAP_SIZE 		4096UL
#define MAP_MASK 		(MAP_SIZE - 1)

#define __SPIDRV_DEBUG__  

#ifdef __SPIDRV_DEBUG__  
#define SPIDRV_PRINT(format,...) printf("File: "__FILE__", Line: %05d: "format"\n", __LINE__, ##__VA_ARGS__)  
#else  
#define SPIDRV_PRINT(format,...) zlog() /*need add*/
#endif  

int initialized = 0;

struct spi_device spidev;
struct sembuf bufLock, bufUnlock;
int semid;

#define MULTI_REG_LEN_MAX		512
static int mix_spi_write(struct spi_device *spi,unsigned short addr, unsigned char *data, size_t count)
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
	else if(CHIPSELECT_EPCS == spi->chip_select)
	{}
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
static int mix_spi_read(struct spi_device *spi,unsigned short addr, unsigned char *data, size_t count)
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
	else if(CHIPSELECT_EPCS == spi->chip_select)
	{}
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
	else if(CHIPSELECT_EPCS == spi->chip_select)
	{}

	return 0;

	
}

static unsigned char rtClause = 0;
int fpga_spi_read(unsigned int addr, unsigned char *data, size_t count, unsigned char slot)
{
	int ret = 0;
	unsigned int len = 0;
	unsigned short address = 0;

/*        if((count > FPGA_CR_CLAU_UNIT_SIZE))
        {
                printf("Read length should be less than 32 count=%d\n",count);
                return ERR_FPGA_DRV_ARGV;
        }*/
	if(!data)
		return ERR_FPGA_DRV_ARGV;

	semop(semid, &bufLock, 1);

	spidev.max_speed_hz = 6500000;//the real rate 6.25M
	spidev.chip_select = CHIPSELECT_FPGA;
	spidev.mode = SPI_MODE_3;
	spidev.bits_per_word = 8;  /*need verify*/

	spi_setup(&spidev);

	if((slot == 0x10) || (slot == 0x11)) /*read local fpga*/
	{
		mix_spi_read(&spidev, (unsigned short)addr, data, count);
	}
	else if(slot <= 0xf) /*remote board fpga*/
	{
		unsigned int rdata[FPGA_RT_CLAU_UNIT_SIZE/2] = {0};
		unsigned int reg_addr;
		unsigned char data_set[4] = {0};
		unsigned int data_rw = 0,bufaddr = 0, clause = 0;
		unsigned int value_en;
		unsigned short read_reg;
		unsigned int delay_count = 1000;
		unsigned short *pbuf = (unsigned short *)data;
		unsigned int i = 0;

		clause = rtClause++;
		if(rtClause >= FPGA_RT_CLAU)
			rtClause = 0;

//		printf("clause = %d,unitboard slot = 0x%x, addr = 0x%02x\n", clause, slot,addr);
		bufaddr = FPGA_RT_CMD_BUFF_ADDR + clause*FPGA_RT_CLAU_UNIT_SIZE;
		read_reg = FPGA_RT_CLAU_ADDR + clause;

		data_set[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
		data_set[1] = addr & 0xff;
		data_set[2] = (0x5 << 5) | ((bufaddr&0x1f00) >> 8);
		data_set[3] = ((bufaddr&0xe0)|(0x1f));

		memcpy(&data_rw,data_set,sizeof(data_rw));

		mix_spi_write(&spidev, (unsigned short)read_reg,(unsigned char *)&data_rw, sizeof(data_rw));

		do{
			mix_spi_read(&spidev, (unsigned short)FPGA_RT_RD_OVER_FLGA, (unsigned char *)&value_en, sizeof(value_en));
			if((value_en == 0)||(delay_count<1))
				break;
		}while(delay_count--);

		reg_addr = FPGA_RT_BUFF_ADDR + clause * FPGA_RT_CLAU_UNIT_SIZE/2;

		mix_spi_read(&spidev, (unsigned short)reg_addr, (unsigned char *)rdata, sizeof(rdata));
		for(i = 0; i < count/2; i++)
			pbuf[i] = i%2?((rdata[i/2]>>16)&0xffff):(rdata[i/2]&0xffff);	
	}
	else
		printf("fpga_spi_read slot %d error\n", slot);

	semop(semid, &bufUnlock, 1);

	return 0;
}

int fpga_spi_write(unsigned int addr, unsigned char *data, size_t count, unsigned char slot)
{
	int ret = 0;
	unsigned int len = 0;
	unsigned short address = 0;

	if(!data)
		return ERR_FPGA_DRV_ARGV;
	semop(semid, &bufLock, 1);
	
	spidev.max_speed_hz = 6500000;//the real rate 6.25M
	spidev.chip_select = CHIPSELECT_FPGA;
	spidev.mode = SPI_MODE_3;
	spidev.bits_per_word = 8;  /*need verify*/

	spi_setup(&spidev);

	if((slot == 0x10) || (slot == 0x11)) /*read local fpga*/
	{
		mix_spi_write(&spidev, (unsigned short)addr, data, count);
	}
	else if(slot <= 0xf) /*remote board fpga*/
	{
		unsigned char wdata[8] = {0};
		unsigned short *ptr = (unsigned short *)data;
		unsigned int wrdata=0;


		for(ptr; ptr < (unsigned short *)(data + count); ptr++)
		{
			wdata[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
			wdata[1] = addr & 0xff;
			wdata[2] = (*ptr & 0xff00) >> 8;
			wdata[3] = *ptr & 0xff;
			memcpy(&wrdata, wdata,sizeof(wrdata));
			mix_spi_write(&spidev, FPGA_RT_WR_ADDR, (unsigned char *)&wrdata, sizeof(wrdata));
			usleep(10);
			addr ++;
		}

	}
	else
		printf("fpga_spi_write slot %d error\n", slot);

	semop(semid, &bufUnlock, 1);

	return 0;
}
int dpll_spi_read(unsigned short addr, unsigned char *data, size_t count)
{
	int ret = 0;
	unsigned int len = 0;

	semop(semid, &bufLock, 1);
	
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
		 	if (mix_spi_read(&spidev, (unsigned short)(addr + 2 * i), data + 2 *i, 2) < 0) {
				printf("dpll-mix spi read failed.!!!!!!!!!!!!!!\n");
			}
		}
	}

	semop(semid, &bufUnlock, 1);

	return 0;
}
int dpll_spi_write(unsigned short addr, unsigned char *data, size_t count)
{
	int ret = 0;
	unsigned int len = 0;
	unsigned short address = 0;

	semop(semid, &bufLock, 1);
	
	spidev.max_speed_hz = 6500000;//the real rate 6.25M
	spidev.chip_select = CHIPSELECT_DPLL;
	spidev.mode = SPI_MODE_3;
	spidev.bits_per_word = 8;  /*need verify*/

	spi_setup(&spidev);

	mix_spi_write(&spidev, (unsigned short)addr, data, count);

	semop(semid, &bufUnlock, 1);

	return 0;
}
union semun
{
  int val;			/* value for SETVAL */
  struct semid_ds *buf;		/* buffer for IPC_STAT & IPC_SET */
  unsigned short int *array;	/* array for GETALL & SETALL */
  struct seminfo *__buf;	/* buffer for IPC_INFO */
  struct __old_semid_ds *__old_buf;
};
static int spidrv_semlock_init()  
{  
	union semun arg;

	semid = semget(0x0812, 1, IPC_CREAT|IPC_EXCL|0666);
	if(-1 == semid)
	{
		initialized = 1;
		semid = semget(0x0812, 1, IPC_CREAT|0666);
		if (semid == -1){  
			return -1;  
		}  
	}
	else
	{
//		printf("sem created\n");
		initialized = 0;
		arg.val = 1;
		semctl(semid, 0, SETVAL, arg);  
	}

	bufLock.sem_num = 0;  
	bufLock.sem_op = -1;  
	bufLock.sem_flg = SEM_UNDO;  

	bufUnlock.sem_num = 0;  
	bufUnlock.sem_op = 1;  
	bufUnlock.sem_flg = SEM_UNDO;  

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

	semop(semid, &bufLock, 1);

	spi_dev_init(&spidev);

	for(i = 0; i <4; i++)
	{
		spidev.max_speed_hz = 6500000;//the real rate 6.25M
		spidev.chip_select = i;
		spidev.mode = SPI_MODE_3;
		spidev.bits_per_word = 8;

		spi_setup(&spidev);
	}

	semop(semid, &bufUnlock, 1);

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








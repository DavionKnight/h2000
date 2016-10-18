/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:Kevin_fzs
*  File Name        	:/home/kevin/works/projects/H20PN-2000/drivers/spidrv/spitest-old.c
*  Create Date        	:2016/09/26 05:35
*  Last Modified      	:2016/09/26 05:35
*  Description    	:
*/

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <pthread.h>

pthread_mutex_t mutex_fpga= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_dpll= PTHREAD_MUTEX_INITIALIZER;

typedef struct spi_rdwr_argv
{
        unsigned char   cs;
        unsigned short  addr;
        unsigned short  len;
        unsigned char   buff[64];
}spi_rdwr;


#define W25_ERASE_CHIP          	_IOW(SPI_IOC_MAGIC, 9,  __u8)
#define W25_ERASE_SECTOR    		_IOW(SPI_IOC_MAGIC, 10, __u32)
#define W25P16_READ		   	_IOR(SPI_IOC_MAGIC, 11, __u32)
#define W25P16_WRITE	   		_IOW(SPI_IOC_MAGIC, 12, __u32)
#define W25P1165_ID    			_IOR(SPI_IOC_MAGIC, 13,  __u8)
#define SPI_IOC_OPER_FLASH		_IOW(SPI_IOC_MAGIC, 14, __u8)
#define SPI_IOC_OPER_FLASH_DONE		_IOW(SPI_IOC_MAGIC, 15, __u8)

#define TEST_ADDR 			0x0
#define BLOCK_SIZE			0x100



int fd,errnu_f = 0,errnu_d = 0;
spi_rdwr sopt;

int fpga_dpll_test()
{
	struct   timeval   start,stop,diff;
	unsigned int i = 100000;
	unsigned int data=0;


	fd = open("/dev/spidev0.0", O_RDWR);
	if (fd == -1) {
		printf("dpll file open failed.\n");
		return -1;
	}

	gettimeofday(&start, NULL);
	while(i--)
	{
		sopt.cs = 0;
		sopt.addr = 0x0;
		sopt.len = 4;
		sopt.buff[0] = 0;
		sopt.buff[1] = 0;
		sopt.buff[2] = 0;
		sopt.buff[3] = 0;

		read(fd, &sopt, sizeof(sopt));
		data = sopt.buff[3];

		if(data!=0x01)
		{
			printf("i=%d, addr 0, data 0x%08x\n",i ,data);
			break;
		}
		sopt.cs = 0;
		sopt.addr = 0x1040;
		sopt.len = 4;
		sopt.buff[0] = (i>>24)&0xff;
		sopt.buff[1] = (i>>16)&0xff;
		sopt.buff[2] = (i>>8)&0xff;
		sopt.buff[3] = (i>>0)&0xff;

		write(fd, &sopt, sizeof(sopt));
		read(fd, &sopt, sizeof(sopt));
		data = sopt.buff[3];

		if(0)
		{
			printf("i=%d, addr 1, data 0x%08x\n",i ,data);
			break;
		}
	}
	gettimeofday(&stop, NULL);
	printf("start:%d s %d ms\n",(unsigned int)start.tv_sec,start.tv_usec);
	printf("stop :%d s %d ms\n",(unsigned int)stop.tv_sec,stop.tv_usec);
	return 0;
}

int main(int argc, char *argv[])
{
	int pid;
	char arg1 = 0, arg2 = 1;
	pthread_t id_1,id_2,id_3,id_4,id_5;


	fpga_dpll_test();

	return 0;
}




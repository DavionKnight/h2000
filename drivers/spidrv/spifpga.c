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
#include <sys/time.h>
#include <time.h>
#include "spidrv.h"

int fpga_test()
{
	unsigned long i = 0;
	unsigned char data[10] = {0}, rdata[10] = {0};
	struct   timeval   start,stop,diff;

	gettimeofday(&start, NULL);

	while(1)
	{
		fpga_spi_read(1,(unsigned char *)data, 4);

		if(0x16 != data[3])
		{
			printf("fpga---i=%d-----addr 1, data 0x%02x\n",i,data[3]);
			break;
		}

		data[0] = 0xab;
		data[1] = 0xcd;
		data[2] = 0xef;
		data[3] = i&0xff;
		//usleep(500000);
		fpga_spi_write(0x1040,(unsigned char *)data, 4);
		fpga_spi_read(0x1040, (unsigned char *)rdata, 4);
		if(data[3]!=rdata[3])
		{
			printf("fpga--------data[3]=0x%02x,rdata[3]=0x%02x\n",data[3],rdata[3]);
			break;
		}
		if((i++)>20000)
		{
			i = 1;
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
	void *status;
	char arg1 = 0, arg2 = 1;
	pthread_t id_1,id_2,id_3,id_4,id_5;

	if(-1 == spidrv_init())
	{
		printf("spidrv_init error\n");
		return 0;
	}

	fpga_test();

	spidrv_exit();

	return 0;
}




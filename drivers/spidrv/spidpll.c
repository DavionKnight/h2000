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

int dpll_test()
{
	unsigned long long i = 100000;
	unsigned char data[10] = {0}, rdata[10] = {0};
	struct   timeval   start,stop,diff;

	gettimeofday(&start, NULL);

	while(1)
	{
		usleep(5);
		dpll_spi_read(0x00,(unsigned char *)data, 2);
		if((data[0]!=0xa8) && (data[1]!=0x7a))
		{
			printf("dpll============addr 0, data[0]=0x%02x,data[1]=0x%02x\n",
				data[0],data[1]);
			break;
		}
		data[0] = i&0xff;
		dpll_spi_write(0x00d3,(unsigned char *)data, 2);
		dpll_spi_read(0x00d3,(unsigned char *)rdata, 2);
		if(rdata[0] != data[0])
		{
			printf("dpll============addr 0xd3, data[0]=0x%02x,rdata[0]=0x%02x\n",
				data[0],rdata[0]);
			break;
		}
		if((i--)<2)
		{
			i = 100000;
		}
//	break;
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

	dpll_test();

	spidrv_exit();

	return 0;
}




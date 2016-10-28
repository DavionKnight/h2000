/**********************************************
 * @file	rfpga.c
 * @author	zhangjj <zhangjj@bjhuahuan.com>
 * @date	2015-08-20
 *********************************************/

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <string.h>
#include "spidrv.h"


int main(int argc, char *argv[])
{
	int ret = 0;
	unsigned short addr = 0,len;
	unsigned char data[32] = {0};
	unsigned char slot_num = 0;
	int i = 0;	

	if(-1 == spidrv_init())
	{
		printf("spidrv_init error\n");
		return 0;
	}

	if (argc == 5 && argv[1][0] == 'r') {
		sscanf(argv[2], "%hhx", &slot_num);
		sscanf(argv[3], "%hx", &addr);
		sscanf(argv[4], "%hd", &len);

		printf("slot num %x addr 0x%04x len %d:\n", slot_num, (unsigned short)addr, len);
		epcs_spi_read(addr, (unsigned char *)data, len);

		printf("The result:\n");
		for(i = 0; i < len; i++)
		{	
			printf("0x%02x ",data[i]);
			if((i+1)%16 == 0)
				printf("\n");
		}
		printf("\n");
	}
	else if (argc == 5 && argv[1][0] == 'w') {
		unsigned short data;
		sscanf(argv[2], "%hhx", &slot_num);
		sscanf(argv[3], "%hx", &addr);
		sscanf(argv[4], "%hx", &data);
		printf("slot num %x,write addr 0x%x data 0x%04x\n", slot_num, addr, data);
		epcs_spi_write(addr, (unsigned char *)&data, sizeof(data)); 
	}
	else
	{
		printf("\nepcs read <slot:hex> <addr:hex> <len:dec>\n");
		printf("epcs write <slot:hex> <addr:hex> <data:hex>\n\n");
		printf("demo:	epcs read 0x0 0x0 2\n");
		printf("	epcs write 0x0 0x85 0xee\n");
	}
	
	spidrv_exit();

	return 0;	
}




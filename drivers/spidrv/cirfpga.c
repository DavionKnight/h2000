/**********************************************
 * @file	cirfpga.c
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
	unsigned char data[30] = {0};
	unsigned char slot_num = 0;
	int i = 0;	
	int clause = 0;

	if(-1 == spidrv_init())
	{
		printf("spidrv_init error\n");
		return 0;
	}

	if (argc == 5 && argv[1][0] == 'r') {
		sscanf(argv[2], "%hhx", &slot_num);
		sscanf(argv[3], "%hx", &addr);
		sscanf(argv[4], "%hd", &len);
#if 0
		printf("0 %s\n",argv[0]);
		printf("1 %s\n",argv[1]);
		printf("2 %s\n",argv[2]);
#endif
		printf("slot num %x addr 0x%04x len %d:\n", slot_num, (unsigned short)addr, len);
		ret = fpga_rm_cir_read_get(&clause);
		if(ret < 0)
		{
			printf("ret = %d, error\n",ret);
			return 0;
		}
		fpga_rm_cir_read_set(clause, slot_num, addr, len);
		fpga_rm_cir_en(clause);
		fpga_rm_cir_read(clause, slot_num, addr, (unsigned short *)data, len);
		printf("The result:\n");
		for(i = 0;i < len;i++)
		{
			printf("0x%02x ",data[i]);
			if(0 == (i+1)%16)
				printf("\n");
		}
		printf("\n");
	}
	else
	{
		printf("\ncirfpga read <slot:hex> <addr:hex> <len:dec>\n\n");
		printf("slot <0-0xf> unit board link GU08 TU02\n");
		printf("     <0x10-0x11> master board link PXPXMM\n\n");
		printf("demo:	cirfpga read 0x0 0x0 2\n");
	}
	
	spidrv_exit();

	return 0;	
}




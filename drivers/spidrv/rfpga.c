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
	unsigned short addr = 0,data;
	unsigned char slot_num = 0;
	int i = 0;	

	if(-1 == spidrv_init())
	{
		printf("spidrv_init error\n");
		return 0;
	}

	if (argc == 4 && argv[1][0] == 'r') {
		sscanf(argv[2], "%hhx", &slot_num);
		sscanf(argv[3], "%hx", &addr);
#if 0
		printf("0 %s\n",argv[0]);
		printf("1 %s\n",argv[1]);
		printf("2 %s\n",argv[2]);
#endif
		printf("slot num %x addr 0x%04x:\n", slot_num, (unsigned short)addr);
		data = 0x7E0;			//last realtime read clause
		fpga_rm_rt_read(0, slot_num, addr, (unsigned short *)&data, 1);
		printf("The result:\n0x%04x\n",data);
	}
	else if (argc == 5 && argv[1][0] == 'w') {
		sscanf(argv[2], "%hhx", &slot_num);
		sscanf(argv[3], "%hx", &addr);
		sscanf(argv[4], "%hx", &data);
		printf("slot num %x,write addr 0x%x data 0x%04x\n", slot_num,addr,data);
		fpga_rm_rt_write(slot_num, addr, &data, 1); 
	}
	else
	{
		printf("remotefpga read <slot:hex> <addr:hex>\n");
		printf("remotefpga write <slot:hex> <addr:hex> <data:hex>\n");
	}
	
	spidrv_exit();

	return 0;	
}




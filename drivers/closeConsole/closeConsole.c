/**********************************************
 * @file	bcm53101.c
 * @author	zhangjj <zhangjj@bjhuahuan.com>
 * @date	2015-10-22
 *********************************************/

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>


int main(int argc, char *argv[])
{
	int ret = 0, bcmfd = 0;
	unsigned short addr = 0,data;
	unsigned char page = 0;

	bcmfd = open( "/dev/ttyS1", O_RDWR);
	if( bcmfd == -1 )
		return -1;
	ioctl(bcmfd, TIOCCONS);	
	close(bcmfd);

	return 0;	
}




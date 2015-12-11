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
#include <string.h>

//#define	IDTDEBUG
#define WORDSIZE			4
struct bcm53101_t{
        unsigned char  page;
        unsigned char  addr;
        unsigned short val[4];
};


void pdata(unsigned char *pdata, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		printf(" %02x", pdata[i]);
	}
	printf("\n");
}

int main(int argc, char *argv[])
{
	int ret = 0, bcmfd = 0;
	unsigned short addr = 0,data;
	unsigned char page = 0;
	struct bcm53101_t bcmstru;

	bcmfd = open( "/dev/bcm53101", O_RDWR);
	if( bcmfd == -1 )
		return -1;

	if (argc == 4 && argv[1][0] == 'r') {
		sscanf(argv[2], "%hhx", &page);
		sscanf(argv[3], "%hx", &addr);
#if 0
		printf("0 %s\n",argv[0]);
		printf("1 %s\n",argv[1]);
		printf("2 %s\n",argv[2]);
#endif
		printf("page %x addr 0x%04x:\n", page, (unsigned short)addr);
		bcmstru.page = page;
		bcmstru.addr = addr;
		read(bcmfd, &bcmstru, sizeof(bcmstru));
		printf("The result:\n0x%04x 0x%04x 0x%04x 0x%04x\n",bcmstru.val[3],bcmstru.val[2],bcmstru.val[1],bcmstru.val[0]);
	}
	else if (argc == 5 && argv[1][0] == 'w') {
		sscanf(argv[2], "%hhx", &page);
		sscanf(argv[3], "%hx", &addr);
		sscanf(argv[4], "%hx", &data);
		printf("page %x,write addr 0x%d data 0x%04x:\n", page,addr,data);
		bcmstru.page = page;
		bcmstru.addr = addr;
		bcmstru.val[0]  = data;
		write(bcmfd, &bcmstru, sizeof(bcmstru)); 
	}
	else if (argc == 3 && argv[1][0] == 's') {
		sscanf(argv[2], "%hx", &data);
		if(data>2)
			printf("para error\n");
		else
		{
			ioctl(bcmfd, data, 0); 
	                printf("\n+++++Select BCM53101_%c********\n\n", data==1?'B':data?'C':'A');
		}
	}
	else
	{
		printf("bcm53101 read <page:hex> <addr:hex>\n");
		printf("bcm53101 write <page:hex> <addr:hex> <data:hex>\n");
		printf("bcm53101 select [0/1/2] \n");
	}
	
	close(bcmfd);

	return 0;	
}




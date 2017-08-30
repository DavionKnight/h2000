
/*
 * Copyright (C) 2016 Huahuan Corporation. All rights reserved.
 * 
 * writen by tianzhy 2016-10
 *
 *  
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <errno.h>
#include <linux/sockios.h>
//#include "phy_api.h"

struct bcm53101_t{
	unsigned char  which_sw;
        unsigned char  page;
        unsigned char  addr;
        unsigned short value[4];
}__attribute__((packed));

#define OUTBAND_STRING "eth0"
#define GET_PORT_LINKSTATE 0x10000

#define MII_BMCR            0x00        /* Basic mode control register */
#define MII_BMSR            0x02        /* Basic mode status register  */

#define BCM53101_A 0
#define BCM53101_B 1
#define BCM53101_C 2

int get_status_in_kernel()
{
		struct ifreq ifr;  
		int sockfd;  

		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)  
		{  
				printf("Create socket fails!\n");  
				return -1;    
		}  

		strcpy(ifr.ifr_name, "eth0");  
		if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0)  
		{  
				printf("ioctl SIOCGIFFLAGS fails!\n");  
				close(sockfd);  
				return -1;    
		}  

		close(sockfd);  

		return ifr.ifr_flags&0x1; 
}

int genphy_read_link(int bcmfd, unsigned char *link_status)
{
	int ret = 0;
	struct bcm53101_t bcmstr;
	int status = 0;

	bcmstr.which_sw = BCM53101_C;
	bcmstr.page= 0x11;
	bcmstr.addr= MII_BMSR;
	ret = read(bcmfd, &bcmstr, sizeof(bcmstr));	
	if(ret < 0)
		return ret;
	ret = read(bcmfd, &bcmstr, sizeof(bcmstr));	
	if(ret < 0)
		return ret;

	status = get_status_in_kernel();
//printf("value=0x%x  %d\n",bcmstr.value[0],status);

	*link_status = status & (bcmstr.value[0]>>2);

	return 0;
}

/*
 * return:
 * 1: success
 * other value: fail
 */
int hsl_eth0_get_linkstate( unsigned int *val)
{
        struct ifreq ifr;
        int sockfd;                     /* socket fd we use to manipulate stuff with */
	int ret = 0, bcmfd = 0;
        int error;
	int setval[2];
	unsigned char stat = 0;

        if(val == NULL)
        {
                printf("Invalid input para = NULL\n");
                return 0;
        }
	
	bcmfd = open( "/dev/bcm53101", O_RDWR);
	if( bcmfd == -1 )
		return -1;
	genphy_read_link(bcmfd, &stat);
	*val = stat;

	close(bcmfd);

        return 1;
}

#if 1 /* for test */
int main(void) 
{
        unsigned int val=0;

        hsl_eth0_get_linkstate(&val);
        printf("linkstate = %d\n", val);
	
	return 1;
}
#endif 


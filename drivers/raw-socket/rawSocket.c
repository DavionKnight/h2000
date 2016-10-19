/*
 *  COPYRIGHT NOTICE
 *  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
 *
 *  Author       	:Kevin_fzs
 *  File Name        	:/home/kevin/works/projects/H20RN-2000/drivers/raw-socket/rawSocket2.c
 *  Create Date        	:2016/10/19 10:04
 *  Last Modified      	:2016/10/19 10:04
 *  Description    	:
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/if_packet.h>
#include <net/if.h>

int sock, n;
char buffer[2048],sdbuf[128];
struct ethhdr *eth;
struct iphdr *iph;
struct ifreq ethreq;
struct sockaddr_ll sock_link;

int raw_send(void *arg)
{
	int i = 0;
	sock_link.sll_family = AF_PACKET;
	sock_link.sll_protocol = htons(ETH_P_ALL);

	strcpy(ethreq.ifr_name,"eth0");
#if 0
	if(-1 == ioctl(sock,SIOCGIFFLAGS,&ethreq)){
		perror("ioctl");
		close(sock);
		exit(1);
	}
	ethreq.ifr_flags |=IFF_PROMISC;
	if(-1 == ioctl(sock,SIOCGIFFLAGS,&ethreq)){
		perror("ioctl");
		close(sock);
		exit(1);
	}
#endif
	if ( ioctl( sock, SIOCGIFINDEX, &ethreq ) < 0 )
	{
		printf( "failed to get IF hw address!" );
		return 0;
	}
	sock_link.sll_ifindex = ethreq.ifr_ifindex;

	while (1) {
		int ret = 0;
		memset(sdbuf, 0, sizeof(sdbuf));
		for(i = 0;i<120;i++)
		{
			sdbuf[i] = i<0xfe?i:0xee;	
		}
#if 0
		sdbuf[6] = 0x00;
		sdbuf[7] = 0x1d;
		sdbuf[8] = 0x80;
		sdbuf[9] = 0x10;
		sdbuf[10] = 0x42;
		sdbuf[11] = 0x56;
//#else
		sdbuf[0] = 0xff;
		sdbuf[1] = 0xff;
		sdbuf[2] = 0xff;
		sdbuf[3] = 0xff;
		sdbuf[4] = 0xff;
		sdbuf[5] = 0xff;
#endif
		memcpy(sock_link.sll_addr,&sdbuf[6],6);
		if((ret = sendto(sock, sdbuf, sizeof(sdbuf), 0, (struct sockaddr *)&sock_link, sizeof(sock_link)))<0)
		{
			printf("failed to send to RAW ret = %d\n",ret);
		}
		sleep(1);
	}	
}



int main(int argc, char **argv) {
        pthread_t tid;

	memset(&sock_link,0,sizeof(sock_link));
	if (0>(sock=socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL)))) {
		perror("socket");
		exit(1);
	}


        pthread_create(&tid, NULL, (void *)raw_send, NULL);

	while (1) {
		printf("=====================================\n");
		//注意：在这之前我没有调用bind函数，原因是什么呢？
#if 1
		n = recvfrom(sock,buffer,2048,0,NULL,NULL);
		printf("%d bytes read\n",n);

		//接收到的数据帧头6字节是目的MAC地址，紧接着6字节是源MAC地址。
		eth=(struct ethhdr*)buffer;
		printf("Dest MAC addr:%02x:%02x:%02x:%02x:%02x:%02x\n",eth->h_dest[0],eth->h_dest[1],eth->h_dest[2],eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);
		printf("Source MAC addr:%02x:%02x:%02x:%02x:%02x:%02x\n",eth->h_source[0],eth->h_source[1],eth->h_source[2],eth->h_source[3],eth->h_source[4],eth->h_source[5]);
#endif
	}
}

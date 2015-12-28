/*
 * =====================================================================================
 *
 *       Filename:  bcm_chip_drv.c
 *
 *    Description:  BCM chip driver
 *
 *        Version:  1.0
 *        Created:  2013年10月29日 10时59分18秒
 *       Revision:  none
 *       Compiler:  powerpc_82xx-gcc
 *
 *
 *         Author:  LIUyk, 
 *   Organization:  
 *
 * =====================================================================================
 */


#include <stdio.h> 
#include <fcntl.h> 
#include <string.h>
#include "bcm_chip_drv.h"


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  bcm_mng_vlan_add
 *  Description: 
 *       Author:  LIUyk
 * =====================================================================================
 */
int
bcm_mng_vlan_add ( int unit, int port, int vid, int pri, int tag )
{
	int fd = -1;
	char dev_name[] = "dev/bcm_chip_drv";
	Drv_vlan_param vlan_param;

	if( (fd = open( dev_name, O_RDWR )) == -1 ) {
   		printf( "Cann't open device : %s\n", dev_name ); 
		return (-1);
   	}

	memset(&vlan_param, 0, sizeof(Drv_vlan_param));	
	vlan_param.vlan = vid;
	vlan_param.port = port;
	vlan_param.pri = pri;
	vlan_param.tag = tag;

	if ( 0 > ioctl( fd, DRV_CMD_VLAN_ADD, (unsigned long)(&vlan_param) ) ) {
		close(fd);
		printf ( "IOCTL : Send Vlan Add CMD Fail.\n" );
		return 0;
	}
	
	close(fd);
	return ;
}		
/* -----  end of function bcm_mng_vlan_add  ----- */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  bcm_mng_vlan_del
 *  Description: 
 *       Author:  LIUyk
 * =====================================================================================
 */
int
bcm_mng_vlan_del ( int unit, int port, int vid, int pri )
{
	int fd = -1;
	char dev_name[] = "dev/bcm_chip_drv";
	Drv_vlan_param vlan_param;

	if( (fd = open( dev_name, O_RDWR )) == -1 ) {
   		printf( "Cann't open device : %s\n", dev_name ); 
		return (-1);
   	}

	memset(&vlan_param, 0, sizeof(Drv_vlan_param));	
	vlan_param.vlan = vid;
	vlan_param.port = port;

	if ( 0 > ioctl( fd, DRV_CMD_VLAN_DEL, (unsigned long)(&vlan_param) ) ) {
		close(fd);
		printf ( "IOCTL : Send Vlan Add CMD Fail.\n" );
		return 0;
	}
	
	close(fd);

	return 0;
}		
/* -----  end of function bcm_mng_vlan_del  ----- */



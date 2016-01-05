/*
 *
 * @file		temperature.c
 * @author	Tianzhy <tianzy@huahuan.com>
 * @date		2013-8-15
 * @modif   zhangjj<zhangjj@huahuan.com>
 */
#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "api.h"

#define I2C_RETRIES 0x0701
#define I2C_TIMEOUT 0x0702
#define I2C_RDWR 0x0707 

struct i2c_msg
{
	 unsigned short addr;
	 unsigned short flags;
	#define I2C_M_TEN 0x0010
	#define I2C_M_RD 0x0001
	 unsigned short len;
	 unsigned char *buf;
};

struct i2c_rdwr_ioctl_data
{
	 struct i2c_msg *msgs;
	 int nmsgs; 

};

/*********************************
  *
  *return value:
  * -1: error
  *  ( 0 -+125 ) * 10
  *  (-0.5 - -55) * 10
  *
  *
  *********************************/
int  getTemperature(short *temperature)
{
	 int fd,ret;
	 struct i2c_rdwr_ioctl_data e2prom_data;
	 short temp = 0;
	 float ftemp = 0.0;
	 
	 fd=open("/dev/i2c-0",O_RDWR);
	 if(fd<0)
	 {
	 	 cdebug("open /dev/i2c-0 error\n");
		 return(-1);
	  }
	  
	 e2prom_data.nmsgs=2; 

	 e2prom_data.msgs=(struct i2c_msg*)malloc(e2prom_data.nmsgs*sizeof(struct i2c_msg));
	 if(!e2prom_data.msgs)
	 {
		  cdebug("malloc error\n");
		  close(fd);
		  return(-1);
	 }
	 
	 ioctl(fd,I2C_TIMEOUT,1);
	 ioctl(fd,I2C_RETRIES,2);
	 
	 /******read
	  * data
	  * from
	  * e2prom*******/

	 e2prom_data.nmsgs=2;
	 (e2prom_data.msgs[0]).len=1; 
	 (e2prom_data.msgs[0]).addr=0x4a;  
	 (e2prom_data.msgs[0]).flags=0;
	 (e2prom_data.msgs[0]).buf=(unsigned char*)malloc(10);
	 (e2prom_data.msgs[0]).buf[0]=0;
	 (e2prom_data.msgs[1]).len=2;
	 (e2prom_data.msgs[1]).addr=0x4a;
	 (e2prom_data.msgs[1]).flags=I2C_M_RD;
	 (e2prom_data.msgs[1]).buf=(unsigned char*)malloc(10);
	 (e2prom_data.msgs[1]).buf[0]=0;
	 (e2prom_data.msgs[1]).buf[1]=0;
	 ret=ioctl(fd,I2C_RDWR,(unsigned long)&e2prom_data);
	 if(ret<0)
	 {
	   	cdebug("ioctl error\n");
		close(fd);
		return(-1);
	 }
	 close(fd);

	temp = ((e2prom_data.msgs[1].buf[0])  << 8 )  | (e2prom_data.msgs[1]).buf[1];
	//printf("%02x",(e2prom_data.msgs[1]).buf[0]);
 	//printf("%02x\n",(e2prom_data.msgs[1]).buf[1]);
#if 0
	temp = temp >> 7;
	

	if(temp & 0x100)
	{
		temp = 0x1ff - temp +1;
		temp = (-(temp * 5));
		
	}
	else temp = temp * 5;
#else
	ftemp = (float)temp /256.0*10;
	temp = (short)ftemp;
	*temperature = temp;
#endif	
	free((e2prom_data.msgs[0]).buf);
	free((e2prom_data.msgs[1]).buf);
	free(e2prom_data.msgs);	
	
	return(temp) ;
}
#if 0
int main()
{
	short temp = 0;
	getTemperature(&temp);
	printf("temperature = %d.%d\n",temp/10,temp%10);
	return 0;
}
#endif

/*
 * @file	sysled.c
 * @author	<zhangjj@huahuan.com>
 * @date	2016-1-5
 * @modif   
 */
#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <math.h>
#include "api.h"
#include <string.h>

#define RUN_LED		9
/*type       0: ALM    1: BUSY*/
/*led_color  0: off    1: on */
int setAlarmLed(int led_type,int led_color)
{
        int fd, type;
        struct fpga_msg msg;
	
	type = led_type? 3:2;
        fd=open("/dev/spidev0.0",O_RDWR);
        if(fd<0)
        {
                 cdebug("open /dev/spidev0.0 error\n");
                 return(-1);
        }
        if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
        {
                cdebug("ioctl SPI_IOC_OPER_FPGA error\n");
        }
        memset(&msg, 0, sizeof(msg));
        msg.addr = 0x15;
        msg.len = 4;
	if(led_color)
		msg.buf[3] &= ~(1 << type);
	else
		msg.buf[3] |= 1 << type;
        write_fpga_data(fd,&msg,0);

        if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
        {
                cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
        }
        close(fd);

	return 0;
}
int sysLedInit()
{
        char setCmd[100]={0};
	
	sprintf(setCmd, "echo %d > /sys/class/gpio/export",RUN_LED);
	system(setCmd);

	memset(setCmd, 0, sizeof(setCmd));
	sprintf(setCmd, "echo \"out\" > /sys/class/gpio/gpio%d/direction",RUN_LED);
	printf("VALUE:%s\n",setCmd);
	system(setCmd);

}
/*0 off, 1 on*/
int setRunLed( int led_color)
{
        char setCmd[100]={0};
	char onoff = 0;

	onoff = led_color? 0:1;	

	memset(setCmd, 0, sizeof(setCmd));
	sprintf(setCmd, "echo %d > /sys/class/gpio/gpio%d/value",onoff,RUN_LED);
	system(setCmd);
	
	return 0;
}

int main(void)
{
	sysLedInit();
	while(1)
	{
		setAlarmLed(0,0);
		setAlarmLed(1,0);
		setRunLed(1);
		sleep(1);
		setAlarmLed(0,1);
		setAlarmLed(1,1);
		setRunLed(0);
		sleep(1);
	}
	return 0;    
}


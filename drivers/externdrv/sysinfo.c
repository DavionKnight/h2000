/*
 * @file		sysinfo.c
 * @author	 kongjian<kongjian@huahuan.com>
 * @date		2014-4-27
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
#include "sfpenv.h"
#include <math.h>
#include "api.h"

/**************************************** 
 * The function and defination of the LED
 * The following is not used currently, 
 * but I keep it here in case of reuse later
 ****************************************/
typedef enum E_TYPE{
    LED_TYPE_NONE = 0,
    LED_TYPE_MINOR = 1,
    LED_TYPE_MAJOR = 2,
    LED_TYPE_ALL = 3
}e_type;
typedef enum E_COLOR{
    LED_COLOR_NOT_CHANGE = 0,
    LED_COLOR_DARK = 1,
    LED_COLOR_GREEN = 2,
    LED_COLOR_RED = 3
}e_color;


/* The register address of the system led and alarm led*/
#define LED_SYS_ADDR 0x16
#define LED_ALM_ADDR 0x18
/***************************************************************
 * 设置告警指示灯
 *     led_type: 1: 次要告警  	2:重大告警
 * 	led_color: 1: dark      2:green or red
 *返回值:  0 正常返回， -1 异常返回
 ***************************************************************/
int setAlarmLed(int led_type, int led_color)
{
    int fd;
    static int led_alarm_value = -1;
    struct fpga_msg led_alarm_data;
    if(led_color == led_alarm_value)
    {
        cdebug("Alarm led need no change\n");
        return 0; 
    }
    led_alarm_value = led_color;

    fd=open(FPGADRVDIR,O_RDWR);

    if(fd<0)
    {
        cdebug("open /dev/fpga error1\n");
        return(-1);
    }
	if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
	{
	}
    led_alarm_data.addr = LED_ALM_ADDR;
    led_alarm_data.len = 2;
    led_alarm_data.buf[0] = 0x00;
    led_alarm_data.buf[1] = (unsigned char)led_alarm_value;

    lseek(fd, led_alarm_data.addr, SEEK_SET);

    if (write(fd, led_alarm_data.buf, led_alarm_data.len) == led_alarm_data.len)
    {
        cdebug("To set the alarm led color\n ");
    }else{
        cdebug("Error to write alarm led value");

		if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
		{
		}
        close(fd);
        return (-1);
    }
	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
	}

    close(fd);
    return 0;
}
/***************************************************************
 * 运行指示灯
 *		led_color: 1: dark			2:green or red
 *返回值:  0 正常返回， -1 异常返回
 ***************************************************************/
int setNorLed( int led_color)
{
    int fd;
    static int led_normal_value= -1;
    struct fpga_msg led_normal_data;

    if(led_color == led_normal_value)
    {
        cdebug("Normal led color need no change\nn");
        return 0; 
    }
    led_normal_value = led_color;
    fd=open(FPGADRVDIR,O_RDWR);

    if(fd<0)
    {
        cdebug("open /dev/fpga error\n");
        return(-1);
    }
	if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
	{
	}

    led_normal_data.addr = LED_SYS_ADDR;
    led_normal_data.len = 2;
    led_normal_data.buf[0] = 0x00;
    led_normal_data.buf[1] = (unsigned char)led_normal_value;

    lseek(fd, led_normal_data.addr, SEEK_SET);

    if (write(fd, led_normal_data.buf, led_normal_data.len) == led_normal_data.len)
    {
        cdebug("To set the normal led color\n ");
    }else{
        cdebug("Error to write led normal value\n");
		if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
		{
		}

        close(fd);
        return (-1);
    }
	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
	}

    close(fd);
    return 0;
}

/*
   int main(void)
   {
   setAlarmLed(1,1);
   setNorLed(1);
   setAlarmLed(1,1);
   setNorLed(0);
   return 1;    
   }
   */

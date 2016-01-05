/*
 * @file		fan.c
 * @author	    kongjian<kongjian@huahuan.com>
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
#include <string.h>
#include <errno.h>

#include "api.h"


#define FAN1_SPEED_ADDR     0xc     //register address to read fan1 speed
#define FAN2_SPEED_ADDR     0x000e     //register address to read fan2 speed
#define FAN_SPEED_THRESH    4000   //added by lidingcheng 2015-02-04 for fan alarm

#define FAN_PLUG_IN_ADDR 0xb   //added by zhangjj 2015-12-29 

#define FAN_OPER_STATUS_ADDR 0x14  //added by zhangjj 2015-12-29 

//#define FAN_STATE_ADDR	    0x68 
#if 0
#define FAN_ENABLE_ADDR     0x6d    // fan enable
#else
#define FAN_ENABLE_ADDR     0x14    // zjj 2015-12-29
#endif
#if 0
#define FAN_LED_ADDR     0x70    // fan led
#else
#define FAN_LED_ADDR     0x6e    // fan led
#endif

#if 0
void pdata(unsigned char *pdata, int count)
{	

    int i;
    for (i = 0; i < count; i++)
    {	
        printf(" %02x", pdata[i]);	
    }
}

/* read data from fpga */
int read_fpga_data(int fd, struct fpga_msg *msg, int step)
{
    if(fd<0)
        return (-1);
    lseek(fd, (*msg).addr, SEEK_SET);
    cdebug("step %d begin === read %d bytes at 0x%04x:", step, (*msg).len, (unsigned short)(*msg).addr);
    if (read(fd, (*msg).buf, (*msg).len) == (*msg).len) {
        pdata((*msg).buf, (*msg).len);
        cdebug(" \nstep %d done\n", step);
    } else {
        cdebug(" \nstep %d error\n", step);
        return (-1);
    }
    return 1;
}

#endif 


/***************************************************************
 * 获取风扇状态(0: ok ;  1:fail)
 *
 *返回值:  0 正常返回， -1 异常返回
 *
 ***************************************************************/

int  getFanHwState(void)      //There is only one fan module
{
    struct fpga_msg fan_state_msg;

    int fd = open(FPGADRVDIR, O_RDWR);
    if (fd < 0)
    {
        cdebug("open /dev/fpga is error");
        return (-1);
    }
	if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA error\n");
	}

    //fan_state_msg.addr = FAN_STATE_ADDR;
    fan_state_msg.addr = FAN_PLUG_IN_ADDR ;
    fan_state_msg.len = 4;

    lseek(fd, fan_state_msg.addr, SEEK_SET);
    if (read(fd, fan_state_msg.buf, fan_state_msg.len) == fan_state_msg.len)
    {
        cdebug("To get the fan Hw state");
    }
	else
    {
        cdebug("Error to get the fan state");
		if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
		{
			cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
		}
        close(fd);
        return (-1);
    }
	
	cdebug("In getFanHwState, register is: \n");

	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
	}

	close(fd);
	return (fan_state_msg.buf[3] & 0x01) ;

}

int  getFanRunState(void)      //There is only one fan module
{
    struct fpga_msg fan_state_msg;

    int fd = open(FPGADRVDIR, O_RDWR);
    if (fd < 0)
    {
        cdebug("open /dev/spidev0.0 is error");
        return (-1);
    }
	if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA error\n");
	}

    //fan_state_msg.addr = FAN_ENABLE_ADDR;
    fan_state_msg.addr = FAN_OPER_STATUS_ADDR;
    fan_state_msg.len = 4;

    lseek(fd, fan_state_msg.addr, SEEK_SET);
    if (read(fd, fan_state_msg.buf, fan_state_msg.len) == fan_state_msg.len)
    {
        cdebug("To get the fan run state");
    }
	else
	{
        cdebug("Error to get the fan state");
		if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
		{
			cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
		}
        close(fd);
        return (-1);
    }

    cdebug("In getFanRunState, register is: \n");
	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
	}

    close(fd);
//    printf("(fan_state_msg.buf[1] & 0x01) %d\r\n" ,(fan_state_msg.buf[1] & 0x01));
   return  (fan_state_msg.buf[3] & 0x01) ;

}


/********************************
 *********************************/
int getFanSpeed(unsigned short *fan)
{

    int i, fd , ret = 0;
    struct fpga_msg fan_fpga_msg[2];

    fd=open(FPGADRVDIR,O_RDWR);

    if(fd<0)
    {
        cdebug("open /dev/spidev0.0 error\n");
        return(-1);
    }
	if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA error\n");
	}

    fan_fpga_msg[0].addr = FAN1_SPEED_ADDR;
    fan_fpga_msg[1].addr = FAN2_SPEED_ADDR;

    for(i=0; i<2; i++)
    {
        fan_fpga_msg[i].len = 4;
        ret = read_fpga_data(fd, (struct fpga_msg*)&fan_fpga_msg[i], 0) ;
		if (ret>0)	
        {
            	cdebug("read the %d fan successfully", i);
            	fan[i] = fan_fpga_msg[i].buf[2]<<8|fan_fpga_msg[i].buf[3];
		fan[i] /= 60; 
	    	cdebug("The fan[%d] is : %d\n", i, fan[i]);
        }
	else
	{
		cdebug("read the %d fan unsuccessfully", i);
        	fan[i] = 0;        // Rethink this value !!!

		if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
		{
			cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
		}
            	close(fd);
        }
    }

	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
	}
    close(fd);

    return 0;
}
/*****
*  Enable fan edit by lidingcheng2015-05-12 for on-off fan
*	1-enable,0-disable
*/
int enableFan(const short on_off)
{
  int fanState, ret = 0;
  unsigned char data[4]={0};
  if (on_off==1)
	{
	  data[2] = 0x00;
	  data[3] = 0x00;
	}
	else
	{
	    data[2] = 0xff;
	    data[3] = 0xff;
	}

	int fd = open(FPGADRVDIR, O_RDWR);
	if (fd < 0) 
    	{
      		cdebug("open /dev/spidev0.0 is error") ;
      		return (-1);
    	}
	if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA error\n");
	}
	lseek(fd, FAN_ENABLE_ADDR, SEEK_SET);
	if(!(write(fd, data, 4) == 4))
	{
		cdebug("Write to enable FAN error!\n");
		ret = -1;
		goto END;
	}

END:
	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
	}
    close(fd);
    return ret;
}
/*
*1-on,0-off
*/
int setFanRunLed(int runStatus)
{
	int ret = 0 ;
     	unsigned char data[2];
	data[0] = 0x00;
	if (runStatus==0)
	  	data[1] = 0x00;
	else
	  	data[1] = 0x01;
	
	int fd = open(FPGADRVDIR, O_RDWR);
	if (fd < 0)
	{
		cdebug("open /dev/spidev0.0 is error");
    	return (-1);
	}
	if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA error\n");
	}

	lseek(fd, FAN_LED_ADDR, SEEK_SET);
	ret = write(fd , data , 2) ;
	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA error\n");
	}
   	close(fd);
	return 0;
}

int getFanState(unsigned short *fan)
{
  if(fan == NULL)
  {
    cdebug("Para pointer is NULL in function getFanState\n");
    return (-1);
  }
  //fan[0] = getFanRunState(); //edit by lidingcheng for plugin
  fan[0] =  getFanHwState();

  if(fan[0]==1 || fan[0]==0)
  	return 0;
  else
  	return -1;
}

int getFanInfo(int index, struct sdm_ce_fan_running_info *fan)
{
  int is_raise_alarm =0 ;
  unsigned short fan_speed[2];
  cdebug("Start fan getFanInfo\n");
  if(index != 1)    // For 1610, There are one fan module with 3 subunit 
  {
    cdebug("Error Fan index %d. \n", index);
    return (-1);
  }

  //if(NULL == fan || NULL == fan[0].SubUnitHwState || NULL == fan[0].RunSpeed)
  if(NULL == fan)
  {
    cdebug("The parameter in function getFanInfo is NULL\n");
    return (-1);
  }
  memset(fan, 0, sizeof(struct sdm_ce_fan_running_info));
  if(getFanSpeed(fan_speed)<0)
  {
  	 cdebug("Can not get the fan speed\n");
  	return (-1);
  }
  fan[0].RunSpeed[0] = fan_speed[0];
  if (fan_speed[0]<=FAN_SPEED_THRESH)
	is_raise_alarm = 1 ;
  fan[0].RunSpeed[1] = fan_speed[1];
  if (fan_speed[1]<=FAN_SPEED_THRESH)
	is_raise_alarm = 1 ;  

//  cdebug("The fan speed in function getFanInfo is: \n");
//  pdata((unsigned char *)fan[0].RunSpeed,4);
//  setFanRunLed(is_raise_alarm);
  return 0;
}

#if 0
int main()
{
	struct sdm_ce_fan_running_info fan={0};
	unsigned short state = 0, ret = 0;
	enableFan(1);
	sleep(2);

	ret = getFanState(&state);
printf("state = %d\n",state);	
 	getFanInfo(1, &fan);
	printf("fan speed:%x   %x\n",fan.RunSpeed[0],fan.RunSpeed[1]);
//	enableFan(0);
	return 0;
}
#endif


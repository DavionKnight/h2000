/*
 * @file		power.c
 * @author	kongjian <kongjian@huahuan.com>
 * @date		2014-04-22
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
#include <tgmath.h>
#include <string.h>
#include "api.h"

#define FPGA_POWER1_PLUGGED     0x070  // power1 module register  
#define FPGA_POWER2_PLUGGED     0x072  // power1 module register  

#define FPGA_POWER1_ADDR         0xb0
#define FPGA_POWER2_ADDR         0xc0
#if 0
#define FPGA_POWER1_PLUGGED     0x074  // power1 module register  
#define FPGA_POWER2_PLUGGED     0x076  // power1 module register  

#define FPGA_POWER1_ADDR         0x100
#define FPGA_POWER2_ADDR         0x110
#endif


#define FPGA_POWER2_OUT_ON_OFF         0x07b  //added by lidingcheng 2015-05-13

int power_plug[2];

#if TEST
/* print the data, used for debugging */
void pdata(unsigned char *pdata, int count)
{	
#ifdef DEBUG
    int i;
    for (i = 0; i < count; i++)
    {	
        printf(" %02x", pdata[i]);	
    }
#else
#endif
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
/*write data to fpga */
int write_fpga_data(int fd, struct fpga_msg *msg, int step)
{
    if(fd < 0) 
        return (-1);
    lseek(fd, (*msg).addr, SEEK_SET);
    cdebug("step %d begin ==== write %d bytes at 0x%04x:", step, (*msg).len, (unsigned short)(*msg).addr);
    if (write(fd, (*msg).buf, (*msg).len) == (*msg).len){
        pdata((*msg).buf, (*msg).len);
        cdebug("\nstep %d done\n", step);
    } else{
        cdebug(" step %d error\n", step);
		close(fd);
        return (-1);
    }
    return 1;
}
#endif 

/*Check if the power module is plugged */
int getPowerHwState(int number)
{
    int fd, ret = 0;
    //unsigned int flag_plugged = 0x01;       // The last 4 bits.  1: unplugged, 0: plugged. 
    struct fpga_msg is_plugged_msg;

    if((number < 1) || (number>2))
		return(-1);
    fd=open(FPGADRVDIR,O_RDWR);
    if(fd<0)
    {
        cdebug("open /dev/fpga error1\n");
        return(-1);
    }
	if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA error\n");
	}

    is_plugged_msg.addr =(unsigned int) ((number==1)?FPGA_POWER1_PLUGGED:FPGA_POWER2_PLUGGED);
    is_plugged_msg.len = 2;

    lseek(fd, is_plugged_msg.addr, SEEK_SET);
    if (read(fd, is_plugged_msg.buf, is_plugged_msg.len) == is_plugged_msg.len)
    {
        cdebug("To check if the power module %d is inserted\n ", number);
        pdata(is_plugged_msg.buf,is_plugged_msg.len); 
    }else{
        cdebug("Can not read fpga when check if power module ");

		ret = -1;
		goto END;
    }
	power_plug[number-1] = is_plugged_msg.buf[1] & 0x07;
    cdebug("\nThe power module %d is plugged",number);

END:
	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
	}

    close(fd);
    return (ret == 0?power_plug[number-1]:ret);
}


/* read the power module infomation */
int getVoltageInfo(int number)
{
    int fd,ret,i,j;
    int delay_count = 10;
    unsigned short power_data[2] = {0};
    unsigned int addr_power_slave;
    unsigned int addr_power;
    unsigned short tmp;
    unsigned char temp[2];
    unsigned short valid_data =0;
	int count = 0xfffff;
    int voltage;
    struct fpga_rdwr_data fpga_data;
    struct fpga_rdwr_data power_control_register;


    // Check whether the power module is plugged
    ret = getPowerHwState(number) ;
    if((ret == 7 ) || (ret == -1)) 
    {
        cdebug("The power module is not plugged");
	 //printf("The power %d is not plugged!!!!\n", number);
        return (-1);
    }
    fd=open(FPGADRVDIR,O_RDWR);

    if(fd<0)
    {
        cdebug("open /dev/fpga error\n");
        return(-1);
    }

	if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
	{
	}

    // The base address in the register
    switch(number)
    {
        case 1: 
            addr_power= FPGA_POWER1_ADDR;
			valid_data = FPGA_POWER1_ADDR+FPGA_EN_OFFSET;
            break;
        case 2:
            addr_power= FPGA_POWER2_ADDR;
			valid_data = FPGA_POWER2_ADDR+FPGA_EN_OFFSET;
            break;
        default:
            cdebug("Error of the power index");

			if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
			{
			}
			close(fd);
            return (-1);
    }

	cdebug("addr_power is 0x%x\n",addr_power);
	cdebug("valid_data is %x\n",valid_data);

    power_control_register.nmsgs = 4;
    power_control_register.msgs = (struct fpga_msg*)malloc(power_control_register.nmsgs * sizeof(struct fpga_msg));
    if(NULL == power_control_register.msgs)
    {
        cdebug("power_control_register is not allocated successfully");

			if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
			{
			}

		close(fd);
        return (-1);
    }

    // write 114 ce06 
    power_control_register.msgs[0].addr = addr_power + FPGA_ADDR_OFFSET;
    power_control_register.msgs[0].flags = 1; // write
    power_control_register.msgs[0].len = 2;
    power_control_register.msgs[0].buf[0] = 0xce;
    power_control_register.msgs[0].buf[1] = 0x06;
    // write 116 0020
    power_control_register.msgs[1].addr = addr_power + FPGA_WRDATA_OFFSET;
    power_control_register.msgs[1].flags = 1; // write
    power_control_register.msgs[1].len = 2;

    //read 116 to check bits[6:5] = 01
//    read_fpga_data(fd, (struct fpga_msg*)&power_control_register.msgs[1],1);
//    if((power_control_register.msgs[1].buf[1]&0x60)^(0x20))    //if 06H bits[6:5] = 01, continue, or set to 0020
 //   {
        power_control_register.msgs[1].buf[0] = 0x00;
        power_control_register.msgs[1].buf[1] = 0x20;

        //read 110 2 
        //power_control_register.msgs[2].addr = addr_power + FPGA_EN_OFFSET; 
        power_control_register.msgs[2].addr = addr_power; 
        power_control_register.msgs[2].len = 2;
        power_control_register.msgs[2].flags = 0; // read 

        //wrtie 110 0001(0000)  产生跳变沿
        power_control_register.msgs[3].addr = addr_power;
        power_control_register.msgs[3].flags = 1; // write
        power_control_register.msgs[3].len = 2;

        //write a4 a000
        write_fpga_data(fd,(struct fpga_msg*)&power_control_register.msgs[0],1);

	 temp[1] = 0;
        count = 0xfffff;
        while(!temp[1])
        {
            lseek(fd, valid_data, SEEK_SET);
            read(fd, temp,2);
            count --;
	     if(count == 0) break;
        }

        write_fpga_data(fd,(struct fpga_msg*)&power_control_register.msgs[1],2);
	 temp[1] = 0;
        count = 0xfffff;
        while(!temp[1])
        {
            lseek(fd, valid_data, SEEK_SET);
            read(fd, temp,2);
            count --;
	     if(count == 0) break;
        }		
        //read a2 2 	
        read_fpga_data(fd, (struct fpga_msg*)&power_control_register.msgs[2],3);
	 temp[1] = 0;
        count = 0xfffff;
        while(!temp[1])
        {
            lseek(fd, valid_data, SEEK_SET);
            read(fd, temp,2);
            count --;
	     if(count == 0) break;
        }		
        //wrtie a2 0001 
        power_control_register.msgs[3].buf[0] = 0x00;
        power_control_register.msgs[3].buf[1] = power_control_register.msgs[2].buf[1]^(0x01);		//跳变沿有效
        write_fpga_data(fd, (struct fpga_msg*)&power_control_register.msgs[3], 4);
	 temp[1] = 0;
        count = 0xfffff;
        while(!temp[1])
        {
            lseek(fd, valid_data, SEEK_SET);
            read(fd, temp,2);
            count --;
	     if(count == 0) break;
        }
        //delay to wait writing the data
//edited by lidincheng 2014-07-07        
		//usleep(400000);	
//lidingcheng edited end 2014-07-07		
  //  }
    cdebug("To check the register 6:5 = 01");
    pdata(power_control_register.msgs[1].buf,2);

    // To read the data of the power
    fpga_data.nmsgs = 5;
    fpga_data.msgs = (struct fpga_msg*)malloc(fpga_data.nmsgs * sizeof(struct fpga_msg));
    if(NULL == fpga_data.msgs)
    {
        cdebug(" fpga_data.msgs is not allocated correctly!\n ");

		if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
		{
		}
		
		close(fd);
        return (-1);
    }

    // write a4 a000
    fpga_data.msgs[0].addr = addr_power + FPGA_ADDR_OFFSET;
    fpga_data.msgs[0].flags = 1; // write
    fpga_data.msgs[0].len = 2;
    fpga_data.msgs[0].buf[0] = 0xce;

    //read a2 2 
    fpga_data.msgs[1].addr = addr_power + FPGA_RDEN_OFFSET; 
    fpga_data.msgs[1].len = 2;
    fpga_data.msgs[1].flags = 0; // read 

    //wrtie a2 0001(0000)  产生跳变沿
    fpga_data.msgs[2].addr = addr_power + FPGA_RDEN_OFFSET;
    fpga_data.msgs[2].flags = 1; // write
    fpga_data.msgs[2].len = 2;

    //read aa 2 
    fpga_data.msgs[3].addr = addr_power + FPGA_EN_OFFSET;
    fpga_data.msgs[3].flags = 0; // read
    fpga_data.msgs[3].len = 2;

    //read a8 2 
    fpga_data.msgs[4].addr = addr_power + FPGA_RDDATA_OFFSET;
    fpga_data.msgs[4].flags = 0; // read
    fpga_data.msgs[4].len = 2;

    for(i=0; i<2; i++)   //read the data of the register(0~93)
    {
        fpga_data.msgs[0].buf[1] = (unsigned char)(0x02 + i);
        cdebug("\n+++++++%d+++++++",i);
        //write a4 a000
        write_fpga_data(fd,(struct fpga_msg*)&fpga_data.msgs[0],1);
	 temp[1] = 0;
        count = 0xfffff;
        while(!temp[1])
        {
            lseek(fd, valid_data, SEEK_SET);
            read(fd, temp,2);
            count --;
	     if(count == 0) break;
        }		
        //read a2 2 	
        read_fpga_data(fd, (struct fpga_msg*)&fpga_data.msgs[1],2);
        //wrtie a2 0001 
        fpga_data.msgs[2].buf[0] = 0x00;
        fpga_data.msgs[2].buf[1] = fpga_data.msgs[1].buf[1]^(0x01);		//跳变沿有效
        write_fpga_data(fd, (struct fpga_msg*)&fpga_data.msgs[2], 3);
	 temp[1] = 0;
        count = 0xfffff;
        while(!temp[1])
        {
            lseek(fd, valid_data, SEEK_SET);
            read(fd, temp,2);
            count --;
	     if(count == 0) break;
        }
        // To wait for the enable flag 
        delay_count = DELAY_COUNT;    // In fact, it needs 3 times to wait for the readable flag

        do
        {
            //read aa 2 
           // usleep(100000);
            read_fpga_data(fd, (struct fpga_msg*)&fpga_data.msgs[3], 4);
            if((unsigned char)fpga_data.msgs[3].buf[1] & 0x01)
                break;
        }while(delay_count--);
        cdebug("\nwait time is +++%d+++++",delay_count);
        // check if the read flag is enabled 
        if((unsigned char)fpga_data.msgs[3].buf[1] & 0x01)
        {
            read_fpga_data(fd, (struct fpga_msg*)&fpga_data.msgs[4], 5);
            power_data[i] = fpga_data.msgs[4].buf[1];
        }else{
            cdebug("FPGA is not RDWR ENABLE!! \n");

			if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
			{
			}
	
            close(fd);
            free(fpga_data.msgs);
            free(power_control_register.msgs);
            return (-1);
        }
    }
	
	  cdebug("Powerdata[0] is <<<<%02x>>>>", power_data[0]);
  	cdebug("Powerdata[1] is <<<<%02x>>>>", power_data[1]);

    tmp = (power_data[0]<<4)|((power_data[1]>>4)&0x0f);
    voltage = (tmp * 25)/1000;
    cdebug("After calculation: %d", voltage);	
    //printf("power %d voltage is  %d\n", number,voltage);	
	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
	}

    close(fd);
    
    free(fpga_data.msgs);
    free(power_control_register.msgs);

    return(voltage) ;
}

#if 1
/***************************************************************
 * 获取电源状态(0: ok ;  1:fail)
 *bit15-8                                7-0
 *Power B state			Power A state
 *返回值:  0 正常返回， -1 异常返回

 b"000"	直流电源盘（-48V/12V）75W，无电源监控
b"001"	直流电源盘（-48V/12V）75W，带电源监控
b"010"	reserve
b"011"	reserve
b"100"	交流电源盘（~220V/12V）75W，无电源监控
b"101"	交流电源盘（~220V/12V）75W，带电源监控
b"110"	reserve
b"111"	不在位

 ***************************************************************/
int  getPowerState(unsigned short *pwrState)
{
  if(pwrState == NULL)
  {
    cdebug("pwrState pointer is NULL\n");
    return (-1);
  }
  pwrState[0] = getPowerHwState(1);
  pwrState[1] = getPowerHwState(2);
  //pwrState[0] = power_plug[0];
  //pwrState[1] = power_plug[1];  
  //printf("----------power[1] plugin [%d] power[2] plugin [%d]\r\n", pwrState[0]  , pwrState[1] );
  return 0;
}


//added by lidingcheng 2015-05-13 for power out status
int getPowerOn(unsigned short *pwrOn)
{
    int fd;    
    struct fpga_msg is_plugged_msg;

    fd=open(FPGADRVDIR,O_RDWR);
    if(fd<0)
    {
        cdebug("open /dev/fpga error1\n");
        return(-1);
    }    
	if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
	{
	}


    is_plugged_msg.addr = FPGA_POWER2_OUT_ON_OFF ;
    is_plugged_msg.len = 2;

    lseek(fd, is_plugged_msg.addr, SEEK_SET);
    if (read(fd, is_plugged_msg.buf, is_plugged_msg.len) == is_plugged_msg.len)
    {
        cdebug("To check if the power out  is On_Off\n ");
        pdata(is_plugged_msg.buf,is_plugged_msg.len); 
    }else{
        cdebug("Can not read fpga when check if power module \r\n");
		if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
		{
		}
		close(fd);
        return (-1);
    }

    //return (0);
    memcpy(pwrOn , is_plugged_msg.buf , 2) ;
   
    //printf("The power out status [%4x]\r\n",*pwrOn);

	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
	}
    close(fd);
}
//lidingcheng add end


/*
typedef struct _sdm_ce_power_running_info
{
	int             PowerID; 
	int             HwState;
	int 		VoltageIn ;
	int    	IntensityIn ;
	int 		VoltageOut ;
	int    	IntensityOut ;
} sdm_ce_power_running_info ;
*/
int  getPowerInfo(int index, struct sdm_ce_power_running_info *pwrInfo)
{
    if(pwrInfo == NULL)
    {
       cdebug("The pwrState is NULL \n");
        return (-1);
    }
    if(index <1 || index >2 )
    {
      cdebug(" The index of the Power module exceeds the ranger\n");
      return (-1);
    }

    //memset((struct sdm_ce_power_running_info *)&pwrInfo[0], 0 , sizeof(struct sdm_ce_power_running_info));

    //pwrInfo[0].HwState = getPowerHwState(index);
    //pwrInfo[0].VoltageIn = (int)getVoltageInfo(index);

    memset((struct sdm_ce_power_running_info *)pwrInfo,0 , sizeof(struct sdm_ce_power_running_info));

    pwrInfo->HwState = getPowerHwState(index);
    //pwrInfo->HwState = 0 ;
	
    //pwrInfo->HwState = 220000 ;	
    pwrInfo->VoltageIn = (int)getVoltageInfo(index);
    return 0;
}
#endif

#if 0
int main(int argc, char * argv[])
{
  //struct sdm_ce_power_running_info powerinfo;
  int ret;
  while(1){
  ret = getVoltageInfo(1);
  ret = getVoltageInfo(2);
  usleep(1000000);
}
  //printf("The voltage is %d\n", powerinfo.VoltageIn);
  return 1;

}
#endif


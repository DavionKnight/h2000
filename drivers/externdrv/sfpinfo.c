/*
 * @file		sfpinfo.c
 * @author	kongjian <kongjian@huahuan.com>
 * @date		2014-04-15
 * @modif   zhangjj<zhangjj@huahuan.com>
 */

#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "sfpenv.h"
#include <tgmath.h>
#include <string.h>
#include "sfpenv.h"
#include "api.h"
#include <sys/time.h>

#define FPGA_SFPXFP_PLUGGED 	0x20;  // 光模块是否在位寄存器地址
#define FPGA_SFP_PLUGGED 		0x22;  // 光模块是否在位寄存器地址


#define SFP 3

#define SLAVE_ADDR_SFP        0xa0;   //start register address of Port1,  Port2: 0xb0, Port3: 0xc0, Port4: 0xd0
#define FPGA_WREN_OFFSET  		0x00;   // write enable , flap  is valid
#define FPGA_RDEN_OFFSET  		0x02;   // read enable,  flap is valid
#define FPGA_ADDR_OFFSET  		0x04;   // set the slave address and reg address
#define FPGA_WRDATA_OFFSET  	0x06;   // write data
#define FPGA_RDDATA_OFFSET  	0x08;   // read data
#define FPGA_EN_OFFSET     	    0x0a;   // read/write/write data flag


#define MAX_SXFP_INFO_OFFSET    93

#define SFPXFP_PORT_NUM 20

#if 0
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

//#if 0
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
        return (-1);
    }
    return 1;
}
#endif



/*Check if the fiber module is plugged */
int is_fiber_plugged(int fd, int port)
{
    struct fpga_msg is_plugged_msg;
	unsigned short temp = 0;
	if(port<16)
	{
    	is_plugged_msg.addr = FPGA_SFP_PLUGGED;
	}
	else
	{
    	is_plugged_msg.addr = FPGA_SFPXFP_PLUGGED;			
	}
	is_plugged_msg.len = 2;
	cdebug("Check the addr 0x%x\n ", is_plugged_msg.addr);
	lseek(fd, is_plugged_msg.addr, SEEK_SET);
	if (read(fd, is_plugged_msg.buf, is_plugged_msg.len) == is_plugged_msg.len)
	{
	cdebug("Check if a fiber modue inserted at Port %d\n ", port);
	pdata(is_plugged_msg.buf,is_plugged_msg.len); 
	}else{
	cdebug("Can not read fpga when check if fiber is plugged");
	return (-1);
	}
	temp = (is_plugged_msg.buf[0] & 0xff)<<8;
	temp += is_plugged_msg.buf[1] & 0xff;
	cdebug("\ntemp is 0x%x\n",temp);
	if((temp>> (port<16?port:(port-16)))  & 0x01 ) //if 1, then return
	{
	cdebug("\n The fiber module is not plugged\n");
	return (-1);
	}
	cdebug("\nThe fiber module is inserted at port %d",port);

	return 1;

}

/* read the sfp infomation */
int readSfpXfpInfo(int  port, char *basePtr)
{
    int fd,i;
    int delay_count = DELAY_COUNT;
    //unsigned char data[128] = {0};
    unsigned int addr_port;
    unsigned int addr_fiber_slave;
    struct fpga_rdwr_data fpga_data;
	
	cdebug("start readSfpXfpInfo....\n");

	if(basePtr == NULL)
    {
        cdebug("readSfpXfpInfo input address err\n");
        return (-1);
    }

    if(port > SFPXFP_PORT_NUM-1)
    {
        cdebug("readSfpXfpInfo input port err\n");
        return (-1);
    }

    fd=open(FPGADRVDIR,O_RDWR);

    if(fd<0)
    {
        cdebug("open %s error1\n",FPGADRVDIR);
        return(-1);
    }
	if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
	{
	}

    // Check if the fiber module is plugged 
    if(is_fiber_plugged(fd,port)  != 1 )
    {
        goto ERROR;
    }

    //.
    fpga_data.nmsgs = 5;
    fpga_data.msgs = (struct fpga_msg*)malloc(fpga_data.nmsgs * sizeof(struct fpga_msg));
    if(NULL == fpga_data.msgs)
    {
        cdebug(" fpga_data.msgs is not allocated correctly!\n ");
        goto ERROR;
    }
    // The base address in the register
    addr_fiber_slave = SLAVE_ADDR_SFP;
    //addr_port = ((addr_fiber_slave>>4)+port)<<4; /* for 161x */
    addr_port = addr_fiber_slave ;
    cdebug("\naddr_port is %02x\n ", addr_port);
    // select the port 
    fpga_data.msgs[0].addr = 0xac;
    fpga_data.msgs[0].flags = 1; // write
    fpga_data.msgs[0].len = 2;
    fpga_data.msgs[0].buf[0] = 0;
    fpga_data.msgs[0].buf[1] = port;   
    write_fpga_data(fd,(struct fpga_msg*)&fpga_data.msgs[0],1);
    
    // write a4 a000
    fpga_data.msgs[0].addr = addr_port + FPGA_ADDR_OFFSET;
    fpga_data.msgs[0].flags = 1; // write
    fpga_data.msgs[0].len = 2;
    fpga_data.msgs[0].buf[0] = addr_fiber_slave + FPGA_WREN_OFFSET;

    //read a2 2 
    fpga_data.msgs[1].addr = addr_port + FPGA_RDEN_OFFSET;
    fpga_data.msgs[1].flags = 0; // read
    fpga_data.msgs[1].len = 2;

    //wrtie a2 0001(0000)  产生跳变沿
    fpga_data.msgs[2].addr = addr_port + FPGA_RDEN_OFFSET;
    fpga_data.msgs[2].flags = 1; // write
    fpga_data.msgs[2].len = 2;

    //read aa 2 
    fpga_data.msgs[3].addr = addr_port + FPGA_EN_OFFSET;
    fpga_data.msgs[3].flags = 0; // read
    fpga_data.msgs[3].len = 2;

    //read a8 2 
    fpga_data.msgs[4].addr = addr_port + FPGA_RDDATA_OFFSET;
    fpga_data.msgs[4].flags = 0; // read
    fpga_data.msgs[4].len = 2;
#if 0
struct  timeval    tv;
struct  timezone   tz;
    fpga_data.msgs[0].addr = 0xe;
    fpga_data.msgs[0].flags = 1; // write^M
    fpga_data.msgs[0].len = 2;
    fpga_data.msgs[0].buf[0] = 1;
        gettimeofday(&tv,&tz);
 printf("time:%d.%d\n",tv.tv_sec,tv.tv_usec);
        write_fpga_data(fd,(struct fpga_msg*)&fpga_data.msgs[0],1);
        gettimeofday(&tv,&tz);
 printf("time:%d.%d\n",tv.tv_sec,tv.tv_usec);
    fpga_data.msgs[0].addr = 0;
        read_fpga_data(fd,(struct fpga_msg*)&fpga_data.msgs[0],1);
        gettimeofday(&tv,&tz);
 printf("time:%d.%d\n",tv.tv_sec,tv.tv_usec);
#endif
    for(i=0; i<MAX_SXFP_INFO_OFFSET; i++)   //read the data of the register(0~93)
    //for(i=0; i<1; i++)   //read the data of the register(0~93)
    {
        fpga_data.msgs[0].buf[1] = (unsigned char)i;
        cdebug("+++++++++%d++++++++++++",i);
        //write a4 a000

        write_fpga_data(fd,(struct fpga_msg*)&fpga_data.msgs[0],1);
        //read a2 2 	
        read_fpga_data(fd, (struct fpga_msg*)&fpga_data.msgs[1],2);
        //wrtie a2 0001 
        fpga_data.msgs[2].buf[0] = 0x00;
        fpga_data.msgs[2].buf[1] = fpga_data.msgs[1].buf[1]^(0x01);		//跳变沿有效
        write_fpga_data(fd, (struct fpga_msg*)&fpga_data.msgs[2], 3);

        // To wait for the enable flag 
        delay_count = DELAY_COUNT;    // In fact, it needs 3 times to wait for the readable flag
        do
        {
            //read aa 2 
            read_fpga_data(fd, (struct fpga_msg*)&fpga_data.msgs[3], 4);
            if((unsigned char)fpga_data.msgs[3].buf[1] & 0x01)
                break;
        }while(delay_count--);
        cdebug("wait time is +++++++++%d+++++++++++++",delay_count);
        // check if the read flag is enabled 
        if((unsigned char)fpga_data.msgs[3].buf[1] & 0x01)
        {
            read_fpga_data(fd, (struct fpga_msg*)&fpga_data.msgs[4], 5);
            basePtr[i] = fpga_data.msgs[4].buf[1];  
        }else{
            cdebug("FPGA is not RDWR ENABLE!! \n");
            //free the memory and close fd.
            free(fpga_data.msgs);
            goto ERROR;
        }
    } 
 //       gettimeofday(&tv,&tz);
 //printf("time:%d.%d\n",tv.tv_sec,tv.tv_usec);

    // The base address in the register
    //addr_fiber_slave = 0xa2;
    addr_fiber_slave = 0xa0;
    //addr_port = ((addr_fiber_slave>>4)+port)<<4;
    addr_port = addr_fiber_slave ;
    cdebug("\naddr_port is %02x\n ", addr_port);
    // write a4 a000
    fpga_data.msgs[0].addr = addr_port + FPGA_ADDR_OFFSET;
    fpga_data.msgs[0].flags = 1; // write
    fpga_data.msgs[0].len = 2;
    fpga_data.msgs[0].buf[0] = addr_fiber_slave + FPGA_WREN_OFFSET;

    //read a2 2 
    fpga_data.msgs[1].addr = addr_port + FPGA_RDEN_OFFSET;
    fpga_data.msgs[1].flags = 0; // read
    fpga_data.msgs[1].len = 2;

    //wrtie a2 0001(0000)  靠靠?
    fpga_data.msgs[2].addr = addr_port + FPGA_RDEN_OFFSET;
    fpga_data.msgs[2].flags = 1; // write
    fpga_data.msgs[2].len = 2;

    //read aa 2 
    fpga_data.msgs[3].addr = addr_port + FPGA_EN_OFFSET;
    fpga_data.msgs[3].flags = 0; // read
    fpga_data.msgs[3].len = 2;

    //read a8 2 
    fpga_data.msgs[4].addr = addr_port + FPGA_RDDATA_OFFSET;
    fpga_data.msgs[4].flags = 0; // read
    fpga_data.msgs[4].len = 2;

    for(i=96; i<128; i++)   //read the data of the register(0~93)
    {
        fpga_data.msgs[0].buf[1] = (unsigned char)i;
        cdebug("+++++++++%d++++++++++++",i);
        //write a4 a000
        write_fpga_data(fd,(struct fpga_msg*)&fpga_data.msgs[0],1);
        //read a2 2 	
        read_fpga_data(fd, (struct fpga_msg*)&fpga_data.msgs[1],2);
        //wrtie a2 0001 
        fpga_data.msgs[2].buf[0] = 0x00;
        fpga_data.msgs[2].buf[1] = fpga_data.msgs[1].buf[1]^(0x01);		//靠靠?
        write_fpga_data(fd, (struct fpga_msg*)&fpga_data.msgs[2], 3);

        // To wait for the enable flag 
        delay_count = DELAY_COUNT;    // In fact, it needs 3 times to wait for the readable flag
        do
        {
            //read aa 2 
            read_fpga_data(fd, (struct fpga_msg*)&fpga_data.msgs[3], 4);
            if((unsigned char)fpga_data.msgs[3].buf[1] & 0x01)
                break;
        }while(delay_count--);
        cdebug("wait time is +++++++++%d+++++++++++++",delay_count);
        // check if the read flag is enabled 
        if((unsigned char)fpga_data.msgs[3].buf[1] & 0x01)
        {
            read_fpga_data(fd, (struct fpga_msg*)&fpga_data.msgs[4], 5);
            basePtr[i] = fpga_data.msgs[4].buf[1];  
        }else{
            cdebug("FPGA is not RDWR ENABLE!! \n");
            //free the memory and close fd.
            free(fpga_data.msgs);
            goto ERROR;
        }
    } 

	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
	}

    close(fd);
    
    free(fpga_data.msgs);

    return(0) ;
ERROR:

	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
	}

    close(fd);
	return -1;
}

/*********************************************
 *
 *从sfp/xfp模块中直接获取光模块信息
 * for 161x: port : 5-8
 * for 0800C port : 1-8
 * for 1604c port : 1-16 sfp 17-20 xfp
 *********************************************/
int get_sfpEnvInfo_by_port_from_sxfp(int port, struct sfpxfpenvinfo_ *sfpPtr)
{
    int m_type;
    unsigned char baseData[128];
    //unsigned char  tempData=0;
    //port -= 5;
    port -= 1;
    
	cdebug("start get_sfpEnvInfo_by_port_from_sxfp\n");
	
    if(sfpPtr == NULL) 
    {
        cdebug("get_sfpEnvInfo_by_port input address err\n");
        return (-1);
    }

    if(port>SFPXFP_PORT_NUM-1 || port<0)
    {
        cdebug("The port number exceed the ranger\n");
        return (-1);
    }
    memset(baseData, 0,128);

    if(readSfpXfpInfo(port, (char*)baseData) != 0)
    {
        cdebug("read sfp/xfp error\n");
        return (-1);		
    }
    if (SFP == baseData[SFP_IDENTIFIYER])
    {
        sprintf(sfpPtr->transceiverType,"%s", "SFP"); 
    }else{
        cdebug("Unsupported Identifiyer");
        return (-1);
    }
    memcpy(sfpPtr->vendorName, &baseData[SFP_VENDOR_NAME_START],16);
    sfpPtr->vendorName[15] = '\0';
    memcpy(sfpPtr->vendorPN, &baseData[SFP_VENDOR_PN_START],16);
    sfpPtr->vendorPN[15] = '\0';
    // CONNECTOR TYPE
    switch (baseData[CONNECTOR]) 
    {
        case 0x07:
            sprintf(sfpPtr->connectorType, "%s", "LC");
            break;
        case 0x01:
            sprintf(sfpPtr->connectorType,"%s", "SC");
            break;
        default:
            sprintf(sfpPtr->connectorType,"%s", "Unspecified");
            break;
    }

    // OPTICAL TYPE
    switch (baseData[TRANCEIVER_ETHERNET]) 
    {
        case 0x80:
            sprintf(sfpPtr->opticalType, "%s", "BASE-PX"); 
            break;
        case 0x40:
            sprintf(sfpPtr->opticalType, "%s", "BASE-BX10");
            break;
        case 0x20:
            sprintf(sfpPtr->opticalType, "%s",  "100BASE-FX");
            break;
        case 0x10:
            sprintf(sfpPtr->opticalType, "%s", "100BASE-LX"); 
            break;
        case 0x08:
            sprintf(sfpPtr->opticalType, "%s", "1000BASE-T");
            break;
        case 0x04:
            sprintf(sfpPtr->opticalType, "%s", "1000BASE-CX"); 
            break;
        case 0x02:
            sprintf(sfpPtr->opticalType, "%s", "1000BASE-LX"); 
            break;
        case 0x01:
            sprintf(sfpPtr->opticalType, "%s", "1000BASE-SX");
            break;
        default:
            sprintf(sfpPtr->opticalType, "%s", "Unspecified"); 
            break;
    }
    /* LINE CODING */
    switch (baseData[ENCODING]) 
    {
        case 0x01:
            sprintf(sfpPtr->lineCoding, "%s",  "8B10B");
            break;
        case 0x02:
            sprintf(sfpPtr->lineCoding, "%s",  "4B5B"); 
            break;
        case 0x03:
            sprintf(sfpPtr->lineCoding, "%s",  "NRZ"); 
            break;
        case 0x04:
            sprintf(sfpPtr->lineCoding, "%s",   "Manchester");
            break;
        case 0x05:
            sprintf(sfpPtr->lineCoding, "%s",  "SONET Scrambled"); 
            break;
        default:
            sprintf(sfpPtr->lineCoding, "%s",  "Unspecified"); 
            break;
    }
    /* Vendor Revison */
    memcpy(sfpPtr->vendorRev, &baseData[SFP_VENDOR_REV_START],3);
    sfpPtr->vendorRev[15] = '\0';

    /* Bit Rate Nominal */
    sprintf(sfpPtr->nominalBitRate, "%d%s", ((int)baseData[BR_NOMINAL])*100 , " MBit/sec");

    /* LinkLength for 9/125 fiber */
    if (baseData[LENGTH_9M_KM] == 0 && baseData[LENGTH_9M] == 0) 
    {
        sprintf(sfpPtr->linkLength , "%s", "Not Specified");
    } else {
        if (baseData[LENGTH_9M_KM] == 0) 
        {
            sprintf(sfpPtr->linkLength , "%d%s",(int)(baseData[LENGTH_9M]) , " m");
        } else {
            sprintf(sfpPtr->linkLength , "%d%s",(int)(baseData[LENGTH_9M_KM]), " km");
        }
    } 
    /* Wave Length */
    if (baseData[SFP_WAVE_LENGTH_START] == 0 && baseData[SFP_WAVE_LENGTH_STOP] == 0) 
    {
        sprintf(sfpPtr->waveLength ,"%s",  "Not Specified");
    } else {
        sprintf(sfpPtr->waveLength ,"%d%s", ((((unsigned int)baseData[SFP_WAVE_LENGTH_START])<<8) | baseData[SFP_WAVE_LENGTH_STOP]) ,  " nm");
    }

    /* Max Bit Rate Margin */
    if(baseData[SFP_BR_MAX] == 0) 
    {
        sprintf(sfpPtr->maxBitRateMargin ,"%s", "Not Specified");
    } else {

        sprintf(sfpPtr->maxBitRateMargin ,"%d%s",  (unsigned int) baseData[SFP_BR_MAX] ," %");
    }
    /* Min Bit Rate Margin */
    if(baseData[SFP_BR_MIN] == 0) 
    {
        sprintf(sfpPtr->minBitRateMargin ,"%s", "Not Specified");
    } else {
        sprintf(sfpPtr->minBitRateMargin ,"%d%s",  (unsigned int) baseData[SFP_BR_MIN] ," %");
    }

    /* Vendor Serial Number */
    memcpy(sfpPtr->vendorSerialNo, &baseData[SFP_SERIAL_NUMBER_START],16);
    sfpPtr->vendorSerialNo[15] = '\0';

    /* Vendor Date Code */
    sprintf(sfpPtr->date, "%c%c%s%c%c%s%c%c", baseData[SFP_DATE_CODE_START+4] , baseData[SFP_DATE_CODE_START+5] , "/"
            , baseData[SFP_DATE_CODE_START+2] , baseData[SFP_DATE_CODE_START+3] , "/20" 
            , baseData[SFP_DATE_CODE_START] , baseData[SFP_DATE_CODE_START+1]);

    /* SFP Temperature*/

    if((int)baseData[SFP_TEMPERATURE_MSB] & 0x80)
    {
    	sprintf(sfpPtr->temperature, "-%d.%d", ((int)baseData[SFP_TEMPERATURE_MSB] & 0x7f),(int)baseData[SFP_TEMPERATURE_LSB]/256);
    }
    else
    	sprintf(sfpPtr->temperature, "%d.%d", (int)baseData[SFP_TEMPERATURE_MSB],(int)baseData[SFP_TEMPERATURE_LSB]/256);

    /* SFP VCC*/
    sprintf(sfpPtr->vccVoltage, "%d.%d", (((int)baseData[SFP_VCC_MSB]<<8 | (int)baseData[SFP_VCC_LSB])/10000),(((int)baseData[SFP_VCC_MSB]<<8 | (int)baseData[SFP_VCC_LSB])/10000)) ;


 /* SFP TX BIAS*/
    //sprintf(sfpPtr->TXBiasCurrent, "%d", (int)baseData[SFP_TX_BIAS_MSB]);
    sprintf(sfpPtr->TXBiasCurrent, "%.3f mA", (float)((((int)baseData[SFP_TX_BIAS_MSB]<<8 | (int)baseData[SFP_TX_BIAS_LSB]))/500));

    /* SFP TX OUTPUT*/
    sprintf(sfpPtr->TXOutputPower, "%.3f dBm", 10 *  log10((double)((int)baseData[SFP_TX_POWER_MSB]<<8 | (int)baseData[SFP_TX_POWER_LSB]) / 10000));

    /* SFP RX OUTPUT*/
    sprintf(sfpPtr->RXInputPower, "%.3f dBm", 10 *  log10((double)((int)baseData[SFP_RX_POWER_MSB]<<8 | (int)baseData[SFP_RX_POWER_LSB]) / 10000));

    return 0;
}


/*********************************************
  *
  *从缓冲器中获取光模块信息
  *
  *
  *
  *********************************************/
int get_sfpEnvInfo_by_port(int port, struct sfpxfpenvinfo_ *sfpPtr)
{

	/*
	struct sfpxfpenvinfo_ sfpinfo;
	
	if(sfpPtr == NULL) 
	{
		//cdebug("get_sfpEnvInfo_by_port input address err\n");
		return (-1);
	}

	if(port > SFPXFP_PORT_NUM)
	{
	 	//cdebug("get_sfpEnvInfo_by_port input port err\n");
		return (-1);
	}
	extern struct sfpxfpenvinfo_ *sfpinfo_table;

	memcpy(sfpPtr, sfpinfo_table[port-1], sizeof(sfpinfo));
	*/

	if(get_sfpEnvInfo_by_port_from_sxfp(port, sfpPtr) != 0)
  {
    cdebug("Error to get_sfpEnvInfo_by_port_from_sxfp()\n");
  }
	
	return 0;
	 
}

#if 0
   int main(int argc, char * argv[])
   {

   struct sfpxfpenvinfo_ *sfpPtr;
   int i ;
   cdebug("Begin-----------\n");
   sfpPtr = malloc(sizeof(struct sfpxfpenvinfo_));
   if(sfpPtr == NULL)
   {
   printf("malloc error\n");
   return ;
   }
	for(i=1;i<21;i++)
	{
   if(get_sfpEnvInfo_by_port_from_sxfp(i,sfpPtr) == 0)
   {
   printf("------------------port %d-----------------\n",i);
   printf("Vendor Name=%s\n", sfpPtr->vendorName);
   printf("Vendor PN = %s \n", sfpPtr->vendorPN);
   printf("transceiverType = %s \n", sfpPtr->transceiverType);
   printf("connector type = %s\n", sfpPtr->connectorType);
   printf("opticalType = %s\n", sfpPtr->opticalType);
   printf("lineCoding = %s\n", sfpPtr->lineCoding);
   printf("vendorRev = %s\n", sfpPtr->vendorRev);
   printf("nominalBitRate = %s\n", sfpPtr->nominalBitRate);
   printf("linkLength = %s\n", sfpPtr->linkLength);
   printf("waveLength = %s\n", sfpPtr->waveLength);
   printf("maxBitRateMargin = %s\n", sfpPtr->maxBitRateMargin);
   printf("minBitRateMargin = %s\n", sfpPtr->minBitRateMargin);
   printf("vendorSerialNo = %s\n", sfpPtr->vendorSerialNo);
   printf("date = %s\n", sfpPtr->date);
   printf("temperature = %s\n", sfpPtr->temperature);
   printf("vccVoltage = %s\n", sfpPtr->vccVoltage);
   printf("TXBiasCurrent = %s\n", sfpPtr->TXBiasCurrent);
   printf("TXOutputPower = %s\n", sfpPtr->TXOutputPower);
   printf("RXInputPower = %s\n", sfpPtr->RXInputPower);

   }else{
   printf("\n get sfp info of port %d is error\n",i);
   }
	}
   free(sfpPtr);

   return 1;

   }
#endif

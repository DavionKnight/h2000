/*
 * @file		boardinfo.c
 * @author	Tianzhy <tianzy@huahuan.com>
 * @date		2014-05-06
 * @modif   zhangjj<zhangjj@huahuan.com>
 */


#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
//#include <sys/ioctl.h>
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


int boardTypeValue;
unsigned char firstMAC[6];
unsigned char VendorName[16];
unsigned char DeviceName[16];

#define EEPROM_ADDR	0x54
#define BUFSIZE	256
unsigned char buf[BUFSIZE];
#define EEPROM_WRITE_BLOCK 16
#if 1
/*********************************************/
/*eeprom是以16字节块大小写入
/*return the writed length 
/*1604c AT24C08
/*********************************************/
int EepromWrite(unsigned short addr,unsigned char *buf, unsigned short len)
{
	int fd,ret, i = 0, j = 0;
	struct i2c_rdwr_ioctl_data e2prom_data;
	unsigned short pos = addr, leftlen = len, wrlen = 0;

	if(NULL == buf)
		return -1;
	if(addr +len > BUFSIZE)
		return -1;
	fd=open("/dev/i2c-0",O_RDWR);
	if(fd<0)
	{
		 cdebug("open /dev/i2c-0 error\n");
		 return(-1);
	}
	e2prom_data.nmsgs=1; 
	e2prom_data.msgs=(struct i2c_msg*)malloc(e2prom_data.nmsgs*sizeof(struct i2c_msg));
	
	if(!e2prom_data.msgs)
	{
		cdebug("EepromWrite malloc error\n");
		close(fd);
		return(-1);
	}

	ioctl(fd,I2C_TIMEOUT,1);
	ioctl(fd,I2C_RETRIES,2);
//	sleep(1);
	
	e2prom_data.nmsgs=1;
	(e2prom_data.msgs[0]).addr=EEPROM_ADDR;
	(e2prom_data.msgs[0]).flags=0;
	(e2prom_data.msgs[0]).buf=(unsigned char*)malloc(EEPROM_WRITE_BLOCK+1);
	memset((e2prom_data.msgs[0]).buf, 0, EEPROM_WRITE_BLOCK+1);
	while(pos < (addr + len))
	{
		cdebug("pos is 0x%x\n",pos);
		cdebug("leftlen is 0x%x\n",leftlen);
		if(pos%EEPROM_WRITE_BLOCK+leftlen>EEPROM_WRITE_BLOCK)
		{
			wrlen = EEPROM_WRITE_BLOCK - pos%EEPROM_WRITE_BLOCK;
		}
		else
		{
			wrlen = leftlen;
		}
		cdebug("wrlen is %d\n",wrlen);
		(e2prom_data.msgs[0]).len= wrlen+1;			
		(e2prom_data.msgs[0]).buf[0] = pos;
		memcpy(&(e2prom_data.msgs[0].buf[1]), buf, wrlen);
		ret=ioctl(fd,I2C_RDWR,(unsigned long)&e2prom_data);
		if(ret<0)
		{
			printf("Write ioctl error\n");
			close(fd);
			return(len - leftlen);
		}
		pos += wrlen;
		leftlen -=  wrlen;
		buf += wrlen;
		usleep(1000);
	}	
	close(fd);
	free((e2prom_data.msgs[0]).buf);
	return len;
}

int EepromRead(unsigned short addr,unsigned char *buf, unsigned short len)
{
	int fd,ret, i = 0, j = 0;
	struct i2c_rdwr_ioctl_data e2prom_data;
	unsigned short pos = addr, leftlen = len, wrlen = 0;

	if(NULL == buf)
		return -1;
	if(addr +len > BUFSIZE)
		return -1;
	fd=open("/dev/i2c-0",O_RDWR);
	if(fd<0)
	{
		 cdebug("open /dev/i2c-0 error\n");
		 return(-1);
	}
	e2prom_data.nmsgs= 2; 
	e2prom_data.msgs=(struct i2c_msg*)malloc(e2prom_data.nmsgs*sizeof(struct i2c_msg));
	
	if(!e2prom_data.msgs)
	{
		cdebug("EepromWrite malloc error\n");
		close(fd);
		return(-1);
	}

	ioctl(fd,I2C_TIMEOUT,1);
	ioctl(fd,I2C_RETRIES,2);
//	sleep(1);
	
	 e2prom_data.nmsgs=2;
	 (e2prom_data.msgs[0]).len=1; 
	 (e2prom_data.msgs[0]).addr=EEPROM_ADDR;  
	 (e2prom_data.msgs[0]).flags=0;
	 (e2prom_data.msgs[0]).buf=(unsigned char*)malloc(10);
	 (e2prom_data.msgs[0]).buf[0]=0;
	 (e2prom_data.msgs[1]).len=BUFSIZE;
	 (e2prom_data.msgs[1]).addr=EEPROM_ADDR;
	 (e2prom_data.msgs[1]).flags=I2C_M_RD;
	 (e2prom_data.msgs[1]).buf=(unsigned char*)malloc(BUFSIZE);
	 (e2prom_data.msgs[1]).buf[0]=0;
	 (e2prom_data.msgs[1]).buf[1]=0;
	 ret=ioctl(fd,I2C_RDWR,(unsigned long)&e2prom_data);
	 if(ret<0)
	 {
		printf("Read ioctl error\n");
		close(fd);
		return(-1);
	 }
	 memcpy(buf, e2prom_data.msgs[1].buf, BUFSIZE);

	 close(fd);

	free((e2prom_data.msgs[0]).buf);
	free((e2prom_data.msgs[1]).buf);
	free(e2prom_data.msgs); 
	
	return len;
}

#endif
/*
int readEepromInfo(void)
{
	int ret;

	ret = EepromRead(0, buf, 256);	 

	if(ret < 0)
	{
		cdebug("Error to read EEprom informaior in function boardinfo_init");
		return (-1);
	}
    cdebug("board init start...\n");
    return 0;
}
*/
int getBoardTypeFromEeprom(void)
{
    int i,j;
    for(i=0; i<0x100; i++)
    {

        if((buf[i] == 'H')  && (buf[i+1]=='w') && (buf[i+2]=='C') && (buf[i+3]=='f') && (buf[i+4]=='g') )
        {
            for(j=0;j<8;j++)
            {
                if(buf[i+8+j] >= '0' && buf[i+8+j] <= '9')
                {

                    boardTypeValue |= (unsigned  int)(buf[i+8+j]-48)<<(32-(j+1)*4);
                }
                else if((buf[i+8+j] >= 'a' && buf[i+8+j] <= 'f') || (buf[i+8+j] >= 'A' && buf[i+8+j] <= 'F'))
                {

                    boardTypeValue |= (unsigned  int)(buf[i+8+j]-55)<<(32-(j+1)*4);
                }
            }
            boardTypeValue = (boardTypeValue >> 8) & 0xff;
            cdebug("Board Type Value=0x%08x\n", boardTypeValue);	
            break;
        }
    }// end of for
    return 0;
}

int getMacAddressFromEeprom(void)
{
    int i,j;

    /* get 1st_MAC address */
    for(i=0; i<0x100; i++)
    {
        if((buf[i] == '1')  && (buf[i+1]=='s') && (buf[i+2]=='t') && (buf[i+3]=='_') && (buf[i+4]=='M') && (buf[i+5]=='A') && (buf[i+6]=='C'))
        {
            for(j=0;j<18;j++)
            {
                if(buf[i+8+j] != ' ')
                    firstMAC[j] = buf[i+8+j];
            }
            cdebug("1st_MAC=%s \n", firstMAC);	
            break;
        }
    }	
	return 0;
}
int getVendorFromEeprom(void)
{
    int i,j;

    /* get Vendor */
    for(i=0; i<0x100; i++)
    {
        if((buf[i] == 'V')  && (buf[i+1]=='e') && (buf[i+2]=='n') && (buf[i+3]=='d') && (buf[i+4]=='o') && (buf[i+5]=='r'))
        {
            for(j=0;j<16;j++)
            {
                if(buf[i+7+j] != ' ')
                    VendorName[j] = buf[i+7+j];
            }
            cdebug("VendorName=%s \n", VendorName);	
            break;
        }
    }	
    return 0;
}
int getDeviceFromEeprom(void)
{
    int i,j;

    /* get Device Name */
    for(i=0; i<0x100; i++)
    {
        if((buf[i] == 'D')  && (buf[i+1]=='e') && (buf[i+2]=='v') && (buf[i+3]=='i') && (buf[i+4]=='c') && (buf[i+5]=='e'))
        {
            for(j=0;j<16;j++)
            {
                if(buf[i+7+j] != ' ')
                    DeviceName[j] = buf[i+7+j];
            }
            cdebug("DeviceName=%s \n", DeviceName);	
            break;
        }
    }	
    return 0;
}

int boardinfo_init(void)
{
  int ret;
  memset(buf,0,BUFSIZE);
  boardTypeValue = 0;

  ret = EepromRead(0, buf, 256);	
  if(ret <0)
  { 
    cdebug("Error to read EEprom informaior in function boardinfo_init");
    return (-1);
  }
  cdebug("board init start...\n");
  return 0;
}


int get_bdtype(void)
{
	int ret;
	ret = getBoardTypeFromEeprom();
    	return boardTypeValue;
}


int get_vendor_name(unsigned char *ptr)
{
	int ret;
	ret = getVendorFromEeprom();

	if(NULL == memcpy(ptr, VendorName,strlen(VendorName)))
	{
		cdebug("Cannot get vendor name\n");
		return (-1);
	}
  cdebug("start get vendor name...\n");
	return 0;
	
}

int get_device_name(unsigned char *ptr)
{
	int ret;
	ret = getDeviceFromEeprom();
	
	if(NULL == memcpy(ptr, DeviceName,strlen(DeviceName)))
	{
		cdebug("can not get device name\n");
		return (-1);
	}
  cdebug("start get device name...\n");
	return 0;
}

int get_mac_address(unsigned char *ptr)
{
	int ret;
	ret = getMacAddressFromEeprom();
	if(NULL == memcpy(ptr,firstMAC,strlen(firstMAC)))
	{
		cdebug("can not get first Mac address\n");
		return (-1);
	}
  cdebug("start get mac address\n");
	return 0 ;
}
#if 0
int main(void)
{
  boardinfo_init();
  getBoardTypeFromEeprom();
  getMacAddressFromEeprom();
  getVendorFromEeprom();
  getDeviceFromEeprom();

  return 1;
}
#endif

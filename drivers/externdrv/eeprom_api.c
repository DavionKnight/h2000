/*
 * @file	eeprom_api.c
 * @date	2015-12-31
 * @author   	zhangjj<zhangjj@huahuan.com>
 */


#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "api.h"

#define I2C_BASE 	0x1000
#define I2C_RSV		(I2C_BASE + 0x0)
#define I2C_CMD		(I2C_BASE + 0x1)
#define I2C_DATA	(I2C_BASE + 0x40)

#define BUFSIZE	256

enum eeprom_addr{
	LOCALBOARD	= 0x00,
	BACKBOARD	= 0x50,
	FAN		= 0x51,
	PWR_A		= 0x52,
	PWR_B		= 0x53,
	CK01		= 0x4F	 
};

int i2c_cmdword_set(int fd, enum eeprom_addr i2c_addr, unsigned char rd_wr)
{
	int cmd_rsv = 0x00010000;
	struct fpga_msg msg;
	unsigned int cmd;

	memset(&msg, 0, sizeof(msg));
	msg.addr = I2C_RSV;
	msg.len = 4;
	memcpy(msg.buf,&cmd_rsv,sizeof(cmd_rsv));
	write_fpga_data(fd,&msg,0);
	read_fpga_data(fd,&msg,0);

	memset(&msg, 0, sizeof(msg));
	msg.addr = I2C_CMD;
	msg.len = 4;
	cmd = 0;
	cmd |= 0x00020000;
	cmd |= 1<<15;	//cmd enable
	cmd |= rd_wr ?(1<<14):0; /*0 write, 1 read*/
	cmd |= 1<<13;
	cmd |= 1<<12;
	cmd |= i2c_addr?(1<<11):0;
	i2c_addr &= 0x7;
	cmd |= i2c_addr << 8;
	memcpy(msg.buf, &cmd, 4);
	write_fpga_data(fd,&msg,0);

	return 0;
}
int EepromWrite(enum eeprom_addr eep_addr,unsigned char *buf, unsigned short len)
{
	int fd,ret, i = 0, j = 0;
        struct fpga_msg msg;
	unsigned char *str = buf;
	
	if((len != 256)||!buf)
	{
		printf("para error\n");
		return -1;
	}

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
	for(i = 0; i<BUFSIZE/4; i++)	
	{
	        msg.addr = I2C_DATA+i;
	        msg.len = 4;
	        memcpy(msg.buf, str, 4);
	        write_fpga_data(fd,&msg,0);
		str += 4;
	}
	ret = i2c_cmdword_set(fd, eep_addr, 0);

	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
	}
	close(fd);

	return ret;
}

int EepromRead(enum eeprom_addr eep_addr,unsigned char *buf, unsigned short len)
{
        int fd,ret = 0, i = 0;
        struct fpga_msg msg;
	unsigned char *str = buf;

        if((len != 256)||!buf)
        {
                printf("para error\n");
                return -1;
        }

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
        ret = i2c_cmdword_set(fd, eep_addr, 1);
        for(i = 0; i<BUFSIZE/4; i++)
        {
                msg.addr = I2C_DATA+i;
                msg.len = 4;
                read_fpga_data(fd,&msg,0);
                memcpy(str, msg.buf, 4);
		str += 4;
        }
	if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
	{
		cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
	}
	close(fd);

        return ret;
}

#if 0
int main(void)
{
	unsigned char buf[BUFSIZE];
	int i=0,ret = 0;
	enum eeprom_addr eepaddr = CK01;
#if 1
	memset(buf, 0, BUFSIZE);
	for(i=0;i<256;i++)
		buf[i] = 0xff-i;
//		buf[i] = i;
	ret = EepromWrite(eepaddr, buf, BUFSIZE);
#endif
	memset(buf, 0, BUFSIZE);
	ret = EepromRead(eepaddr, buf, BUFSIZE);
	for(i=0;i<256;i++)
	{
		if(!(i%16))
			printf("\n");	
		printf("0x%02x ",buf[i]);
	}
	printf("\n");	

	return 0;
}
#endif

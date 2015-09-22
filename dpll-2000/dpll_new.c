/**********************************************
 * @file	dpll.c
 * @author	zhangjj <zhangjj@bjhuahuan.com>
 * @date	2015-08-20
 *********************************************/

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <string.h>

//#define	IDTDEBUG
#define IDT285_SLAVE_ADDR		0xf8

#define UNIT_REG_BASE			0x2000
#define CTRL_STATUS_REG			(UNIT_REG_BASE+0)
#define READ_OVER_FLAG			(UNIT_REG_BASE+0x0002)
#define WRITE_ONCE_REG			(UNIT_REG_BASE+0x0010)
#define READ_ONCE_REG			(UNIT_REG_BASE+0x0020)
#define BUFFER_ADDR_400			(UNIT_REG_BASE+0x0200)				

#define I2C_WREN			0x40
#define I2C_RDEN			0x41
#define I2C_DEV_ADDR			0x42
#define I2C_REG_ADDR			0x43
#define I2C_WRDATA			0x44
#define I2C_EN				0x45

#define SPI_IOC_MAGIC			'k'
#define SPI_IOC_OPER_FPGA	    	_IOW(SPI_IOC_MAGIC, 5, __u8)
#define SPI_IOC_OPER_FPGA_DONE		_IOW(SPI_IOC_MAGIC, 6, __u8)

#define WORDSIZE			4

const uint ic_size = 0x00004000; // 0x00010000 bytes
static int fpga_dev = -1;

void pdata(unsigned char *pdata, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		printf(" %02x", pdata[i]);
	}
	printf("\n");
}

/* ---------------------------------------------------------------- 
fpga_init
must be called first !!!
return:
	0,		success
	-1,		fail to open the driver
*******************************************************************/
int fpga_init(void)
{
	fpga_dev = open( "/dev/spidev0.0", O_RDWR);
	if( fpga_dev == -1 )
		return -1;
        ioctl(fpga_dev, SPI_IOC_OPER_FPGA, NULL);
	return 0;
}
int fpga_close(void)
{
	ioctl(fpga_dev, SPI_IOC_OPER_FPGA_DONE, NULL);
	close(fpga_dev);
	return 0;
}
/********************************************************************
write any registers of the fpga, directly
return:
	0,		success
	-1,		fail to open the driver
	-2,		some parameters are out of the valid range
**********************************************************************/
int fpga_write_once(unsigned char slot, unsigned short addr, unsigned short* wdata)
{
	unsigned char data[32] = {0};
	unsigned int fpga_devata = 0;

	data[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
	data[1] = addr & 0xff;
	data[2] = (*wdata & 0xff00) >> 8;
	data[3] = *wdata & 0xff;
#ifdef IDTDEBUG
	printf("Write Data:\n");
	pdata(data,4);
#endif	
	if (lseek(fpga_dev, WRITE_ONCE_REG, SEEK_SET) != WRITE_ONCE_REG) {
		printf("lseek error.\n");
	}
	write(fpga_dev, data, WORDSIZE);
	usleep(10);

	return 0;
}

int fpga_read_once(unsigned char slot, unsigned short addr, unsigned short* wdata)
{
	unsigned char data[32] = {0}, mode = 1;
	unsigned int read_flag = 0;
        unsigned int delay_count = 1000;


	data[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
	data[1] = addr & 0xff;
	data[2] = ((mode & 0x07) << 5) | ((*wdata&0x1f00) >> 8);
	data[3] = *wdata & 0xff;
#ifdef IDTDEBUG
	printf("Read data:\n");
	pdata(data,4);
#endif

	if (lseek(fpga_dev, READ_ONCE_REG, SEEK_SET) != READ_ONCE_REG) {
		printf("lseek error.\n");
	}
	write(fpga_dev, data, WORDSIZE);

	if (lseek(fpga_dev, READ_OVER_FLAG, SEEK_SET) != READ_OVER_FLAG) {
		printf("lseek error.\n");
	}

	do{
		read(fpga_dev, &read_flag, WORDSIZE);
		if((read_flag == 0)||(delay_count <1))
			break;
	}while(delay_count--);

	if (lseek(fpga_dev, UNIT_REG_BASE + ((*wdata) >>1), SEEK_SET) != UNIT_REG_BASE+ ((*wdata) >> 1)) {
		printf("lseek error.\n");
	}
	memset(data,0,32);
	read(fpga_dev, data, 4);
	usleep(10);
#ifdef IDTDEBUG
	printf("Return Data:\n");	
	pdata(data,4);
#endif
	if((*wdata)%2)
		*wdata = data[0]<<8 | data[1];
	else
		*wdata = data[2]<<8 | data[3];
	return 0;
}
int idt285_read(unsigned char slot_num, unsigned short addr, unsigned char *data)
{
	unsigned short wdata = 0, bufaddr = 0x400;	
	unsigned char slot = slot_num;
	unsigned int delay_count = 100;

	//1step,write addr
	wdata = IDT285_SLAVE_ADDR;				
	fpga_write_once(slot, I2C_DEV_ADDR, &wdata);	//dev addr
	wdata = addr;	
	fpga_write_once(slot, I2C_REG_ADDR, &wdata);	//iic reg addr

	//2step,write rd enable at eadge jump
	wdata = bufaddr;
	fpga_read_once(slot, I2C_RDEN,&wdata);
	wdata = (wdata^0x1)&0x1;
	fpga_write_once(slot, I2C_RDEN, &wdata);

	//3step,if we can write 
	do{
		wdata = bufaddr;
		fpga_read_once(slot, I2C_EN, &wdata);
		if((wdata&0x8000)||(delay_count<1))
		break;
	}while(delay_count--);
	usleep(10);
#ifdef IDTDEBUG
	printf("delay_count= %d\n",delay_count);
#endif

	//4step,return data
	*data = wdata & 0xff;
	return 0;
}
int idt285_write(unsigned char slot_num, unsigned short addr, unsigned char data)
{
	unsigned short wdata = 0, bufaddr = 0x400;	
	unsigned char slot = slot_num;
	unsigned int delay_count = 100;

	if(addr > 0x400)
		return -1;

	//1step,write addr
	wdata = IDT285_SLAVE_ADDR;				
	fpga_write_once(slot, I2C_DEV_ADDR, &wdata);	//dev addr
	wdata = addr;	
	fpga_write_once(slot, I2C_REG_ADDR, &wdata);	//iic reg addr

	//2step,write data into wrdata addr
	wdata = data;
	fpga_write_once(slot, I2C_WRDATA, &wdata);	//dev addr

	
	//3step,write wr enable at eadge jump
	wdata = bufaddr;
	fpga_read_once(slot, I2C_WREN, 1,&wdata);
	wdata = (wdata^0x1)&0x1;
	fpga_write_once(slot, I2C_WREN, &wdata);

	//4step,if we can write 
	do{
		wdata = bufaddr;
		fpga_read_once(slot, I2C_EN, 1, &wdata);
		if((wdata&0x8000)||(delay_count<1))
		break;
	}while(delay_count--);

	usleep(10);
#ifdef IDTDEBUG
	printf("delay_count= %d\n",delay_count);
#endif
	return 0;
}

int dpll_idt285_init(unsigned char slot)
{
	FILE *fp;
	char reg_buf[20];
	char *fret = NULL;
	unsigned short regaddr;
	unsigned char val;

	if((fp = fopen("idt285_Reg.txt","r")) == NULL)
	{
		printf("Open idt285_Reg.txt error\n");
		return -1;
	}

	//fgets will stop when line break
	fret = fgets(reg_buf,20,fp);
	while(!feof(fp))
	{
		sscanf(reg_buf,"%hx%hhx",&regaddr,&val);
		if(regaddr > 0x7)
		{
//			printf("Write:%02x %02x\n",regaddr,val);
			idt285_write(slot, regaddr, val);
			usleep(10);
			idt285_read(slot, regaddr,&data);
//			printf("Read:%02x %02x\n",regaddr,data);
		}
		memset(reg_buf,0,20);
		fret = fgets(reg_buf,20,fp);
	}

	fclose(fp);
	
#ifdef IDTDEBUG
	for(i = 0;i<0x317;i++)
	{
		addr = i;
		idt285_read(addr,&data);
		printf("0x%02x ",data);
		if((i+1)%16)
		;
		else
		printf("\n");
		usleep(100);
	}
	printf("\n");
#endif
	
#if 0
	addr = 0x3;
	idt285_read(addr,&data);

	printf("Read Reg:0x%x = 0x%x=================1\n",addr,data);
	addr = 0x3;
	data = 0x60;
	idt285_write(addr,data);
	printf("Write Reg:0x%x = 0x%x================2\n",addr,data);
	addr = 0x3;
	idt285_read(addr,&data);

	printf("Read Reg:0x%x = 0x%x=================3\n",addr,data);
#endif
	return 0;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	unsigned short addr = 0;
	unsigned char data = 0, slot_num = 0;
	int i = 0;	

	if(argc != 2)
	{
		printf("Please Input slot num[0-3]");
		return 0;
	}
	if((argv[1][0]<'0')||(argv[1][0]>'3'))
	{
		printf("Please Input the correct slot num [0-3]");
		return 0;
	}
	slot_num = argv[1][0]-'0';
	printf("slot_num = %d\n",slot_num);

	fpga_init();
	
	dpll_idt285_init(slot_num);	

END:
	fpga_close();
	return 0;	
}




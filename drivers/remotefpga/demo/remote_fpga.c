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

int main(int argc, char *argv[])
{
	int ret = 0;
	unsigned short addr = 0,data;
	unsigned char slot_num = 0;
	int i = 0;	

	fpga_init();

	if (argc == 4 && argv[1][0] == 'r') {
		sscanf(argv[2], "%hhx", &slot_num);
		sscanf(argv[3], "%hx", &addr);
#if 0
		printf("0 %s\n",argv[0]);
		printf("1 %s\n",argv[1]);
		printf("2 %s\n",argv[2]);
#endif
		printf("slot num %x addr 0x%04x:\n", slot_num, (unsigned short)addr);
		data = 0x400;
		fpga_read_once(slot_num,addr,&data);
		printf("The result:\n0x%04x\n",data);
	}
	else if (argc == 5 && argv[1][0] == 'w') {
		sscanf(argv[2], "%hhx", &slot_num);
		sscanf(argv[3], "%hx", &addr);
		sscanf(argv[4], "%hx", &data);
		printf("slot num %x,write addr 0x%d data 0x%04x:\n", slot_num,addr,data);
		fpga_write_once(slot_num, addr, &data); 
	}
	else
	{
		printf("remotefpga read <slot:hex> <addr:hex>\n");
		printf("remotefpga write <slot:hex> <addr:hex> <data:hex>\n");
	}
	
	fpga_close();

	return 0;	
}




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

#define UNIT_REG_BASE			0x2000
#define CTRL_STATUS_REG			(UNIT_REG_BASE+0)
#define READ_OVER_FLAG			(UNIT_REG_BASE+0x0002)
#define WRITE_ONCE_REG			(UNIT_REG_BASE+0x0010)
#define READ_ONCE_REG			(UNIT_REG_BASE+0x0020)
#define BUFFER_ADDR_400			(UNIT_REG_BASE+0x0200)				

#define I2C_WREN			0x40
#define I2C_RDEN			0x41
#define I2C_ADDR			0x42
#define I2C_WRDATA			0x43
#define I2C_EN				0x44

#define SPI_IOC_MAGIC			'k'
#define SPI_IOC_OPER_FPGA	    	_IOW(SPI_IOC_MAGIC, 5, __u8)
#define SPI_IOC_OPER_FPGA_DONE		_IOW(SPI_IOC_MAGIC, 6, __u8)
#define WORDSIZE			4

const uint ic_size = 0x00004000; // 0x00010000 bytes
static int fpga_dev = -1;



typedef struct{
unsigned short	slot:4;
unsigned short	addr:12;
unsigned short	mode:3;
unsigned short	bufaddr:13;
}singleRead;

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
must be called in first !!!
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
//	printf("Write Data:\n");
	pdata(data,4);
	
	if (lseek(fpga_dev, WRITE_ONCE_REG, SEEK_SET) != WRITE_ONCE_REG) {
		printf("lseek error.\n");
	}
	write(fpga_dev, data, WORDSIZE);

	return 0;
}
int fpga_read_once(unsigned char slot, unsigned short addr, unsigned char mode, unsigned short* wdata)
{
	unsigned char data[32] = {0};
	unsigned int read_flag = 0;
	data[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
	data[1] = addr & 0xff;
	data[2] = ((mode & 0x07) << 5) | ((*wdata&0x1f00) >> 8);
	data[3] = *wdata & 0xff;
	printf("Read data:\n");
	pdata(data,4);

	if (lseek(fpga_dev, READ_ONCE_REG, SEEK_SET) != READ_ONCE_REG) {
		printf("lseek error.\n");
	}
	write(fpga_dev, data, WORDSIZE);

	if (lseek(fpga_dev, READ_OVER_FLAG, SEEK_SET) != READ_OVER_FLAG) {
		printf("lseek error.\n");
	}
	read(fpga_dev, &read_flag, WORDSIZE);
//	printf("read_over_flag = 0x%x\n",read_flag);

	if (lseek(fpga_dev, UNIT_REG_BASE + ((*wdata) >>1), SEEK_SET) != UNIT_REG_BASE+ ((*wdata) >> 1)) {
		printf("lseek error.\n");
	}
	memset(data,0,32);
	read(fpga_dev, data, 4);
//	printf("Return Data:\n");	
	pdata(data,4);
	if((*wdata)%2)
		*wdata = data[0]<<8 | data[1];
	else
		*wdata = data[2]<<8 | data[3];
	return 0;
}

int main()
{
	int ret = 0, delay_count = 50;
	unsigned short wdata;
	unsigned char slot = 2, addr = 0x3;
	unsigned char baddr = 0x00;	

	fpga_init();
do{	
	//1step,write addr
	wdata = (0x40 << 8) | (addr);	//iic slave addr+ 285 addr reg
	fpga_write_once(slot, 0x42, &wdata);	//iic reg addr
#if 0
	wdata = 0x0;	//iic slave addr+ 285 addr reg
	fpga_write_once(slot, 0x23, &wdata);	//iic reg addr
	

	wdata = 0x400;				//
	fpga_read_once(slot, 0x42, 1,&wdata);
	printf("upup\n");
#endif
	//2step,read rd enable
	wdata = 0x400;				//
	fpga_read_once(slot, 0x41, 1,&wdata);

	//3step,write rd enable at eadge jump
	wdata = (wdata^0x1)&0x1;
	fpga_write_once(slot, 0x41, &wdata);
//sleep(1);
//	printf("In loop..\n");
	do{
		wdata = 0x400;
		fpga_read_once(slot, 0x44, 1, &wdata);
		if(wdata&0x8000)
		break;
	}while(delay_count--);
//	printf("delay_count= %d\n",delay_count);
//	printf("The ID :0x%x\n",wdata&0xf);
baddr++;
}while((baddr<0xff)&&((wdata&0xf)==0));
	printf("baddr=0x%x,wdata=0x%x\n",baddr,(wdata&0xf));

END:
	ioctl(fpga_dev, SPI_IOC_OPER_FPGA_DONE, NULL);
	close(fpga_dev);
	return 0;	
}




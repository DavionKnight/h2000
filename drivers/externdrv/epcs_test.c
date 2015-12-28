/*
* @file		 epcs_test.c
* @author  zhangjj<zhangjj@huahuan.com>
* @date		 2014-04-15
*/


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


#define W25_ERASE_CHIP			_IOW(SPI_IOC_MAGIC, 6 , __u8)
#define W25_ERASE_SECTOR		_IOW(SPI_IOC_MAGIC, 7 , __u32)
#define W25P16_READ				_IOR(SPI_IOC_MAGIC, 8 , __u32)
#define W25P16_WRITE			_IOW(SPI_IOC_MAGIC, 8 , __u32)
#define   W25P1165_ID    _IOR(SPI_IOC_MAGIC, 13,  __u8)

typedef struct{
	loff_t addr;
	size_t len;
	u_char buf[256];
}w25_rw_date_t;


static const char *device = "/dev/spidev0.0";
static uint8_t mode=3;
static uint8_t bits = 8;
static uint32_t speed = 200000;
//static uint16_t delay = 20;

#define LEN		0x100
#define ADDR	0x1000


#if 0
int main(int argc, char *argv[])
{
	int ret = 0;
	int fd;
	

	w25_rw_date_t  w25p16_date;
	unsigned int sector_addr = ADDR;
	int i;

	fd = open(device, O_RDWR | O_SYNC | O_DSYNC | O_RSYNC);
	if (fd < 0)
		printf("can't open device");
#if 0
	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		printf("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		printf("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		printf("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		printf("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		printf("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		printf("can't get max speed hz");

	chip_select = 1;
	ret = ioctl(fd, SPI_IOC_WR_CHIP_SLECT_NUM, &chip_select);
	if (ret == -1)
		printf("cs failed");
#endif
//**********************************************************************first read++++++++++++++++++++++++
	ret = ioctl(fd, W25P1165_ID, NULL);
	if (ret == -1)
		printf("\ncan't set spi mode\n");

	
	w25p16_date.addr = ADDR;
	w25p16_date.len =  LEN;
	memset(w25p16_date.buf, 0x00, sizeof(w25p16_date.buf));//clear

	ret = ioctl(fd, W25P16_READ, (unsigned long)&w25p16_date);
	if (ret == -1)
		printf("\nafter erase read FAILD\n");
    printf("\n");
	printf("\n+++++++++++++++++++++++++++++++before earse:++++++++++++++++++++++++++++++++++++++++++++++\n");
	for( i = 0; i <  LEN; i++)
	{
		if( (i %8) == 0)
			printf("\n");
		printf(" buf[0x%02x] = 0x%02x", i, w25p16_date.buf[i]);
	}
#if 1
	ret = ioctl(fd,  W25_ERASE_CHIP, NULL);
	if (ret == -1)
		printf("\nERASE FAILD\n");
	
	w25p16_date.addr = ADDR;
	w25p16_date.len =  LEN;
	memset(w25p16_date.buf, 0x00, sizeof(w25p16_date.buf));//clear
	ret = ioctl(fd, W25P16_READ, (unsigned long)&w25p16_date);
	if (ret == -1)
		printf("\nafter erase read FAILD\n");
    printf("\n");
    printf("\n+++++++++++++++++++++++++++++++after earse:++++++++++++++++++++++++++++++++++++++++++++++\n");
	for( i = 0; i <  LEN; i++)
	{
		if( (i %8) == 0)
			printf("\n");
		printf(" buf[0x%02x] = 0x%02x", i, w25p16_date.buf[i]);
	}

	for(i = 0; i <  LEN; i++)
		w25p16_date.buf[i] = i;

	ret = ioctl(fd, W25P16_WRITE, (unsigned long)&w25p16_date);
	if (ret == -1)
	{
		printf("\nwrite FAILD\n");
		return ret;
	}

	memset(w25p16_date.buf, 0x00, sizeof(w25p16_date.buf));//clear

	ret = ioctl(fd, W25P16_READ, (unsigned long)&w25p16_date);
	if (ret == -1)
	{
		printf("\nafter erase read FAILD\n");
		return ret;
	}
	printf("\n");
    printf("\n+++++++++++++++++++++++++++++++after program:++++++++++++++++++++++++++++++++++++++++++++++\n");
	for( i = 0; i < LEN; i++)
	{
		if( (i %8) == 0)
			printf("\n");
		printf(" buf[0x%02x] = 0x%02x", i, w25p16_date.buf[i]);
	}
#endif
	printf("\n");
	close(fd);

	return ret;
}
#endif


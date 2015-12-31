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

#define SPI_IOC_MAGIC			'k'
#define SPI_IOC_OPER_FPGA	    	_IOW(SPI_IOC_MAGIC, 5, __u8)
#define SPI_IOC_OPER_FPGA_DONE		_IOW(SPI_IOC_MAGIC, 6, __u8)
#define SPI_IOC_OPER_DPLL		_IOW(SPI_IOC_MAGIC, 7, __u8)
#define SPI_IOC_OPER_DPLL_DONE		_IOW(SPI_IOC_MAGIC, 8, __u8)
#define W25_ERASE_CHIP          	_IOW(SPI_IOC_MAGIC, 9,  __u8)
#define W25_ERASE_SECTOR    		_IOW(SPI_IOC_MAGIC, 10, __u32)
#define W25P16_READ		   	_IOR(SPI_IOC_MAGIC, 11, __u32)
#define W25P16_WRITE	   		_IOW(SPI_IOC_MAGIC, 12, __u32)
#define W25P1165_ID    			_IOR(SPI_IOC_MAGIC, 13,  __u8)
#define SPI_IOC_OPER_FLASH		_IOW(SPI_IOC_MAGIC, 14, __u8)
#define SPI_IOC_OPER_FLASH_DONE		_IOW(SPI_IOC_MAGIC, 15, __u8)

#define TEST_ADDR 			0x0
#define BLOCK_SIZE			0x100

typedef struct{
	loff_t addr;
	size_t len;
	u_char buf[BLOCK_SIZE];
}w25_rw_date_t;


int fpga_flash_write(int fd_pof)
{
	int fd,len,ret=0;
	
	w25_rw_date_t  w25p16_date;
	unsigned int  faddr = TEST_ADDR;

	fd = open("/dev/spidev0.0",O_RDWR);
	if(fd<0)
		return -1;
        ret = ioctl(fd, SPI_IOC_OPER_FLASH, NULL);
	if(ret<0)
		goto END;
#if 0
	for(i=0;i<5;i++)
	{
		if(ioctl(fd, W25_ERASE_SECTOR, faddr)<0)
			return -1;
		faddr += 0x10000;
	}
#else	
	if(ioctl(fd, W25_ERASE_CHIP, 0)<0)
	{
		ret = -1;
		goto END;
	}
#endif
	
	while((len =read(fd_pof,w25p16_date.buf,BLOCK_SIZE))>0)
        {

                w25p16_date.addr = faddr;
                w25p16_date.len =  len;
	       	ret = ioctl(fd, W25P16_WRITE, (unsigned long)&w25p16_date);
		if (ret != 0)
		{
			printf("write error\n");
			break;
		}
		faddr += BLOCK_SIZE;
#if 0
		j++;
		if((j+1)%100==0)
			printf("j = %d\n",j+1);	
#endif
		usleep(10);
	}
END:
	ioctl(fd, SPI_IOC_OPER_FLASH_DONE, NULL);
	close(fd);
	return ret;
}


int main(int argc, char *argv[])
{
	int fd_app;

	if(argc<2)
	{
		printf("Please input pof name");
		return 0;
	}
	printf("fpga file is %s\nupdating..\n",argv[1]);
	fd_app = open(argv[1],O_RDWR);

	if(fpga_flash_write(fd_app)<0)
	{
		printf("error\n");
	}

	printf("update fpga program successfully!\n");	
	sleep(1);
#if 0
	printf("\n+++++++++++++++++++++++++++++++read data:++++++++++++++++++++++++++++++++++++++++++++++\n");
	faddr = TEST_ADDR;

	fd_rd = open("fpga_read.bin",O_RDWR|O_CREAT);
	while(j>0)
        {

                w25p16_date.addr = faddr;
                w25p16_date.len =  BLOCK_SIZE;
                memset(w25p16_date.buf, 0x00, BLOCK_SIZE);
        
                ret = ioctl(fd, W25P16_READ, (unsigned long)&w25p16_date);
                if (ret != 0)
                {
                        printf("after erase read FAILD");
                        break;
                }
		write(fd_rd,w25p16_date.buf,BLOCK_SIZE);
                faddr += BLOCK_SIZE;
                j--;
		usleep(100);
	}
	close(fd_rd);
#endif
	close(fd_app);
	return 0;
}




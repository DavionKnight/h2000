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
#define SPI_IOC_OPER_FPGA		    _IOW(SPI_IOC_MAGIC, 5, __u8)
#define SPI_IOC_OPER_FPGA_DONE		_IOW(SPI_IOC_MAGIC, 6, __u8)
#define SPI_IOC_OPER_DPLL		    _IOW(SPI_IOC_MAGIC, 7, __u8)
#define SPI_IOC_OPER_DPLL_DONE		_IOW(SPI_IOC_MAGIC, 8, __u8)
#define  W25_ERASE_CHIP          _IOW(SPI_IOC_MAGIC, 9,  __u8)
#define  W25_ERASE_SECTOR     _IOW(SPI_IOC_MAGIC, 10, __u32)
#define  W25P16_READ		   _IOR(SPI_IOC_MAGIC, 11, __u32)
#define   W25P16_WRITE	   _IOW(SPI_IOC_MAGIC, 12, __u32)
#define   W25P1165_ID    _IOR(SPI_IOC_MAGIC, 13,  __u8)
#define SPI_IOC_OPER_FLASH		    _IOW(SPI_IOC_MAGIC, 14, __u8)
#define SPI_IOC_OPER_FLASH_DONE		_IOW(SPI_IOC_MAGIC, 15, __u8)

typedef struct{
	loff_t addr;
	size_t len;
	u_char buf[256];
}w25_rw_date_t;


int main()
{
	int fd;
	int pid = 0, ret = 0,j = 0;
	unsigned short i = 0, data = 0;
	w25_rw_date_t  w25p16_date;
	unsigned int  faddr = 0x80000;

//	printf("child process, test w/r fpga\n");
	fd = open("/dev/spidev0.0",O_RDWR);
//	ioctl(fd, W25P1165_ID, NULL);
	
        ret = ioctl(fd, SPI_IOC_OPER_FLASH, NULL);
	printf("after ioctl,ret = %d\n",ret );
	ioctl(fd,W25_ERASE_SECTOR,&faddr);
	printf("after erase\n");
	printf("\n+++++++++++++++++++++++++++++++read data:++++++++++++++++++++++++++++++++++++++++++++++\n");
	faddr = 0x80000;
	j = 0;
	while(1)
        {

                w25p16_date.addr = faddr;
                w25p16_date.len =  0x100;
                memset(w25p16_date.buf, 0x00, sizeof(w25p16_date.buf));
        
                ret = ioctl(fd, W25P16_READ, (unsigned long)&w25p16_date);
                if (ret != 0)
                {
                        printf("after erase read FAILD");
                        break;
                }
                for( i = 0; i <  0x100; i++)
                {
                        if( (i %16) == 0)
                                printf("\n");
                        printf("%02X ",w25p16_date.buf[i]);
                }
                faddr += 0x100;
                j++;
                if(j>50)
                {
                        printf("j>8192\n");
                        break;
                }
	}
	sleep(1);
	faddr = 0x80000;
	j = 0;
	while(1)
	{
#if 0
		w25p16_date.addr = faddr;
		w25p16_date.len =  0x100;
		memset(w25p16_date.buf, 0x00, sizeof(w25p16_date.buf));
	
	       	ret = ioctl(fd, W25P16_READ, (unsigned long)&w25p16_date);
		if (ret != 0)
		{
			printf("after erase read FAILD");
			break;
		}
		for( i = 0; i <  0x100; i++)
		{
			if( (i %16) == 0)
				printf("\n");
			printf("%02X ",w25p16_date.buf[i]);
		}
		faddr += 0x100;
		j++;
		if(j>800)
		{
			printf("j>8192\n");
			break;
		}
#endif
		w25p16_date.addr = faddr;	//500K
		w25p16_date.len =  0x100;
		memset(w25p16_date.buf, 0x00, sizeof(w25p16_date.buf));
		

		for( i = 0; i <  0x100; i++)
		{
			if(j%2)
			w25p16_date.buf[i] = i;
			else
			w25p16_date.buf[i] = 0x100 - i;
			
		}
	       	ret = ioctl(fd, W25P16_WRITE, (unsigned long)&w25p16_date);
		if (ret != 0)
		{
			printf("after erase read FAILD");
			break;
		}
		faddr += 0x100;
		j++;
		if(j>50)
		{
			printf("j>800\n");
			break;
		}
		usleep(10);
	}
	printf("write down..\n");
	sleep(1);

	printf("\n+++++++++++++++++++++++++++++++read data:++++++++++++++++++++++++++++++++++++++++++++++\n");
	faddr = 0x80000;
	j = 0;
	while(1)
        {

                w25p16_date.addr = faddr;
                w25p16_date.len =  0x100;
                memset(w25p16_date.buf, 0x00, sizeof(w25p16_date.buf));
        
                ret = ioctl(fd, W25P16_READ, (unsigned long)&w25p16_date);
                if (ret != 0)
                {
                        printf("after erase read FAILD");
                        break;
                }
                for( i = 0; i <  0x100; i++)
                {
                        if( (i %16) == 0)
                                printf("\n");
                        printf("%02X ",w25p16_date.buf[i]);
                }
                faddr += 0x100;
                j++;
                if(j>50)
                {
                        printf("j>8192\n");
                        break;
                }
	}
	ioctl(fd, SPI_IOC_OPER_FLASH_DONE, NULL);
	close(fd);
	return 0;
#if 0
	if(write(fd , &i , 2) < 0)
	{
		printf("FPGA write error\n");
		return -1;
	}
	if(read(fd , &data , 2) < 0)
	{
		printf("FPGA read error\n");
		return -1;
	}
	if(i!=data)
	
		printf("FPGA i=0x%x,data=0x%x\n",i,data);
		return 0;
	}
	else
	{
	}
	i++;
	if(i>0xfffe)
		i = 0;
	ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL);
	sleep(100);	
#endif	
}




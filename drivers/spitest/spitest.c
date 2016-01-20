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

typedef struct spi_rdwr_argv
{
        unsigned char   cs;
        unsigned short  addr;
        unsigned short  len;
        unsigned char   buff[64];
}spi_rdwr;


#define W25_ERASE_CHIP          	_IOW(SPI_IOC_MAGIC, 9,  __u8)
#define W25_ERASE_SECTOR    		_IOW(SPI_IOC_MAGIC, 10, __u32)
#define W25P16_READ		   	_IOR(SPI_IOC_MAGIC, 11, __u32)
#define W25P16_WRITE	   		_IOW(SPI_IOC_MAGIC, 12, __u32)
#define W25P1165_ID    			_IOR(SPI_IOC_MAGIC, 13,  __u8)
#define SPI_IOC_OPER_FLASH		_IOW(SPI_IOC_MAGIC, 14, __u8)
#define SPI_IOC_OPER_FLASH_DONE		_IOW(SPI_IOC_MAGIC, 15, __u8)

#define TEST_ADDR 			0x0
#define BLOCK_SIZE			0x100



int fd,errnu_f = 0,errnu_d = 0;
spi_rdwr sopt;

int fpga_dpll_test(int pid)
{
        fd = open("/dev/spidev0.0", O_RDWR);
        if (fd == -1) {
                printf("dpll file open failed.\n");
                return -1;
        }
#if 1
	if(pid > 0)
	{
		short i = 0,data;
		while(1)
		{
			sopt.cs = 0;
                	sopt.addr = 0x1040;
			sopt.len = 4;
			sopt.buff[0] = i;

			write(fd, &sopt, sizeof(sopt));
			usleep(1000);
			read(fd, &sopt, sizeof(sopt));
			data = sopt.buff[0];
			if(data!=i)
			{
				errnu_f++;
				printf("fpga,error,i=%x,data=%x,err=%d\n\n",i,data,errnu_f);
//				return 0;
			}
			i++;
			if(i>0xfe)
				i = 0;
			usleep(1000);
		}
	}
	else
#endif
	{
		unsigned char i = 0,data;
		while(1)
		{
                        sopt.cs = 1;
                        sopt.addr = 3;
                        sopt.len = 1;
                        sopt.buff[0] = i;

                        write(fd, &sopt, sizeof(sopt));
			usleep(1000);
                        read(fd, &sopt, sizeof(sopt));
                        data = sopt.buff[0];
                        if(data!=i)
                        {
                                errnu_f++;
                                printf("dpll,error,i=%x,data=%x,err=%d\n\n",i,data,errnu_f);
//                                return 0;
                        }
                        i++;
                        if(i>0xfe)
                                i = 0;
                        usleep(1000);
                }
	}
}

int main(int argc, char *argv[])
{
	int pid;


	pid = fork();
	pid = fork();
//	pid = 0;
	
	printf("pid = %d\n",pid);
	fpga_dpll_test(pid);
	sleep(1);
	printf("pid = %d,error out\n",pid);
while(1);
	return 0;
}




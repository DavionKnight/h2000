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
#include <pthread.h>

pthread_mutex_t mutex_fpga= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_dpll= PTHREAD_MUTEX_INITIALIZER;

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

int fpga_dpll_test(void *arg)
{
        fd = open("/dev/spidev0.0", O_RDWR);
        if (fd == -1) {
                printf("dpll file open failed.\n");
                return -1;
        }
	printf("arg=%d\n",*((char*)arg));
#if 1
	if(*((char*)arg) == 0)
	{
		short i = 0,data;
		while(1)
		{
pthread_mutex_lock(&mutex_fpga);
			sopt.cs = 0;
                	sopt.addr = 0x1040;
			sopt.len = 4;
			sopt.buff[0] = i;
			sopt.buff[1] = i;
			sopt.buff[2] = i;
			sopt.buff[3] = i;

			write(fd, &sopt, sizeof(sopt));
			usleep(100);
			read(fd, &sopt, sizeof(sopt));
			data = sopt.buff[0];
pthread_mutex_unlock(&mutex_fpga);
			if(data!=i)
			{
				errnu_f++;
				printf("fpga,error,i=%x,data=%x %x %x %x,err=%d\n\n",i,data,sopt.buff[1],sopt.buff[2],sopt.buff[3],errnu_f);
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
pthread_mutex_lock(&mutex_dpll);
                        sopt.cs = 1;
                        sopt.addr = 3;
                        sopt.len = 1;
                        sopt.buff[0] = i;

                        write(fd, &sopt, sizeof(sopt));
			usleep(100);
                        read(fd, &sopt, sizeof(sopt));
                        data = sopt.buff[0];
pthread_mutex_lock(&mutex_dpll);
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
	char arg1 = 0, arg2 = 1;
	pthread_t id_1,id_2,id_3,id_4,id_5;


	pthread_create(&id_1, NULL, (void *)fpga_dpll_test, &arg1);
	pthread_create(&id_2, NULL, (void *)fpga_dpll_test, &arg1);
	pthread_create(&id_3, NULL, (void *)fpga_dpll_test, &arg2);
	pthread_create(&id_4, NULL, (void *)fpga_dpll_test, &arg2);
	pthread_create(&id_5, NULL, (void *)fpga_dpll_test, &arg1);
	sleep(1);
while(1);
	return 0;
}




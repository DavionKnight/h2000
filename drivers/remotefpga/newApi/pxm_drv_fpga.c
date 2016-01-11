
#include <stdio.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

#include "err_code.h"
#include "pxm_drv_fpga.h"

#define	FPGA_DRV_NAME			"/dev/spidev0.0"
#define SPI_IOC_MAGIC			'k'
#define SPI_IOC_OPER_FPGA		_IOW(SPI_IOC_MAGIC, 5, unsigned char)
#define SPI_IOC_OPER_FPGA_DONE	_IOW(SPI_IOC_MAGIC, 6, unsigned char)
#define SPI_IOC_OPER_DPLL		_IOW(SPI_IOC_MAGIC, 7, unsigned char)
#define SPI_IOC_OPER_DPLL_DONE	_IOW(SPI_IOC_MAGIC, 8, unsigned char)

#pragma pack(1) 
typedef struct s_fpga_argv
{
    unsigned short	addr;    // register address to read or write
    unsigned char	flag;    // read or write flag; 0: read, 1:write
    unsigned short	size;    // length to read or write
    unsigned char	buff[4]; // buffer to store the data
}	s_FPGA_ARGV;
#pragma pack() 

int					g_fpga_fd = -1;

int pxm_fpga_init(void)
{
	if (g_fpga_fd < 0)
	{
		g_fpga_fd = open(FPGA_DRV_NAME, O_RDWR);
//		ioctl(g_fpga_fd, SPI_IOC_OPER_FPGA_DONE, NULL);
	}
	if (g_fpga_fd < 0)
	{
		return ERR_FPGA_DRV_OPEN;
	}

	return ERR_NONE;
}

int pxm_fpga_close(void)
{
	if (g_fpga_fd > 0)
	{
		close(g_fpga_fd);
//		ioctl(g_fpga_fd, SPI_IOC_OPER_FPGA_DONE, NULL);
	}

	return ERR_NONE;
}

int pxm_fpga_lc_rd(unsigned short addr, unsigned int *data)
{
	struct s_fpga_argv	argv;

	if (g_fpga_fd < 0)
	{
		return ERR_FPGA_DRV_OPEN;
	}
	if (NULL == data)
	{
		return ERR_FPGA_DRV_ARGV;
	}

	argv.addr	= addr;
	argv.flag	= 0;
	argv.size	= 4;

	ioctl(g_fpga_fd, SPI_IOC_OPER_FPGA, NULL);
	lseek(g_fpga_fd, argv.addr, SEEK_SET);
	read (g_fpga_fd, argv.buff, argv.size);
	ioctl(g_fpga_fd, SPI_IOC_OPER_FPGA_DONE, NULL);

	*data		= ((unsigned int)argv.buff[0] << 24)
				| ((unsigned int)argv.buff[1] << 16)
				| ((unsigned int)argv.buff[2] <<  8)
				| ((unsigned int)argv.buff[3]);

	return ERR_NONE;
}
int pxm_fpga_lc_rd_len(unsigned short addr, unsigned int *data, unsigned short len)
{
	struct s_fpga_argv	argv;
	unsigned char rddata[64]={0}, *ptr = rddata;
	unsigned int i = 0;

	if (g_fpga_fd < 0)
	{
		return ERR_FPGA_DRV_OPEN;
	}
	if ((NULL == data) || (len > 64)||(len%4))
	{
		return ERR_FPGA_DRV_ARGV;
	}

	argv.addr	= addr;
	argv.flag	= 0;
	argv.size	= len;

	ioctl(g_fpga_fd, SPI_IOC_OPER_FPGA, NULL);
	lseek(g_fpga_fd, argv.addr, SEEK_SET);
	read (g_fpga_fd, rddata, argv.size);
	ioctl(g_fpga_fd, SPI_IOC_OPER_FPGA_DONE, NULL);
	pxm_fpga_lc_rd(0, &i);

	for(i = 0; i < (argv.size/4); i++)
	{
		data[i]		= ((unsigned int)ptr[0] << 24)
					| ((unsigned int)ptr[1] << 16)
					| ((unsigned int)ptr[2] <<  8)
					| ((unsigned int)ptr[3]);
		ptr +=4;
	}
	return ERR_NONE;

}

int pxm_fpga_lc_wr(unsigned short addr, unsigned int data)
{
	struct s_fpga_argv	argv;

	if (g_fpga_fd < 0)
	{
		return ERR_FPGA_DRV_OPEN;
	}

	argv.addr		= addr;
	argv.flag		= 1;
	argv.size		= 4;
	argv.buff[0]	= (unsigned char)(data >> 24);
	argv.buff[1]	= (unsigned char)(data >> 16);
	argv.buff[2]	= (unsigned char)(data >>  8);
	argv.buff[3]	= (unsigned char)(data);

	ioctl(g_fpga_fd, SPI_IOC_OPER_FPGA, NULL);
	lseek(g_fpga_fd, argv.addr, SEEK_SET);
	write(g_fpga_fd, argv.buff, argv.size);
	ioctl(g_fpga_fd, SPI_IOC_OPER_FPGA_DONE, NULL);

	return ERR_NONE;
}

#if 1
int main()
{
	unsigned char slot;
	unsigned short addr, data[32]={0}, i =0,wrdata[10]={0};
	unsigned int size;
	
	pxm_fpga_init();
	slot = 0;
	addr = 0;
	size = 32;
	pxm_fpga_rm_cr_rd_set(0, 3, 0, 32);
	pxm_fpga_rm_cr_rd_set(1, 3, 0x80, 32);
	pxm_fpga_rm_cr_rd_inf(0, &slot, &addr, &size);
	printf("slot %d, addr %x, size %d\n",slot, addr, size);
	
	pxm_fpga_rm_cr_rd_inf(1, &slot, &addr, &size);
	printf("slot %d, addr %x, size %d\n",slot, addr, size);

	pxm_fpga_rm_cr_en(0);
	pxm_fpga_rm_cr_en(1);
	slot = 3;
	addr = 0x80;
	size = 32;
	pxm_fpga_rm_cr_rd(1, slot, addr, data, size);
	
	for(i=0;i<32;i++)
	{
		if(!(i%16))printf("\n");
		printf("0x%04x ",data[i]);
	}

	printf("\n");
	wrdata[0] = 0x0;
	wrdata[1] = 0;
	wrdata[2] = 0;
	wrdata[3] = 0x0;
	wrdata[4] = 0x0;
	wrdata[5] = 0x0;
	pxm_fpga_rm_wr(3, 0x80, wrdata, 6);

	pxm_fpga_rm_cr_en(1);
	slot = 3;
	addr = 0x80;
	size = 32;
	pxm_fpga_rm_cr_rd(1, slot, addr, data, size);
	
	for(i=0;i<32;i++)
	{
		if(!(i%16))printf("\n");
		printf("0x%04x ",data[i]);
	}

	printf("\n");
	
	memset(data,0,sizeof(data));
	pxm_fpga_rm_rt_rd(0,3,0x80,data,32,0);
	for(i=0;i<32;i++)
	{
		if(!(i%16))printf("\n");
		printf("0x%04x ",data[i]);
	}

	printf("\n");


	pxm_fpga_close();	
	return 0;
}
#endif




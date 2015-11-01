/**********************************************
 * @file	deal_fpga.c
 * @author	zhangjj <zhangjj@bjhuahuan.com>
 * @date	2015-11-4
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
#include <pthread.h>

//#define	IDTDEBUG
pthread_mutex_t mutex_cir = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_rt = PTHREAD_MUTEX_INITIALIZER;

#define UNIT_REG_BASE			0x2000
#define CTRL_STATUS_REG			(UNIT_REG_BASE+0)
#define READ_OVER_FLAG			(UNIT_REG_BASE+0x0002)
#define WRITE_ONCE_REG			(UNIT_REG_BASE+0x0010)
#define READ_ONCE_REG			(UNIT_REG_BASE+0x0020)

#define SPI_IOC_MAGIC			'k'
#define SPI_IOC_OPER_FPGA	    	_IOW(SPI_IOC_MAGIC, 5, __u8)
#define SPI_IOC_OPER_FPGA_DONE		_IOW(SPI_IOC_MAGIC, 6, __u8)

#define WORDSIZE			4

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


struct ic_oper_para{
        unsigned char slot;
        unsigned short addr;
        unsigned int len;
        unsigned short *rdbuf;
};

#define BUFF_ADDR_BASE  	UNIT_REG_BASE+0x200
#define BUFF_RT_BASE		BUFF_ADDR_BASE
#define BUFF_SIZE		0xD00
#define BUFF_RT_RD_SIZE		0x80

/*
*All clauses is 256,but the buff is too less to assigned
*so,the number of clauses is CIR_RD_CLAUSES
*/
#define BUFF_CIR_RD_BASE	(BUFF_ADDR_BASE+BUFF_RT_RD_SIZE)
#define BUFF_CIR_RD_SIZE	32
#define CIR_RD_CLAUSES		((BUFF_SIZE-BUFF_RT_RD_SIZE)*2/BUFF_CIR_RD_SIZE)

#define REG_RD_CIR_BASE   	(UNIT_REG_BASE+0x100)
#define REG_RD_CIR_EN		(UNIT_REG_BASE+0x40)

struct ic_oper_para clauses_map[CIR_RD_CLAUSES];

int fpga_rm_cir_rd_en(int clause, int en)
{
	int en_addr,en_bit;
	unsigned int data;

	if((clause > CIR_RD_CLAUSES)||((en<0) || (en >1)))
	{
		printf("clause is bigger than 220\n");
		return -1;
	}
	en_addr = clause/16;
	en_bit  = clause%16;
        if (lseek(fpga_dev, en_addr, SEEK_SET) != en_addr) {
                printf("lseek error.\n");
                return -1;
        }
	read(fpga_dev, &data, WORDSIZE);
	if(en == 1)
		data |= 1<< en_bit;
	else
		data &= ~(1<<en_bit);
        if (lseek(fpga_dev, en_addr, SEEK_SET) != en_addr) {
                printf("lseek error.\n");
                return -1;
        }
        write(fpga_dev, &data, WORDSIZE);
	return 0;
}

int fpga_rm_cir_rd_get(int clause, struct ic_oper_para *oper)
{
        if((clause < 0)||(clause >= CIR_RD_CLAUSES))
                return -1;
        if(oper->rdbuf == NULL)
        {
                printf("Read length should less than 32\n");
                return -1;
        }
	oper->slot = clauses_map[clause].slot;
	oper->addr = clauses_map[clause].addr;
	oper->len = clauses_map[clause].len;
	oper->rdbuf = clauses_map[clause].rdbuf;

	return 0;
}
int fpga_rm_cir_rd_set(int clause, struct ic_oper_para *oper)
{
        unsigned char data[WORDSIZE] = {0}, mode;
        unsigned short read_reg;
	unsigned int bufaddr = 0, count;

        if((clause<0)||(clause>=CIR_RD_CLAUSES))
                return -1;
        if((oper->len > BUFF_CIR_RD_SIZE)||(oper->rdbuf == NULL))
        {
                printf("Read length should less than 32\n");
                return -1;
        }

        clauses_map[clause].slot = oper->slot;
        clauses_map[clause].addr = oper->addr;
        clauses_map[clause].len = oper->len;

        count = oper->len;
        bufaddr = BUFF_CIR_RD_BASE + (clause*BUFF_CIR_RD_SIZE)/2;
        read_reg = REG_RD_CIR_BASE + clause;
        if(count <= 16)
        {
                mode = 2;
                data[3] = (bufaddr & 0xf0) | (count & 0xf);
        }
        else if(count <= 128)
        {
                mode = 3;
                data[3] = (bufaddr & 0x80) | (count & 0x7f);
        }
        else
        {
		printf("read count error\n");
                return -1;
        }
        data[0] = ((oper->slot & 0x0f) << 4) | ((oper->addr & 0xf00) >> 8);
        data[1] = oper->addr & 0xff;
        data[2] = ((mode & 0x07) << 5) | ((bufaddr&0x1f00) >> 8);

        if (lseek(fpga_dev, read_reg, SEEK_SET) != read_reg) {
                printf("lseek error.\n");
		return -1;
        }

        write(fpga_dev, data, WORDSIZE);
        return 0;
}

int fpga_rm_cir_rd(int clause, struct ic_oper_para *oper)
{
        unsigned char data[WORDSIZE] = {0}, mode;
	int i = 0, count;
	unsigned int buf_addr;

        if((clause < 0)||(clause >= CIR_RD_CLAUSES))
                return -1;
        if((oper->len > BUFF_CIR_RD_SIZE)||(oper->rdbuf == NULL))
        {
                printf("Read length should less than 32\n");
                return -1;
        }

        if((clauses_map[clause].slot != oper->slot) ||(clauses_map[clause].addr != oper->addr) || (clauses_map[clause].len != oper->len))
        {
                printf("Oper para is not confirm with config");
                return -1;
        }

	count = oper->len;
	buf_addr = BUFF_CIR_RD_BASE + clause * BUFF_CIR_RD_SIZE;
	
	pthread_mutex_lock(&mutex_cir);
        for(i = 0; i<(count/2 + count%2); i++)
        {
	        if (lseek(fpga_dev, (buf_addr+i), SEEK_SET) != (buf_addr + 1)) {
        	        printf("lseek error.\n");
			return -1;
        	}
                read(fpga_dev, data, WORDSIZE);
                *(oper->rdbuf + i*2) = data[2]<<8 | data[3];
                if(count>=(i+1)*2)
                *(oper->rdbuf + i*2+1) = data[0]<<8 | data[1];
        }
	pthread_mutex_unlock(&mutex_cir);
	return 0;
}


/*read 1~128 short data*/
int fpga_rm_rt_rd(unsigned char slot, unsigned short addr, unsigned short *wdata, size_t count)
{
        unsigned char data[WORDSIZE] = {0}, mode;
        unsigned int read_flag = 0, i = 0;
        unsigned int delay_count = 1000;
        unsigned short bufaddr = BUFF_RT_BASE;

        if(count <= 16)
        {
                mode = 2;
                data[3] = (bufaddr & 0xf0) | (count & 0xf);
        }
        else if(count <= 128)
        {
                mode = 3;
                data[3] = (bufaddr & 0x80) | (count & 0x7f);
        }
        else
        {
                printf("read count error\n");
                return 0;
        }
        data[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
        data[1] = addr & 0xff;
        data[2] = ((mode & 0x07) << 5) | ((bufaddr&0x1f00) >> 8);
#ifdef IDTDEBUG
        printf("Read data:\n");
        pdata(data,4);
#endif

        pthread_mutex_lock(&mutex_rt);
        if (lseek(fpga_dev, READ_ONCE_REG, SEEK_SET) != READ_ONCE_REG) {
                printf("lseek error.\n");
		return -1;
        }

        write(fpga_dev, data, WORDSIZE);

        if (lseek(fpga_dev, READ_OVER_FLAG, SEEK_SET) != READ_OVER_FLAG) {
                printf("lseek error.\n");
                return -1;
        }

        do{
                read(fpga_dev, &read_flag, WORDSIZE);
                if((read_flag == 0)||(delay_count <1))
                        break;
        }while(delay_count--);

        memset(data,0,WORDSIZE);

        for(i = 0; i<(count/2 + count%2); i++)
        {
	        if (lseek(fpga_dev, (UNIT_REG_BASE + ((bufaddr)>>1) + i), SEEK_SET) != (UNIT_REG_BASE + ((bufaddr)>>1) + i)) {
	                printf("lseek error.\n");
	                return -1;
        	}

                read(fpga_dev, data, WORDSIZE);
                *(wdata+i*2) = data[2]<<8 | data[3];
                if(count>=(i+1)*2)
                *(wdata+i*2+1) = data[0]<<8 | data[1];
        }
	pthread_mutex_unlock(&mutex_rt);

        //usleep(1);
#ifdef IDTDEBUG
        printf("Return Data:\n");
        pdata(wdata,2*count);
#endif
        return 0;
}



/********************************************************************
write any registers of the fpga, directly
return:
	0,		success
	-1,		fail to open the driver
	-2,		some parameters are out of the valid range
**********************************************************************/
int fpga_rm_wr(unsigned char slot, unsigned short addr, unsigned short* wdata)
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


int main(int argc, char *argv[])
{
	int ret = 0;
	unsigned short addr = 0,data;
	unsigned char slot_num = 0;
	int i = 0;	

	fpga_init();
#if 0
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
#endif	
	fpga_close();

	return 0;	
}




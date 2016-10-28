/**********************************************************
* file name: fpgadrv.c
* Copyright: 
	 Copyright 2016 huahuan.
* author: 
*    huahuan zhangjj 2016-01-11
* function: 
*    
* modify:
*
***********************************************************/

#include <stdio.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

#include "spi.h"
#include "spidrv.h"
#include "spidrv_common.h"

extern struct spi_device spidev;
extern struct sembuf spidrv_sembufLock, spidrv_sembufUnlock;
extern int spidrv_semid;

static unsigned char rtClause = 0;
int fpga_spi_read(unsigned int addr, unsigned char *data, size_t count, unsigned char slot)
{
	int ret = 0;
	unsigned int len = 0;
	unsigned short address = 0;

/*        if((count > FPGA_CR_CLAU_UNIT_SIZE))
        {
                printf("Read length should be less than 32 count=%d\n",count);
                return ERR_FPGA_DRV_ARGV;
        }*/
	if(!data)
		return -1;

	semop(spidrv_semid, &spidrv_sembufLock, 1);

	spidev.max_speed_hz = 6500000;//the real rate 6.25M
	spidev.chip_select = CHIPSELECT_FPGA;
	spidev.mode = SPI_MODE_3;
	spidev.bits_per_word = 8;  /*need verify*/

	spi_setup(&spidev);

	if((slot == 0x10) || (slot == 0x11)) /*read local fpga*/
	{
		mix_spi_read(&spidev, (unsigned short)addr, data, count);
	}
	else if(slot <= 0xf) /*remote board fpga*/
	{
		unsigned int rdata[FPGA_RT_CLAU_UNIT_SIZE/2] = {0};
		unsigned int reg_addr;
		unsigned char data_set[4] = {0};
		unsigned int data_rw = 0,bufaddr = 0, clause = 0;
		unsigned int value_en;
		unsigned short read_reg;
		unsigned int delay_count = 1000;
		unsigned short *pbuf = (unsigned short *)data;
		unsigned int i = 0;

		clause = rtClause++;
		if(rtClause >= FPGA_RT_CLAU)
			rtClause = 0;

//		printf("clause = %d,unitboard slot = 0x%x, addr = 0x%02x\n", clause, slot,addr);
		bufaddr = FPGA_RT_CMD_BUFF_ADDR + clause*FPGA_RT_CLAU_UNIT_SIZE;
		read_reg = FPGA_RT_CLAU_ADDR + clause;

		data_set[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
		data_set[1] = addr & 0xff;
		data_set[2] = (0x5 << 5) | ((bufaddr&0x1f00) >> 8);
		data_set[3] = ((bufaddr&0xe0)|(0x1f));

		memcpy(&data_rw,data_set,sizeof(data_rw));

		mix_spi_write(&spidev, (unsigned short)read_reg,(unsigned char *)&data_rw, sizeof(data_rw));

		do{
			mix_spi_read(&spidev, (unsigned short)FPGA_RT_RD_OVER_FLGA, (unsigned char *)&value_en, sizeof(value_en));
			if((value_en == 0)||(delay_count<1))
				break;
		}while(delay_count--);

		reg_addr = FPGA_RT_BUFF_ADDR + clause * FPGA_RT_CLAU_UNIT_SIZE/2;

		mix_spi_read(&spidev, (unsigned short)reg_addr, (unsigned char *)rdata, sizeof(rdata));
		for(i = 0; i < count/2; i++)
			pbuf[i] = i%2?((rdata[i/2]>>16)&0xffff):(rdata[i/2]&0xffff);	
	}
	else
	{
		printf("fpga_spi_read slot %d error\n", slot);
		ret = -1;
	}

	semop(spidrv_semid, &spidrv_sembufUnlock, 1);

	return ret;
}

int fpga_spi_write(unsigned int addr, unsigned char *data, size_t count, unsigned char slot)
{
	int ret = 0;
	unsigned int len = 0;
	unsigned short address = 0;

	if(!data)
		return -1;

	semop(spidrv_semid, &spidrv_sembufLock, 1);
	
	spidev.max_speed_hz = 6500000;//the real rate 6.25M
	spidev.chip_select = CHIPSELECT_FPGA;
	spidev.mode = SPI_MODE_3;
	spidev.bits_per_word = 8;  /*need verify*/

	spi_setup(&spidev);

	if((slot == 0x10) || (slot == 0x11)) /*read local fpga*/
	{
		mix_spi_write(&spidev, (unsigned short)addr, data, count);
	}
	else if(slot <= 0xf) /*remote board fpga*/
	{
		unsigned char wdata[8] = {0};
		unsigned short *ptr = (unsigned short *)data;
		unsigned int wrdata=0;


		for(ptr; ptr < (unsigned short *)(data + count); ptr++)
		{
			wdata[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
			wdata[1] = addr & 0xff;
			wdata[2] = (*ptr & 0xff00) >> 8;
			wdata[3] = *ptr & 0xff;
			memcpy(&wrdata, wdata,sizeof(wrdata));
			mix_spi_write(&spidev, FPGA_RT_WR_ADDR, (unsigned char *)&wrdata, sizeof(wrdata));
			usleep(10);
			addr ++;
		}

	}
	else
	{
		printf("fpga_spi_write slot %d error\n", slot);
		ret = -1;
	}

	semop(spidrv_semid, &spidrv_sembufUnlock, 1);

	return ret;
}



#pragma pack(1) 
typedef struct s_fpga_rm_argv
{
    unsigned char	slot;    // 0/1/2/3/11
    unsigned int 	addr;    // 0x0000~0x0FFF
    unsigned int	size;    // max FPGA_COPRO_MAX_SIZE
    unsigned short	*pbuf;	// 
    unsigned char	used;	// 
}	s_FPGA_RM_ARGV;
#pragma pack() 

/* buffer assigned
---------------------------------------0x2200
|
|	32 short * 32 clauses    rt
|--------------------------------------0x2400
|
|
|	32 short * 128 clauses   cir
|
|
|--------------------------------------0x2C00
|
|
|--------------------------------------0x2FFF
 */


//s_FPGA_RM_ARGV clausRtMap[FPGA_RT_CLAU];
s_FPGA_RM_ARGV clausCrMap[FPGA_CR_CLAU] = {0};

int fpga_read_remote_set(int clause, unsigned char slot, unsigned int addr, unsigned int size)
{
	unsigned char data[4] = {0};
	unsigned short read_reg;
	unsigned int data_rw = 0,bufaddr = 0, count;

	if((clause<0)||(clause>=FPGA_CR_CLAU))
		return ERR_FPGA_DRV_ARGV;

	if((size > FPGA_CR_CLAU_UNIT_SIZE))
	{
		printf("Read length should be less than 32, size = %d\n", size);
		return ERR_FPGA_DRV_ARGV;
	}

	clausCrMap[clause].slot = slot;
	clausCrMap[clause].addr = addr;
	clausCrMap[clause].size = size;
	clausCrMap[clause].used = 1;

	bufaddr = FPGA_CR_CMD_BUFF_ADDR + clause*FPGA_CR_CLAU_UNIT_SIZE;
	read_reg = FPGA_CR_CLAU_ADDR + clause;

	data[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
	data[1] = addr & 0xff;
	data[2] = (0x5<< 5) | ((bufaddr&0x1f00) >> 8);
	data[3] = ((bufaddr&0xe0)|(0x1f));

	memcpy(&data_rw,data,sizeof(data_rw));
	//	printf("data_rw=0x%x data3=0x%x size=0x%x\n",data_rw,data[3],size);
	return fpga_spi_write(read_reg, (unsigned char *)&data_rw, sizeof(data), 0x10);

}

int fpga_read_remote_get(int *clause)
{
	int i = 0;
	
	for(i = 0; i < FPGA_CR_CLAU; i++)
	{
		if(!clausCrMap[i].used)
		{
			break;	
		}
	}
	if(FPGA_CR_CLAU == i)
		return -1;
	*clause = i;
	return 0;
}

int fpga_read_remote_inf(int clause, unsigned char *slot, unsigned int *addr, unsigned int *size)
{
	if((clause < 0)||(clause >= FPGA_CR_CLAU))
		return -1;

	*slot = clausCrMap[clause].slot;
	*addr = clausCrMap[clause].addr;
	*size = clausCrMap[clause].size;

	return 0;
}
int fpga_read_remote_en(int clause)
{
	unsigned int en_addr, en_bit;
	unsigned int data, delay_count = 1000;

	if(clause > FPGA_CR_CLAU)
	{
		return ERR_FPGA_DRV_ARGV;
	}
	en_addr = FPGA_CR_EN_ADDR + clause/16;
	en_bit  = clause%16;

	do{
		fpga_spi_read(en_addr, (unsigned char *)&data, sizeof(data), 0x10);
		if(((data & 0xffff) == 0)||(delay_count<1))
			break;
	}while(delay_count--);

	data = 1<<en_bit;
	//	printf("en_addr=0x%x,en data=0x%x\n",en_addr,data);
	return fpga_spi_write(en_addr, (unsigned char *)&data, sizeof(data), 0x10);
}

/*
 *set circle read enable by enbuf
 *size: 0 ~ 7
 *
 */
int fpga_read_remote_block_en(unsigned short *enbuf, unsigned int size)
{
	unsigned int en_addr, i = 0;
	unsigned int data, delay_count = 1000;

	if((NULL == enbuf)||(size >=8))
	{
		return -1;
	}
	for(i = 0; i <size; i++)
	{
		en_addr = FPGA_CR_EN_ADDR + i;
		do{
			fpga_spi_read(en_addr, (unsigned char *)&data, sizeof(data), 0x10);
			if(((data & 0xffff) == 0)||(delay_count<1))
				break;
		}while(delay_count--);

		data = 0;
		data |= enbuf[i];
		fpga_spi_write(en_addr, (unsigned char *)&data, sizeof(data), 0x10);
	}
	return 0;
}

/*
 * cir rd data
 *   mode: 0, check status, default
 *         1, do not check
 */
int fpga_read_remote(int clause, unsigned char slot, unsigned int addr, unsigned short *pbuf, unsigned int size)
{
	unsigned int data[FPGA_CR_CLAU_UNIT_SIZE/2] = {0}, mode;
	int i = 0;
	unsigned int reg_addr;
	unsigned int en_addr,en_bit;
	unsigned int delay_count = 1000;
	unsigned int value_en;

	if((clause < 0)||(clause >= FPGA_CR_CLAU))
		return -1;
	if(!pbuf)
		return -1;
	if((clausCrMap[clause].slot != slot) ||(clausCrMap[clause].addr != addr) || (clausCrMap[clause].size != size))
	{
		printf("fpga_rm_cir_read para is not confirm with configure\n");
		return -1;
	}

	en_addr = FPGA_CR_EN_ADDR + clause/16;
	en_bit  = clause%16;

	do{
		fpga_spi_read(en_addr, (unsigned char *)&value_en, sizeof(value_en), 0x10);
		if(((value_en&(1 << en_bit)) == 0)||(delay_count<1))
			break;
	}while(delay_count--);

	reg_addr = FPGA_CR_BUFF_ADDR + clause * FPGA_CR_CLAU_UNIT_SIZE/2;

	fpga_spi_read(reg_addr, (unsigned char *)data, sizeof(data), 0x10);
	for(i = 0; i < size; i++)
		pbuf[i] = i%2?((data[i/2]>>16)&0xffff):(data[i/2]&0xffff);

	return 0;

}
#if 0
int fpga_rm_rt_read(int clause, unsigned char slot, unsigned short addr, unsigned short *pbuf, unsigned int size)
{
	unsigned int data[FPGA_RT_CLAU_UNIT_SIZE/2] = {0};
	unsigned char data_set[4] = {0};
	int i = 0;
	unsigned int reg_addr;
	unsigned int delay_count = 1000;
	unsigned int value_en;
	unsigned int data_rw = 0,bufaddr = 0;
	unsigned short read_reg;

	if((clause < 0)||(clause >= FPGA_RT_CLAU))
		return ERR_FPGA_DRV_ARGV;


	bufaddr = FPGA_RT_CMD_BUFF_ADDR + clause*FPGA_RT_CLAU_UNIT_SIZE;
	read_reg = FPGA_RT_CLAU_ADDR + clause;

	data_set[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
	data_set[1] = addr & 0xff;
	data_set[2] = (0x5 << 5) | ((bufaddr&0x1f00) >> 8);
	data_set[3] = ((bufaddr&0xe0)|(0x1f));

	memcpy(&data_rw,data_set,sizeof(data_rw));

	fpga_spi_write(read_reg, (unsigned char *)&data_rw, sizeof(data_set), 0);

	do{
		fpga_spi_read(FPGA_RT_RD_OVER_FLGA, (unsigned char *)&value_en, sizeof(value_en), 0);
		if((value_en == 0)||(delay_count<1))
			break;
	}while(delay_count--);

	reg_addr = FPGA_RT_BUFF_ADDR + clause * FPGA_RT_CLAU_UNIT_SIZE/2;

	//	pthread_mutex_lock(&mutex_cir);

	fpga_spi_read(reg_addr, (unsigned char *)data, sizeof(data), 0);
	for(i = 0; i < size; i++)
		pbuf[i] = i%2?((data[i/2]>>16)&0xffff):(data[i/2]&0xffff);

	//	pthread_mutex_unlock(&mutex_cir);

	return 0;


}
int fpga_rm_rt_write(unsigned char slot, unsigned short addr, unsigned short *pbuf, unsigned int size)
{
	unsigned char data[8] = {0};
	unsigned short *ptr = pbuf;
	unsigned int wrdata=0;

	if(!pbuf)
		return ERR_FPGA_DRV_ARGV;

	for(ptr; ptr < (pbuf+size); ptr++)
	{
		data[0] = ((slot & 0x0f) << 4) | ((addr & 0xf00) >> 8);
		data[1] = addr & 0xff;
		data[2] = (*ptr & 0xff00) >> 8;
		data[3] = *ptr & 0xff;
		memcpy(&wrdata, data,sizeof(wrdata));
		fpga_spi_write(FPGA_RT_WR_ADDR, (unsigned char *)&wrdata, sizeof(wrdata), 0);
		usleep(10);
		addr ++;
	}
	return 0;

}

#endif





/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:Kevin_fzs
*  File Name        	:/home/kevin/works/projects/H20PN-2000/drivers/spidrv/spidrv.h
*  Create Date        	:2016/09/26 04:26
*  Last Modified      	:2016/09/26 04:26
*  Description    	:
*/

#define		ERR_NONE		 		 0

#define		ERR_FPGA_DRV_OPEN		-1
#define		ERR_FPGA_DRV_RDWR		-2
#define		ERR_FPGA_DRV_TIMEOUT	-3
#define		ERR_FPGA_DRV_LOCK		-4
#define		ERR_FPGA_DRV_ARGV		-5

int spidrv_init();

int spidrv_exit();

int fpga_spi_read(unsigned short addr, unsigned char *data, size_t count, unsigned char slot);

int fpga_spi_write(unsigned short addr, unsigned char *data, size_t count, unsigned char slot);

int dpll_spi_read(unsigned short addr, unsigned char *data, size_t count);

int dpll_spi_write(unsigned short addr, unsigned char *data, size_t count);

/*读远端fpga比较复杂，需要4个步骤 获取->配置->使能->读
先调用fpga_spi_cir_read_inf获取*/
int fpga_rm_cir_read_get(int *clause);

int fpga_rm_cir_read_set(int clause, unsigned char slot, unsigned short addr, unsigned int size);

int fpga_rm_cir_read_inf(int clause, unsigned char *slot, unsigned short *addr, unsigned int *size);

int fpga_rm_cir_en(int clause);

int fpga_rm_cir_en_blk(unsigned short *enbuf, unsigned int size);

int fpga_rm_cir_read(int clause, unsigned char slot, unsigned short addr, unsigned short *pbuf, unsigned int size);

int fpga_rm_rt_read(int clause, unsigned char slot, unsigned short addr, unsigned short *pbuf, unsigned int size);

int fpga_rm_rt_write(unsigned char slot, unsigned short addr, unsigned short *pbuf, unsigned int size);




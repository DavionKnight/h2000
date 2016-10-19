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


int spidrv_init();

int spidrv_exit();

int fpga_spi_read(unsigned short addr, unsigned char *data, size_t count, unsigned char slot);

int fpga_spi_write(unsigned short addr, unsigned char *data, size_t count, unsigned char slot);

int dpll_spi_read(unsigned short addr, unsigned char *data, size_t count, unsigned char slot);

int dpll_spi_write(unsigned short addr, unsigned char *data, size_t count, unsigned char slot);



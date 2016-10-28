/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:fzs
*  File Name        	:/home/kevin/works/projects/H20RN-2000/drivers/spidrv/spidrv_common.h
*  Create Date        	:2016/10/27 05:42
*  Last Modified      	:2016/10/27 05:42
*  Description    	:
*/

#define		ERR_NONE		 		 0

#define		ERR_FPGA_DRV_OPEN		-1
#define		ERR_FPGA_DRV_RDWR		-2
#define		ERR_FPGA_DRV_TIMEOUT	-3
#define		ERR_FPGA_DRV_LOCK		-4
#define		ERR_FPGA_DRV_ARGV		-5

#define CHIPSELECT_FPGA		0
#define CHIPSELECT_DPLL		1
#define CHIPSELECT_EPCS		2

/* Flash opcodes. */
#define GPIO_FPGAFLASH         12
#define	OPCODE_WREN		0x06	/* Write enable */
#define	OPCODE_RDSR		0x05	/* Read status register */
#define	OPCODE_WRSR		0x01	/* Write status register 1 byte */
#define	OPCODE_CHIP_ERASE	0xc7	/* Erase whole flash chip */
#define	OPCODE_ERASE_SECTOR	0xd8	/* Sector erase (usually 64KiB) */
#define	OPCODE_NORM_READ	0x03	/* Read data bytes (low frequency) */
#define	OPCODE_FAST_READ	0x0b	/* Read data bytes (high frequency) */
#define OPCODE_READ 	OPCODE_NORM_READ
#define OPCODE_WRITE 		0x02	


#define SPI_FPGA_WR_SINGLE 0x01
#define SPI_FPGA_WR_BURST  0x02
#define SPI_FPGA_RD_BURST  0x03
#define SPI_FPGA_RD_SINGLE 0x05

#define	FPGA_RT_CLAU_ADDR		(0x2020)
#define	FPGA_RT_BUFF_ADDR		(0x2200)
#define	FPGA_RT_CMD_BUFF_ADDR	(0x0400)
#define	FPGA_RT_CLAU_UNIT_SIZE	(0x0020)
#define FPGA_RT_RD_OVER_FLGA	(0x2002)

#define FPGA_RT_WR_ADDR			(0x2010)

//#define	FPGA_CR_CLAU_ADDR		(0x2020)
#define	FPGA_CR_CLAU_ADDR		(0x2100)
#define	FPGA_CR_BUFF_ADDR		(0x2400)
#define	FPGA_CR_CMD_BUFF_ADDR	(0x0800)
#define	FPGA_CR_CLAU_UNIT_SIZE	(0x0020)
#define FPGA_CR_EN_ADDR			(0x2040)

#define FPGA_RT_CLAU			32
#define FPGA_CR_CLAU			128
#define FPGA_CLAUSE_NUM			168


union semun
{
  int val;			/* value for SETVAL */
  struct spidrv_semid_ds *buf;		/* buffer for IPC_STAT & IPC_SET */
  unsigned short int *array;	/* array for GETALL & SETALL */
  struct seminfo *__buf;	/* buffer for IPC_INFO */
  struct __old_spidrv_semid_ds *__old_buf;
};

#define __SPIDRV_DEBUG__  

#ifdef __SPIDRV_DEBUG__  
#define SPIDRV_PRINT(format,...) printf("File: "__FILE__", Line: %05d: "format"\n", __LINE__, ##__VA_ARGS__)  
#else  
#define SPIDRV_PRINT(format,...) zlog() /*need add*/
#endif  




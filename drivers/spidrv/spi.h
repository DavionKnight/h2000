/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:Kevin_fzs
*  File Name        	:/home/kevin/works/projects/H20PN-2000/drivers/spidrv/spi.h
*  Create Date        	:2016/09/23 02:17
*  Last Modified      	:2016/09/23 02:17
*  Description    	:
*/
#define	SPI_CPHA	0x01			/* clock phase */
#define	SPI_CPOL	0x02			/* clock polarity */
#define	SPI_MODE_0	(0|0)			/* (original MicroWire) */
#define	SPI_MODE_1	(0|SPI_CPHA)
#define	SPI_MODE_2	(SPI_CPOL|0)
#define	SPI_MODE_3	(SPI_CPOL|SPI_CPHA)
#define	SPI_CS_HIGH	0x04			/* chipselect active high? */
#define	SPI_LSB_FIRST	0x08			/* per-word bits-on-wire */
#define	SPI_3WIRE	0x10			/* SI/SO signals shared */
#define	SPI_LOOP	0x20			/* loopback mode */
#define	SPI_NO_CS	0x40			/* 1 dev/bus, no chipselect */
#define	SPI_READY	0x80			/* slave pulls low to pause */

struct spi_reg_t {
	unsigned int mode;
	unsigned int event;
	unsigned int mask;
	unsigned int command;
	unsigned int transmit;
	unsigned int receive;
	unsigned int res[2];
	unsigned int csmode[4];
};
struct spi_device {
	volatile struct spi_reg_t	*spi_reg;
	unsigned int			max_speed_hz;
	unsigned char			chip_select;
	unsigned char			mode;
	unsigned char			bits_per_word;
	int			irq;
};

int spi_transfer(struct spi_device *spidev, unsigned char *txbuf, unsigned char *rxbuf, int len);
int spi_dev_init(struct spi_device *spidev);
int spi_setup(struct spi_device *spidev);




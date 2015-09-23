
#ifndef _FPGA_H_
#define _FPGA_H_

int fpga_spi_write(unsigned short addr, unsigned char *data, size_t count);
int fpga_spi_read(unsigned short addr, unsigned char *data, size_t count);

#define SPI_FPGA_WR_SINGLE 0x01
#define SPI_FPGA_WR_BURST  0x02
#define SPI_FPGA_RD_BURST  0x03
#define SPI_FPGA_RD_SINGLE 0x05

#endif /* _FPGA_H_ */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>		// for strcpy()

#include <sys/types.h>	// for open lseek
#include <sys/stat.h>	// for open
#include <fcntl.h>		// for open

#include <unistd.h>		// for lseek read write
#include "spidev.h"
#include <sys/ioctl.h>

typedef struct spi_rdwr_argv
{
        unsigned char   cs;
        unsigned short  addr;
        unsigned short  len;
        unsigned char   buff[64];
}spi_rdwr;

void usage(void)
{
	printf("fpga_demo read <reg:hex> <len:dec>\n");
	printf("fpga_demo write <reg:hex> <val:hex>\n");
	printf("fpga_demo test\n");
}

void pdata(unsigned char *pdata, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		printf(" %02x", pdata[i]);
	}
}

void selftest(int fd)
{
	int count = 0;
	unsigned char data[32] = {0};
	
	printf("ds31400 spi self test:\n");
	
	printf("single read 0x00 ID1\n");
	if (lseek(fd, 0x00, SEEK_SET) != 0x00) {
		printf("lseek error.\n");
		return;
	}
	count = 1;
	printf("result:");
	if (read(fd, data, (size_t)count) == count) {
		pdata(data, count);
		printf(" done.\n");
	} else {
		printf(" error.\n");
		return;
	}

	printf("burst read 0x00 ID[1:2]\n");
	if (lseek(fd, 0x00, SEEK_SET) != 0x00) {
		printf("lseek error.\n");
		return;
	}
	count = 2;
	printf("result:");
	if (read(fd, data, (size_t)count) == count) {
		pdata(data, count);
		printf(" done.\n");
	} else {
		printf(" error.\n");
		return;
	}

	printf("single write 0x60 ICSEL = 0x01\n");
	if (lseek(fd, 0x60, SEEK_SET) != 0x60) {
		printf("lseek error.\n");
		return;
	}
	count = 1;
	data[0] = 0x01;
	printf("result:");
	if (write(fd, data, (size_t)count) == count) {
		printf(" done.\n");
	} else {
		printf(" error.\n");
		return;
	}

	printf("single read 0x60 ICSEL\n");
	if (lseek(fd, 0x60, SEEK_SET) != 0x60) {
		printf("lseek error.\n");
		return;
	}
	count = 1;
	printf("result:");
	if (read(fd, data, (size_t)count) == count) {
		pdata(data, count);
		printf(" done.\n");
	} else {
		printf(" error.\n");
		return;
	}

	printf("burst write 0x66 ICD[1:4] = 1a 2b 3c 4d\n");
	if (lseek(fd, 0x66, SEEK_SET) != 0x66) {
		printf("lseek error.\n");
		return;
	}
	count = 4;
	data[0] = 0x1a; data[1] = 0x2b;
	data[2] = 0x3c; data[3] = 0x4d;
	printf("result:");
	if (write(fd, data, (size_t)count) == count) {
		printf(" done.\n");
	} else {
		printf(" error.\n");
		return;
	}

	printf("burst read 0x66 ICD[1:4]\n");
	if (lseek(fd, 0x66, SEEK_SET) != 0x66) {
		printf("lseek error.\n");
		return;
	}
	count = 4;
	printf("result:");
	if (read(fd, data, (size_t)count) == count) {
		pdata(data, count);
		printf(" done.\n");
	} else {
		printf(" error.\n");
		return;
	}
}

int main(int argc, char *argv[])
{
	int fd;
	int count = 0; 
	unsigned int addr = 0;
	unsigned char data[32] = {0};
	int ret;
	spi_rdwr sopt;

	if (argc != 2 && argc != 4) {
		usage();
		return 0;
	}
	
	fd = open("/dev/spidev0.0", O_RDWR);
	if (fd == -1) {
		printf("dpll file open failed.\n");
		return -1;
	}

	if (argc == 2 && argv[1][0] == 't') {
		selftest(fd);
	}
	else if (argc == 4 && argv[1][0] == 'r') {
		sopt.cs = 0;
		sscanf(argv[2], "%hx", &sopt.addr);
		sscanf(argv[3], "%hd", &sopt.len);
		printf("read %d bytes at 0x%04x:", sopt.len, (unsigned short)sopt.addr);
		if (read(fd, &sopt, 0) == sopt.len) {
			pdata(sopt.buff, sopt.len);
			printf(" done\n");
		} else {
			printf(" error\n");
		}
	}
	else if (argc == 4 && argv[1][0] == 'w') {
		sopt.cs = 0;
		sscanf(argv[2], "%hx", &sopt.addr);

		sopt.len = strlen(argv[3]);
		if (count % 2 != 0) {
			printf("error: unaligned byte:hex\n");
		} else {
			int i; char *cp;
			unsigned char tmp;
			cp = argv[3];
			for(i = 0; *cp; i++, cp++) {
				tmp = *cp - '0';
				if(tmp > 9)
					tmp -= ('A' - '0') - 10;
				if(tmp > 15)
					tmp -= ('a' - 'A');
				if(tmp > 15) {
					printf("Hex conversion error on %c, mark as 0.\n", *cp);
					tmp = 0;
				}
				if((i % 2) == 0)
					sopt.buff[i / 2] = (tmp << 4);
				else
					sopt.buff[i / 2] |= tmp;
			}

			sopt.len >>= 1;
			printf("write %d bytes at 0x%04x:", sopt.len, (unsigned short)sopt.addr);
			pdata(sopt.buff, sopt.len);
			if (write(fd, &sopt, 0) == sopt.len) 
				printf(" done\n");
			else
				printf(" error\n");
		}
	}
	else {
		usage();
	}
	close(fd);
	
	return 0;
}


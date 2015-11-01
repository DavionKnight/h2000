#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <pthread.h>

struct ic_oper_para{
        unsigned char slot;
        unsigned short addr;
        unsigned int len;
        unsigned short *rdbuf;
};

/* ---------------------------------------------------------------- 
fpga_init
must be called first !!!
return:
        0,              success
        -1,             fail to open the driver
*******************************************************************/
int fpga_init(void);
int fpga_close(void);

int fpga_rm_cir_rd_en(int clause, int en);

int fpga_rm_cir_rd_get(int clause, struct ic_oper_para *oper);
int fpga_rm_cir_rd_set(int clause, struct ic_oper_para *oper);

int fpga_rm_cir_rd(int clause, struct ic_oper_para *oper);

int fpga_rm_rt_rd(unsigned char slot, unsigned short addr, unsigned short *wdata, size_t count);

int fpga_rm_wr(unsigned char slot, unsigned short addr, unsigned short* wdata);






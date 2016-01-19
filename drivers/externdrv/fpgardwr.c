#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
//#include <bdinfo.h>
//#include <bd_export_info.h>
#include "api.h"

/* print the data, used for debugging */

void pdata(unsigned char *pdata, int count)
{
#ifdef DEBUG
    int i;
    for (i = 0; i < count; i++)
    {
        printf(" %02x", pdata[i]);
    }
#else
#endif
}


/* read data from fpga */
int read_fpga_data(int fd, struct fpga_msg *msg, int step)
{
    if(fd<0)
        return (-1);
    lseek(fd, (*msg).addr, SEEK_SET);
    if (read(fd, (*msg).buf, (*msg).len) == (*msg).len) {
        pdata((*msg).buf, (*msg).len);
    } else {
        cdebug(" \nstep %d error\n", step);
        return (-1);
    }
    return 1;
}
/*write data to fpga */
int write_fpga_data(int fd, struct fpga_msg *msg, int step)
{
    if(fd < 0)
        return (-1);
    lseek(fd, (*msg).addr, SEEK_SET);
    cdebug("step %d begin ==== write %d bytes at 0x%04x:", step, (*msg).len, (unsigned short)(*msg).addr);
    if (write(fd, (*msg).buf, (*msg).len) == (*msg).len){
        pdata((*msg).buf, (*msg).len);
        cdebug("\nstep %d done\n", step);
    } else{
        cdebug(" step %d error\n", step);
        return (-1);
    }
    return 1;
}




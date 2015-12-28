/*
 * @file		switchport.c
 * @author	Tianzhy <tianzy@huahuan.com>
 * @date		2013-8-22
 */
#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <bdinfo.h>
#include <bd_export_info.h>

#include "api.h"

int switchDevFd;
int gpioDevFd;

static int bdtype = 0;

int switch_total_RJ45100M_port_num = 0;
int switch_total_SFP100M_port_num = 0;
int switch_total_RJ451G_port_num = 0;
int switch_total_SFP1G_port_num = 0;

int switch_total_XFP10G_port_num = 0;

int switch_total_logic_port_num = 0;
int switch_board_type = 0;
int switch_logic_port_min_id = 0;
int switch_logic_port_max_id = 0;
bdinfo_port *switch_bdinfo_port = NULL;


int switchadmin_app_global_init(void)
{
  /*
    int switchDevFd;
    int error = 0;

    switchDevFd = open("/dev/switch", O_RDONLY);   
    if (switchDevFd < 0)
    {
        cdebug( "Opening switch device '/dev/switch'  failed: %d " , switchDevFd); 
        error = -1;
        goto exit;
    }

    if (ioctl(switchDevFd, IOCTL_getBoardType, &switch_board_type) < 0) 
    {
        cdebug(" SWITCH getting board info error.");
        error = -1;
        goto exit;
    }

    if (ioctl(switchDevFd, IOCTL_getTotalPortsNum, 
                (int *)&switch_total_logic_port_num) < 0)
    {
        cdebug( " SWITCH getting total ports num error.");
        error = -1;
        goto exit;
    }

    if (ioctl(switchDevFd, IOCTL_getSFP100MPortsNum, 
                (int *)&switch_total_SFP100M_port_num) < 0)
    {
        cdebug( " SWITCH getting total SFP100M ports num error.");
        error = -1;
        goto exit;
    }

    if (ioctl(switchDevFd, IOCTL_getRJ45100MPortsNum, 
                (int *)&switch_total_RJ45100M_port_num) < 0)
    {
        cdebug( " SWITCH getting total RJ45100M ports num error.");
        error = -1;
        goto exit;
    }
    if (ioctl(switchDevFd, IOCTL_getRJ451GPortsNum, 
                (int *)&switch_total_RJ451G_port_num) < 0)
    {
        cdebug( " SWITCH getting total RJ451G ports num error.");
        error = -1;
        goto exit;
    }
    if (ioctl(switchDevFd, IOCTL_getSFP1GPortsNum, 
                (int *)&switch_total_SFP1G_port_num) < 0)
    {
        cdebug( " SWITCH getting total SFP1G ports num error.");
        error = -1;
        goto exit;
    }
    if (ioctl(switchDevFd, IOCTL_getXFP10GPortsNum, 
                (int *)&switch_total_XFP10G_port_num) < 0)
    {
        cdebug( " SWITCH getting total XFP10G ports num error.");
        error = -1;
        goto exit;
    }

    if (switch_total_logic_port_num <= 0)
    {
        error = -1;
        goto exit;
    }

    switch_logic_port_min_id = 1;
    switch_logic_port_max_id = 
        switch_logic_port_min_id + switch_total_logic_port_num - 1;
    switch_bdinfo_port = (bdinfo_port *)
        malloc(sizeof(bdinfo_port) * switch_total_logic_port_num);
    if (!switch_bdinfo_port)
    {
        error = -1;
        goto exit;
    }
    if (ioctl(switchDevFd, IOCTL_getBdInfoPort, 
                (unsigned long)switch_bdinfo_port) < 0)
    {
        free(switch_bdinfo_port);
        error = -1;
        goto exit;
    }

exit:
    close(switchDevFd);
    return error;
    */
      return 0;
}
#if 0
int get_bdtype(void)
{
    return bdtype;
}
#endif
/********************************************************
 *端口初始化函数，要优先sfp_app_global_init处理
 *
 ********************************************************/
void swtichport_api_init()
{
  /*
    int fd = -1;

    switchDevFd = open("/dev/switch", O_RDWR);
    if (switchDevFd < 0)
    {
        cdebug("Opening switch device failed \n" );
        return(-1);
    }
    fd = ioctl(switchDevFd, IOCTL_getBoardType, &bdtype);


    if (fd < 0)
    {
        cdebug("get board info error. fd=%d \n",fd);
        return(-1);
    }	

    if(switchadmin_app_global_init() < 0){

    }


    gpioDevFd = open("/dev/ports", O_RDONLY);
    if (gpioDevFd < 0)
    {
        cdebug("Opening gpio device error\n" );
        close(gpioDevFd);
        return(-1);
    }
*/
}



int get_bdinfo_port_num(struct bdinfo_ *ptr)
{
    if(ptr != NULL)
    {
        ptr->RJ45100M_num = switch_total_RJ45100M_port_num;
        ptr->SFP100M_num = switch_total_SFP100M_port_num;
        ptr->RJ451G_num = switch_total_RJ451G_port_num;
        ptr->SFP1G_num = switch_total_SFP1G_port_num;
        ptr->XFP10G_num = switch_total_XFP10G_port_num;
        ptr->total_num = switch_total_RJ45100M_port_num + switch_total_SFP100M_port_num + switch_total_RJ451G_port_num + \
                         switch_total_SFP1G_port_num + switch_total_XFP10G_port_num;


        cdebug("RJ45 100M port=%d  SFP 100M port=%d RJ45 1G port=%d SFP 1G port=%d XFP 10G port=%d total_port=%d\n", \
                switch_total_RJ45100M_port_num, switch_total_SFP100M_port_num,switch_total_RJ451G_port_num,\
                switch_total_SFP1G_port_num, switch_total_XFP10G_port_num, ptr->total_num);

        return 0;
    }
    else
        return(-1);
}


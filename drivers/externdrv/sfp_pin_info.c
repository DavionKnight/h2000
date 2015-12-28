/*
 *
 * @file		sfpapi.c
 * @author	Tianzhy <tianzy@huahuan.com>
 * @date		2013-8-22
 * @modif   zhangjj<zhangjj@huahuan.com>
 */
#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>
//#include <gpio_ioctls.h>
//#include <bdinfo.h>
//#include <bd_export_info.h>

#include "api.h"

#ifndef SOL_NETLINK
#define SOL_NETLINK 270
#endif

#define SFP_UPDATE_INFO_TIME_OUT	30 	/* seconds */

#define SFP_CHECK_PRESENCE	0


  
/* Global */
#define PER_SFP_PORT_PIN_NUM 8

#define SFP_PING_CHANGE_DEFAULT		1
#define SFP_MO_INSERT		0
#define SFP_MO_INSERT_NOT	1
#define SFP_MO_INSERT_DEFAULT		SFP_MO_INSERT_NOT

#define SFP_TX_DISABLE_NOT	0
#define SFP_TX_DISABLE  1
#define SFP_TX_DISABLE_DEFAULT		SFP_TX_DISABLE_NOT

#define SFP_TX_FAULT_NOT	0
#define SFP_TX_FAULT		1
#define SFP_TX_FAULT_DEFAULT		SFP_TX_FAULT_NOT

#define SFP_RX_LOS_NOT		0
#define SFP_RX_LOS			1
#define SFP_RX_LOS_DEFAULT			SFP_RX_LOS_NOT


#define SFP_PORT_MIN 1
#define SFP_PORT_MAX 20	//sfp + sfp+ num
#define SFP_PORT_NUM 16	//sfp num

 
pthread_mutex_t sfp_pin_table_mutex = PTHREAD_MUTEX_INITIALIZER;

struct sfpxfpenvinfo_ *sfpinfo_table;
int portFlag;

int ms_devfd = -1;
extern int sfp_boardType;
//extern int SFPXFP_PORT_NUM;
//
static struct sfpinfo_ce sfpinfo_ce_table[8];


#if 0
/* init sfp pin table */
int sfp_init_pin_table(int sfp_num)
{
//  int i = 0;
//  sfpinfo_ce_table = (sfpinfo_table*)malloc(sizeof (sfpinfo_table) * PER_SFP_PORT_PIN_NUM );
//  if(sfpinfo_table == NULL)
//  {
//    cdebug("Error to allocate memory in func sfp_init_pin_table"); 
//    return (-1);
//  }
//  for(i=0; i<PER_SFP_PORT_PIN_NUM; i++)
//  {
//    sfpinfo_table[i].mo_state = SFP_MO_INSERT_DEFAULT;
//    sfpinfo_table[i].mo_changed = SFP_MO_INSERT_DEFAULT;
//    sfpinfo_table[i].rxlos_state = SFP_RX_LOS_DEFAULT;
//    sfpinfo_table[i].rxlos_changed = SFP_RX_LOS_DEFAULT;
//    sfpinfo_table[i].txdis_state = SFP_TX_DISABLE_DEFAULT;
//    sfpinfo_table[i].txdis_changed = SFP_TX_DISABLE_DEFAULT;
//    sfpinfo_table[i].txfault_state = SFP_TX_FAULT_DEFAULT;
//    sfpinfo_table[i].txfault_changed = SFP_TX_DISABLE_DEFAULT;
//  }
//  return 1;
//
    return 0;
//	int id;
//	struct sfpxfpenvinfo_ envInfo;
//
//    p_sfp_pin_table = (sfp_pin_info *)malloc(sizeof (sfp_pin_info) * PER_SFP_PORT_PIN_NUM * sfp_num);
//    if (NULL == p_sfp_pin_table) {
//        cdebug(" SFP malloc pin info talbe failed.\n");
//        return -1;
//    }
//    sfpinfo_table = (struct sfpxfpenvinfo_ *)malloc(sizeof (envInfo) *  sfp_num);
//    memset(sfpinfo_table, 0, sizeof (envInfo) *  sfp_num);
//
//
//
//    portFlag = 1;
//
//    //pthread_mutex_lock(&sfp_pin_table_mutex);
//
//    for ( id = 1; id <= sfp_num; ++id) {
//        int base = (id - 1) * PER_SFP_PORT_PIN_NUM;
//
//        p_sfp_pin_table[base + 0].sfp_id = id;
//        p_sfp_pin_table[base + 0].pin_type = e_gpio_user_pin_type_sfp_txdis;
//
//        p_sfp_pin_table[base + 1].sfp_id = id;
//        p_sfp_pin_table[base + 1].pin_type = e_gpio_user_pin_type_sfp_mo;
//
//        p_sfp_pin_table[base + 2].sfp_id = id;
//        p_sfp_pin_table[base + 2].pin_type = e_gpio_user_pin_type_sfp_txfault;
//
//        p_sfp_pin_table[base + 3].sfp_id = id;
//        p_sfp_pin_table[base + 3].pin_type = e_gpio_user_pin_type_sfp_rxlos;
//    }
//
//    if(CONFIG_MTS1080_2402 == sfp_boardType || CONFIG_MTS1080_2404 == sfp_boardType
//        || CONFIG_MTS1080P_2404 == sfp_boardType || CONFIG_MTS1080P_2402 == sfp_boardType
//        || CONFIG_MTS1080P_1202 == sfp_boardType || CONFIG_MTS1090_2404 == sfp_boardType
//        || CONFIG_MTS1090_2402 == sfp_boardType || CONFIG_MTS1090_1202 == sfp_boardType
//        || CONFIG_MTS1090C_2404 == sfp_boardType || CONFIG_MTS1090C_2402 == sfp_boardType
//        || CONFIG_MTS1090C_1202 == sfp_boardType)
//	{
//		for ( id = 1; id <= SFPXFP_PORT_NUM; ++id) 
//		{
//        		set_pin_default(id);
//		}
//	}
//	else
//	{
//		for ( id = 0; id < SFPXFP_PORT_NUM; ++id)
//		{
//        	set_pin_default(id);
//		}
//    }
//	
//   //pthread_mutex_unlock(&sfp_pin_table_mutex);
//	

}
#endif
extern int SFPXFP_PORT_NUM;
extern int XFP_PORT_NUM;


#define I2C_PORTEXT_BUF_DISABLE		0
#define I2C_PORTEXT_BUF_ENABLE		1
#define I2C_PORTEXT_BUF_REFRESH		2





/*******************************************
  * 获取光模块在位、los、txdis等信息
  * 
  * return value: 
  * 				-1:   error
  * 				0 :    sucess
  ********************************************/
#define FPGA_SFPP_OK   		0x0020
#define FPGA_SFP_OK			0x0022
#define FPGA_SFPP_LOS  		0x0024
#define FPGA_SFP_LOS  		0x0026
#define FPGA_SFPP_TX_FLT   	0x0028
#define FPGA_SFP_TX_FLT   	0x002a
#define FPGA_SFPP_TX_DIS   	0x002c
#define FPGA_SFP_TX_DIS   	0x002e


int get_sfpinfo_by_port(int port, struct sfpinfo_ce *ptr)
{
  int fd,ret;
  int index = port - 1;
  int addr = 0;
//  static int txdis_state = 0; //0: tx enable, 1: tx disable
//  static int mo_state = 0;    //0: plugged, 1: unplugged 
//  static int txfault_state = 0;//
//  static int rxlos_state = 0; //0: no los, 1: los
  //No change :0  ,   Chang: 1
  unsigned char active[2];

  cdebug("\nget_sfpinfo_by_port() start...\n");

  if(ptr == NULL)
  {
    cdebug("Parameter struct sfpinfo pointer is NULL in function get_sfpinfo_by_port()"); 
    return (-1);
  }
  if(port > SFP_PORT_MAX || port < SFP_PORT_MIN)  // Fiber port range from 5~8
  {
    cdebug("readSfpXfpInfo input exceeds the ranger \n");
    return (-1);
  }
  
  fd=open(FPGADRVDIR,O_RDWR);
  if(fd<0)
  {
    cdebug("open /dev/spidev0.0 error1\n");
    return(-1);
  }
  if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
  {
  	cdebug("ioctl SPI_IOC_OPER_FPGA error\n");
  }

  addr = port > SFP_PORT_NUM?FPGA_SFPP_OK:FPGA_SFP_OK;
  //get the mo state and save in sfpinfo_ce_table
  lseek(fd, addr, SEEK_SET);
  read(fd, active,2);
  ptr->mo_state = active[1]>>index & 0x01;
  ptr->mo_changed = ptr->mo_state ^ sfpinfo_ce_table[index].mo_state;
  sfpinfo_ce_table[index].mo_state= ptr->mo_state;
  cdebug("mo_state is %x\n",sfpinfo_ce_table[index].mo_state);

  addr = port > SFP_PORT_NUM?FPGA_SFPP_LOS:FPGA_SFP_LOS;
  lseek(fd, addr, SEEK_SET);
  read(fd, active,2); 
  ptr->rxlos_state  = active[1]>>index & 0x01;
  ptr->rxlos_changed  = ptr->rxlos_state  ^ sfpinfo_ce_table[index].rxlos_state ;
  sfpinfo_ce_table[index].rxlos_state = ptr->rxlos_state;
  cdebug("rxlos_state is %x\n",sfpinfo_ce_table[index].rxlos_state);

  addr = port > SFP_PORT_NUM?FPGA_SFPP_TX_FLT:FPGA_SFP_TX_FLT;
  lseek(fd, addr, SEEK_SET);
  read(fd, active,2);
  ptr->txfault_state = active[1]>>index & 0x01;
  ptr->txfault_changed = ptr->txfault_state ^ sfpinfo_ce_table[index].txfault_state ;
  sfpinfo_ce_table[index].txfault_state = ptr->txfault_state; 
  cdebug("txfault_state is %x\n",sfpinfo_ce_table[index].txfault_state);

  addr = port > SFP_PORT_NUM?FPGA_SFPP_TX_DIS:FPGA_SFP_TX_DIS;
  lseek(fd, addr, SEEK_SET);
  read(fd, active,2);
  ptr->txdis_state  = active[1]>>index & 0x01;
  ptr->txdis_changed  = ptr->txdis_state  ^ sfpinfo_ce_table[index].txdis_state ;
  sfpinfo_ce_table[index].txdis_state = ptr->txdis_state;
  cdebug("txdis_state is %x\n",sfpinfo_ce_table[index].txdis_state);

  if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
  {
  	cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
  }
  close(fd);

  return 0;
}

/************************
*1-disable,0-enable
*
*************************/
int set_sfptxdisable_by_port(int port, int enable)
{
  int fd,ret;
  int index = port - 1;
  unsigned int addr = 0;
  unsigned char data[2] = {0};
  unsigned int offset = 0, pos = 0;

  cdebug("set_sfptxdisable_by_port() start...\n");

  //if(ptr == NULL)
  //{
    //cdebug("Parameter struct sfpinfo pointer is NULL in function get_sfpinfo_by_port()"); 
    //return (-1);
  //}
  if((port > SFP_PORT_MAX || port < SFP_PORT_MIN) || (enable <0  || enable > 1))
  {
    cdebug("readSfpXfpInfo input exceeds the ranger \n");
    return (-1);
  }
  
  fd=open(FPGADRVDIR,O_RDWR);
  if(fd<0)
  {
    cdebug("open /dev/spidev0.0 error1\n");
    return(-1);
  }
  if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
  {
  	cdebug("ioctl SPI_IOC_OPER_FPGA error\n");
  }
  if(port > SFP_PORT_NUM)
  {
  	addr = FPGA_SFPP_TX_DIS;
	offset = index - SFP_PORT_NUM;
	pos = 0;
  }
  else
  {
  	addr = FPGA_SFP_TX_DIS;
	offset = index;
	if(port >4)
		pos = 1;
	else
		pos = 0;
  }

  lseek(fd, addr, SEEK_SET);
  read(fd, data,2);

  if(enable == 1)
  {
  	data[pos] |= 1 << offset;
  }
  else
  {
  	data[pos] &= ~(1 << offset);	
  }
  cdebug("addr is %x\n",addr);
  cdebug("data[0] is %x\n",data[0]);
  cdebug("data[1] is %x\n",data[1]);

  lseek(fd, addr, SEEK_SET);
  write(fd, data,2);
  
  if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
  {
  	cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
  }
  close(fd);

  return 0;
}
/*
start to add als function by cq 20141027
*/
int get_sfp_loss_info_by_all_port(unsigned int * los_state)
{
	int fd,ret;
	unsigned char active[2];

	cdebug("\nget_sfpinfo_by_port() start...\n");

	fd=open("/dev/fpga",O_RDWR);
	if(fd<0)
	{
		cdebug("open /dev/fpga error1\n");
		return(-1);
	}
	if(ioctl(fd, SPI_IOC_OPER_FPGA, NULL))
	{
	  cdebug("ioctl SPI_IOC_OPER_FPGA error\n");
	}

	//get the mo state and save in sfpinfo_ce_table
	lseek(fd, FPGA_SFPP_LOS, SEEK_SET);
	read(fd, active, 2); 
	*los_state	|= (active[0]& 0xFF)<<16;
	lseek(fd, FPGA_SFP_LOS, SEEK_SET);
	read(fd, active, 2); 
	*los_state	|= (active[1]& 0xFF)<<8;
	*los_state	|= active[0]& 0xFF;

    if(ioctl(fd, SPI_IOC_OPER_FPGA_DONE, NULL))
    {
    	cdebug("ioctl SPI_IOC_OPER_FPGA_DONE error\n");
    }	
	close(fd);
	return 0;
}


/*
end to add als function by cq 20141027
*/



/*
   struct sfpinfo_ce{
   int txdis_state;
   int txdis_changed;
   int mo_state;
   int mo_changed;
   int txfault_state;
   int txfault_changed;
   int rxlos_state;
   int rxlos_changed;
   }

   */
#if 0
int main()
{
  struct sfpinfo_ce info={0,0,0,0,0,0,0,0};  
    int i;
    for(i=1;i<5; i++ )
    {
      get_sfpinfo_by_port(i, &info);
      printf("SFPINFO is:  %d,\t %d,\t %d,\t %d,\t %d,\t %d,\t, %d,\t %d,\t\n",info.txdis_state, info.txdis_changed, info.mo_state, info.mo_changed, info.txfault_state, info.txfault_changed, info.rxlos_state, info.rxlos_changed);

    }
    return 1;
}
#endif


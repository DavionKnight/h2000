/*
 *
 * @file		api.h
 * @author	Tianzhy <tianzy@huahuan.com>
 * @date		2013-8-15
 * @modif   zhangjj<zhangjj@huahuan.com>
 */
#ifndef __CE_API_H__
#define __CE_API_H__


//#define DEBUG
#ifdef DEBUG
#define cdebug(format, arg...)		\
    printf(format , ## arg);        \
    printf("\n")
#else
#define cdebug(format, arg...)
#endif

//add by zhangjj for 1604
#define FPGADRVDIR	"/dev/spidev0.0"


#define SPI_IOC_MAGIC			'k'

#define SPI_IOC_OPER_FPGA		    _IOW(SPI_IOC_MAGIC, 5, __u8)
#define SPI_IOC_OPER_FPGA_DONE		_IOW(SPI_IOC_MAGIC, 6, __u8)
#define SPI_IOC_OPER_DPLL		    _IOW(SPI_IOC_MAGIC, 7, __u8)
#define SPI_IOC_OPER_DPLL_DONE		_IOW(SPI_IOC_MAGIC, 8, __u8)
//add end

#define DELAY_COUNT 100
// offset of the fpga 
#define FPGA_WREN_OFFSET  		0x00;   // write enable , flap  is valid
#define FPGA_RDEN_OFFSET  		0x02;   // read enable,  flap is valid
#define FPGA_ADDR_OFFSET  		0x04;   // set the slave address and reg address
#define FPGA_WRDATA_OFFSET  	0x06;   // write data
#define FPGA_RDDATA_OFFSET  	0x08;   // read data
#define FPGA_EN_OFFSET     	    0x0a;   // read/write/write data flag


/*struct used for the infomation*/
struct bdinfo_{
    int RJ45100M_num;
    int SFP100M_num;
    int RJ451G_num;
    int SFP1G_num;
    int XFP10G_num;
    int total_num;
};

struct bdinfo_ bdinfo_port_num;

struct sfpinfo_{
    int txdis_state;
    int txdis_changed;
    int mo_state;
    int mo_changed;
    int txfault_state;
    int txfault_changed;
    int rxlos_state;
    int rxlos_changed;
};

struct sfpinfo_ce{
  int txdis_state;
  int txdis_changed;
  int mo_state;
  int mo_changed;
  int txfault_state;
  int txfault_changed;
  int rxlos_state;
  int rxlos_changed;
};


struct sfpxfpenvinfo_ {
    char vendorName[16];
    char vendorPN[16];
    char vendorRev[16];
    char vendorSerialNo[16];
    char date[16];
    char transceiverType[16];
    char connectorType[16];
    char opticalType[32];
    char lineCoding[64];
    char nominalBitRate[32];
    char linkLength[16];
    char waveLength[16];
    char maxBitRateMargin[16];
    char minBitRateMargin[16];

    char temperature[16];
    char vccVoltage[16];
    char TXBiasCurrent[16];
    char TXOutputPower[16];
    char RXInputPower[16];	

};
/* struct to read and write to fpga*/
struct fpga_msg 
{
    unsigned short addr;       //register address to read or write

    unsigned char  flags;      //read or write flag; 0: read, 1:write

    unsigned short len;        //length to read or write

    unsigned char  buf[2];       //buffer to store the data
};

struct fpga_rdwr_data
{
    struct fpga_msg *msgs;
    int nmsgs;      //step to implement an action, it needs 5 steps to read sfp informaion, etc.
};


//typedef struct _sdm_ce_fan_running_info
struct sdm_ce_fan_running_info
{
	int           FanID;
  int           HwState ;
//	int           *SubUnitHwState;
	int           SubUnitHwState[8];
//	int           *RunSpeed;
	int           RunSpeed[8];
	int           Voltage; 
	int           Intensity; 
} ;

//typedef struct _sdm_ce_power_running_info
struct sdm_ce_power_running_info 
{
	int             PowerID; 
	int             HwState;
	int 		VoltageIn ;
	int    	IntensityIn ;
	int 		VoltageOut ;
	int    	IntensityOut ;
} ;


/************************************************
 *获取板子温度(TMP101)
 *参数:
 *返回值:	-1 异常返回, value:温度值
 ************************************************/
int  getTemperature(short *temp);      // 目前温度为两个值
//int getTemperature(float *temprature);


/************************************************
 *获取rtc日历，如2013 9 4 17 21 10
 *参数:		可通过int time[6];来赋值
 *返回值:	0 正常返回， -1 异常返回
 ************************************************/
int rtc_get_time(int *piYear, int *piMonth,  int *piDay, 
        int *piHour, int *piMinute, int *piSecond);

/************************************************
 *设置rtc日历，如2013 9 4 17 21 10
 *例:rtc_set_time(2013,9,4,17,21 10)
 *返回值:	0 正常返回， -1 异常返回
 ************************************************/
int rtc_set_time(int  iYear, int  iMonth,  int  iDay,
        int  iHour, int  iMinute, int  iSecond) ;

/************************************************
 *通过rct时钟设置系统时钟
 *参数:
 *返回值:	0 正常返回， -1 异常返回
 ************************************************/
int rtc_recover_systime();


/*******************************************
 * 读取EEPROM内容到buffer---初始化时调用一次
 * 参数: 	
 * 返回值:	0 正常返回， -1 异常返回
 ********************************************/
int boardinfo_init(void);

/* 获取系统类型 目前已经定义的值为*/
#define CONFIG_H20PN_1610       00  /* only 4GE + 4FX */
#define CONFIG_H20PN_1611	01  /* 4GE+4FX+8E1 */
#define CONFIG_H20PN_1612	02  /* */
#define CONFIG_H20PN_1613	03  /* 4GE+4GE+2STM1 */
#define CONFIG_H20PN_1841_4E1	04  /* 2GE + 2GX + 4E1 */
#define CONFIG_H20PN_1822_2GX	05  /* 4GE + 2GX */

#define CONFIG_H18CE_2404B	16

#define CONFIG_H18CE_2404C    	19

#define CONFIG_H20PN_1660	22 /* 4x10GE + 24GE */
#define CONFIG_H20PN_1664	23 /* only 16GE */
#define CONFIG_H20PN_1663	24 /* only 24GE no 10GE */

#if 0
/*******************************************
 * 从buffer解析DeviceName---初始化时调用一次
 * 参数:    
 * 返回值:  0 正常返回， -1 异常返回
 ********************************************/
int getDeviceFromEeprom(void);

/*******************************************
 * 从buffer解析VendorName---初始化时调用一次
 * 参数:    
 * 返回值:  0 正常返回， -1 异常返回
 ********************************************/
int getVendorFromEeprom(void);
#endif

/***************************************************
 * 从EEPROM获取板类型	
 * 参数:    
 * 返回值:  boardTypeValue --具体类型参考上面宏定义
 **************************************************/
int get_bdtype(void);

/*******************************************
 * 从EEPROM获取verdor name
 * 参数:    ptr:vendor name
 * 返回值:  返回值:	0 正常返回， -1 异常返回
 ********************************************/
int get_vendor_name(unsigned char *ptr);

/*******************************************
 * 从EEPROM获取板类型
 * 参数:    ptr: device name
 * 返回值:  返回值:	0 正常返回， -1 异常返回
 ********************************************/
int get_device_name(unsigned char *ptr);

/**********************************************************
 * EEPROM写函数
 * 参数:    addr:目的地址，buf:需写入的buf，len:buf length
 * 返回值:  返回值:	0 正常返回， -1 异常返回
 **********************************************************/
int EepromWrite(unsigned short addr,unsigned char *buf, unsigned short len);

/**********************************************************
 * EEPROM读函数
 * 参数:	addr:EEPROM地址，buf:需读入的buf，len:buf length
 * 返回值:	返回值: 0 正常返回， -1 异常返回
 **********************************************************/
int EepromRead(unsigned short addr,unsigned char *buf, unsigned short len);


/*******************************************
 * 获取光模块在位、los、txdis等信息
 * 参数: 	port: 1-20  ptr:光模块信息
 * 返回值:	0 正常返回， -1 异常返回
 ********************************************/
int get_sfpinfo_by_port(int port, struct sfpinfo_ce *ptr);

/*********************************************
 *从缓冲器中获取光模块信息
 *函数实现与get_sfpEnvInfo_by_port_from_sxfp相同
 *********************************************/
int get_sfpEnvInfo_by_port(int port, struct sfpxfpenvinfo_ *ptr);

/*********************************************
 *从sfp/xfp模块中直接获取光模块信息
 *参数: 	port: 1-20  sfpPtr: 光模块信息
 *返回值: 	0 正常返回， -1 异常返回
 *********************************************/
int get_sfpEnvInfo_by_port_from_sxfp(int port, struct sfpxfpenvinfo_ *sfpPtr);

//int get_bdinfo_port_num(struct bdinfo_ *ptr);

/***************************************************
 *b"000"	直流电源盘（-48V/12V）75W，无电源监控
 *b"001"	直流电源盘（-48V/12V）75W，带电源监控
 *b"010"	reserve
 *b"011"	reserve
 *b"100"	交流电源盘（~220V/12V）75W，无电源监控
 *b"101"	交流电源盘（~220V/12V）75W，带电源监控
 *b"110"	reserve
 *b"111"	不在位
 ***************************************************/
/****************************************************
 * 获取电源状态
 * 参数:	pwrState: 电源状态 参考上表
 * 返回值:  0 正常返回， -1 异常返回
 ****************************************************/
int  getPowerState(unsigned short *pwrState);

/***************************************************************
 * 获取电源信息(0: ok ;  1:fail)----该接口暂时不可用，FPGA到pwr的IIC口没有接
 * Power B state			Power A state
 * 返回值:  0 正常返回， -1 异常返回
 ***************************************************************/
int  getPowerInfo(int index, struct sdm_ce_power_running_info *pwrInfo);

/*******************************************
 * 获取风扇在位状态
 * 参数: fan: 0-在位	1-不在位
 * 返回值:  0 正常返回， -1 异常返回
 *******************************************/
int  getFanState(unsigned short *fan);

/*******************************************
 * 获取风扇状态
 * 参数: 	index: for 1604c this value fixed 1
 			fan: 风扇信息
 * 返回值:  0 正常返回， -1 异常返回
 *******************************************/
int  getFanInfo(int index, struct sdm_ce_fan_running_info *fan);

/***************************************
 * 风扇开关设置
 * 参数:	1-ON  0-OFF
 * 返回值:  0 正常返回， -1 异常返回
 ***************************************/
int  enableFan(const short on_off);

/***************************************************
 * 设置告警指示灯
 * 参数:    led_type: unused 
 *			led_color: 1-OFF	2-ON
 * 返回值:  0 正常返回， -1 异常返回
 ***************************************************/
int setAlarmLed(int led_type, int led_color);


/****************************************
 * 运行指示灯
 * 参数:	led_color: 1: OFF	2: ON
 * 返回值：	0 正常  -1 错误
 ****************************************/
int setNorLed( int led_color);

#if 0
/********************************************************
 *端口初始化函数，要优先sfp_app_global_init处理
 *
 ********************************************************/
void swtichport_api_init();


/********************************************************
 *sfp初始化函数要在swtichport_api_init之后初始化
 *
 ********************************************************/
int sfp_app_global_init(void);


/********************************************************
 * 获取正在运行的image号，1 or 2
 ********************************************************/
//int readRunningImage(void);
#endif

/***************************************
        type 					value
#define TYPE_IMAGE   	1	 	1 or 2
#define TYPE_KERNEL1 	2		0 or 1 1表示有效，0表示无效
#define TYPE_RAMFS1 	3		0 or 1
#define TYPE_DTB1		4		0 or 1
#define TYPE_APP1		5		0 or 1
#define TYPE_KERNEL2 	6		0 or 1
#define TYPE_RAMFS2 	7		0 or 1
#define TYPE_DTB2		8		0 or 1
#define TYPE_APP2		9		0 or 1
 ***************************************/
 
/**********************************************
*修改EEPROM 各模块有效标记 
*参数：type:参考上表 value:参考上表
*返回值：0 正常  -1 错误
***********************************************/
int writeImageFlag(int type, int value);

/**********************************************
*读取EEPROM 各模块有效标记 
*参数：type:参考上表 value:参考上表
*返回值：0 正常  -1 错误
***********************************************/
int readImageFlag(int type, int *value);

/*printf function, used for debug */
void pdata(unsigned char *pdata, int count);

/**********************************************
*FPGA数据读取
*参数：fd:设备描述符  msg:地址，buf等信息  step:读写次数
*返回值：1 正常  -1 错误
***********************************************/
int read_fpga_data(int fd, struct fpga_msg *msg, int step);

/**********************************************
*FPGA数据写入
*参数：fd:设备描述符  msg:地址，buf等信息  step:读写次数
*返回值：1 正常  -1 错误
***********************************************/
int write_fpga_data(int fd, struct fpga_msg *msg, int step);

#endif  //endif __API_H__

/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:fzs
*  File Name        	:/home/kevin/works/projects/H20PN-2000/drivers/spidrv/spidrv.h
*  Create Date        	:2016/09/26 04:26
*  Last Modified      	:
*  Description    	:
*/

/*
用户态spi初始化，必须首先调用
参数：
返回值：-1 初始化失败
	0  初始化成功
*/
int spidrv_init();

/*
用户态spi退出，进程退出时调用
参数：	
返回值：-1 退出失败
	0  退出成功
*/
int spidrv_exit();

/*
本盘fpga读，单盘fpga读
参数：	count 要读取的字节数
	slot	要读取的盘的槽位号，本cpu盘用0x10,0x11,其他单盘按物理槽位
返回值：-1 读取失败
	0  读取成功
*/
int fpga_spi_read(unsigned int addr, unsigned char *data, size_t count, unsigned char slot);

/*
本盘fpga写，单盘fpga写
参数：	count 要写入的字节数
	slot	要写入的盘的槽位号，本cpu盘用0x10,0x11,其他单盘按物理槽位
返回值：-1 写入失败
	0  写入成功
*/
int fpga_spi_write(unsigned int addr, unsigned char *data, size_t count, unsigned char slot);

/*
本盘dpll读
参数：	count 要读取的字节数
返回值：-1 读取失败
	0  读取成功
*/
int dpll_spi_read(unsigned short addr, unsigned char *data, size_t count);

/*
本盘dpll写
参数：	count 要写入的字节数
	slot	要写入的盘的槽位号，本cpu盘用0x10,0x11,其他单盘按物理槽位
返回值：-1 写入失败
	0  写入成功
*/
int dpll_spi_write(unsigned short addr, unsigned char *data, size_t count);

/*
读单盘fpga比较复杂，分为实时读和循环读
一般读取寄存器次数较少时使用实时读，实时读的接口同本盘fpga读
fpga_spi_read。
需要不断重复读取的采用循环读
循环读需要4个步骤 获取可用条目->配置条目->使能条目->读
目前循环读有128条
先调用fpga_rm_cir_read_get获取可用的条目
调用fpga_rm_cir_read_set设置要读取的槽位、地址和长度
调用fpga_rm_cir_en使能该条目
调用fpga_rm_cir_read读取从远端返回的数据
之后的读取只需调用fpga_rm_cir_en使能条目然后调用fpga_rm_cir_read
循环读可通过fpga_rm_cir_en_blk使能多个条目，同时将多个单盘数据读到本盘fpga

*/
/*
获取可用的循环读条目
参数：	
返回值：-1 获取失败
	0  获取成功
*/
int fpga_read_remote_get(int *clause);

/*
配置要读取的单盘的槽位地址和长度
参数：	slot 0x0 ~ 0x3，0xb单盘槽位号，按物理槽位
	size：要读取的字节数，单盘fpga寄存器是16位宽的，即每个寄存器是2个字节
返回值：-1 设置失败
	0  设置成功
*/
int fpga_read_remote_set(int clause, unsigned char slot, unsigned int addr, unsigned int size);


/*
获取已配置的循环读条目的槽位地址和长度
参数：
返回值：-1 获取失败
	0  获取成功
*/
int fpga_read_remote_inf(int clause, unsigned char *slot, unsigned int *addr, unsigned int *size);

/*
使能一条循环读条目
参数：
返回值：-1 获取失败
	0  获取成功
*/
int fpga_read_remote_en(int clause);

/*
使能多个循环读条目，使能的过程是将单盘上的数据读到fpga缓冲区的过程
参数：	enbuf是short形指针，每一个short有16位对应16个条目
	size指有多少个short,可以使能的总条目数为16*size
返回值：-1 获取失败
	0  获取成功
*/
int fpga_read_remote_block_en(unsigned short *enbuf, unsigned int size);

/*
从fpga缓冲区将数据读出
参数：	需要填入与前面设置相应的slot，addr，size
返回值：-1 获取失败
	0  获取成功
*/
int fpga_read_remote(int clause, unsigned char slot, unsigned int addr, unsigned short *pbuf, unsigned int size);



int epcs_erase_chip();
int epcs_erase_sector(unsigned int offset);
int epcs_spi_read(unsigned int addr, unsigned char *data, size_t count);
int epcs_spi_write(unsigned short addr, unsigned char *data, size_t count);








/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:fzs
*  File Name        	:/home/kevin/works/projects/H20RN-2000/drivers/gpio/watchdog.c
*  Create Date        	:2016/10/26 19:54
*  Last Modified      	:2016/10/26 19:54
*  Description    	:
*/

#include<stdio.h>
#include<stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include "gpiodrv.h"

#define GPIO_BASE		0x18000000
#define MAP_SIZE 		4096UL
#define MAP_MASK 		(MAP_SIZE - 1)

void *mapbase = NULL;
int fd_mmap;

/*设置为短周期，并将喂狗的gpio设置为输入，使喂狗失效*/
int device_rst(void)
{
	gpiodrv_init();
	
	gpio_direction_output(3, 0);
	gpio_direction_input(1);

	gpiodrv_exit();

	return 0;
}

int wdt_lp_set(void)
{
	gpiodrv_init();
	
	gpio_direction_output(3, 1);

	gpiodrv_exit();

	return 0;
}

int wdt_sp_set(void)
{
	gpiodrv_init();
	
	gpio_direction_output(3, 0);

	gpiodrv_exit();

	return 0;
}

/*2000上没有使能管脚，姑且把喂狗的gpio设置为输出*/
int wdt_en(void)
{
	gpiodrv_init();
	
	gpio_direction_output(1, 0);
	gpio_direction_output(3, 1);

	gpiodrv_exit();
	return 0;
}

int wdt_wdi(void)
{
	unsigned char value;

	gpiodrv_init();
	
	value = gpio_get_value(1);
	value = value? 0:1;	

	gpio_output(1, value);

	gpiodrv_exit();

	return 0;
}







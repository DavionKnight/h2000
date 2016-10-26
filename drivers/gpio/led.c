/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:Kevin_fzs
*  File Name        	:/home/kevin/works/projects/H20RN-2000/drivers/gpio/led.c
*  Create Date        	:2016/10/26 20:08
*  Last Modified      	:2016/10/26 20:08
*  Description    	:
*/

#include "gpiodrv.h"
#include <stdio.h>

int main()
{
	int i = 20;

	gpiodrv_init();
printf("init\n");
	do{
		gpio_direction_output(9, i%2);
		sleep(1);
	}while(i-->0);

	gpiodrv_exit();

	return 0;
}


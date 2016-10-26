/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:Kevin_fzs
*  File Name        	:/home/kevin/works/projects/H20RN-2000/drivers/gpio/gpiodrv.h
*  Create Date        	:2016/10/26 19:55
*  Last Modified      	:2016/10/26 19:55
*  Description    	:
*/


int gpiodrv_init();
void gpiodrv_exit();

int gpio_get_value(unsigned gpio);
int gpio_direction_input(unsigned gpio);
int gpio_direction_output(unsigned gpio, int value);


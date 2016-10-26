/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:fzs
*  File Name        	:/home/kevin/works/projects/H20RN-2000/drivers/gpio/wdtdog.c
*  Create Date        	:2016/10/27 01:48
*  Last Modified      	:2016/10/27 01:48
*  Description    	:
*/


#include "gpiodrv.h"
#include<stdio.h>

int main()
{
	wdt_en();

	device_rst();
	while(1)
	{
		wdt_wdi();
		printf("wdt wdi\n");
		sleep(1);
	}	

	return 0;	
}


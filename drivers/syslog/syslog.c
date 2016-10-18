/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:Kevin_fzs
*  File Name        	:/home/kevin/works/projects/H20PN-2000/drivers/test\log2file.c
*  Create Date        	:2016/07/27 15:35
*  Last Modified      	:2016/07/27 15:35
*  Description    	:
*/

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
	int ret = 0;
	int fd = 0;

	openlog("syslogtest", LOG_CONS|LOG_PID, 0);	

	syslog(LOG_DEBUG, "aaaaaaaaaaaaaa", argv[0]);

	closelog();
	return 0;	
}




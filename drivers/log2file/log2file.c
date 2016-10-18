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

#define LOG_FILE  	"/home/log/logfile"
#define LOGDEV		"/dev/log2file"
#define FILE_NUM  5
#define LOG2FILE_ENABLE		0x1
#define LOG2FILE_DISABLE	0x2 
#define LOG2FILE_SIZE		(2*1024*1024) 

int backup_logfile()
{
        int i = 0;
	int ret = 0;
	char filen[20] = {0};
	char fileold[20] = {0};
	int  filp = NULL;

	/*if logfile is not exist,do not do anything*/
	if(access(LOG_FILE, 0))
	{
		return 0;
	}
	/*find first file that is not exist*/
	for(i = 1; i <= FILE_NUM; i++)
	{
		sprintf(filen, "%s%d", LOG_FILE, i);		
		if(access(filen, 0))
		{
			break;
		}
	}	
	if((FILE_NUM+1) == i)	/*if all exist*/	
	{
		for(i = 1; i <= FILE_NUM+1; i++)
		{
			memcpy(fileold, filen, sizeof(fileold));
			sprintf(filen, "%s%d", LOG_FILE, i);		
			if(1 == i)	
				unlink(filen);	/*delete the first one*/	
			else if(FILE_NUM+1 == i)
			{
				if(rename(LOG_FILE, fileold))
					return -1;
			}
			else
				if(rename(filen, fileold))
					return -1;
		}
		
	}
	else 
		if(rename(LOG_FILE, filen))
			return -1;
	return 0;
	
}
int mk_dir()
{
	DIR *logdir = NULL;  
	char dir[20] = "/home/log";
	if((logdir= opendir(dir))==NULL)
	{  
		int ret = mkdir(dir, 0);
		if (ret != 0)  
		{  
			return -1;  
		}  
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int fd = 0;
	struct stat statbuf;
	
	if(-1 == mk_dir())
	{
		printf("create log directory error\n");
		return 0;
	}	

	if(-1 == backup_logfile())
	{
		printf("backup logfile error\n");
		return 0;
	}	
	fd = open(LOGDEV, O_RDWR);
	if(-1 == fd)	
		return -1;

	/*enable log2file*/
	ioctl(fd, LOG2FILE_ENABLE, sizeof(int));

	while(1)
	{
		sleep(5);

		if(!access(LOG_FILE, 0))
		{
			if(stat(LOG_FILE, &statbuf)<0)
			{
				printf("get file size err\n");
				return 0;
			}	
			if(LOG2FILE_SIZE < statbuf.st_size)
			{
				/*disable log2file*/
				ioctl(fd, LOG2FILE_DISABLE, sizeof(int));
				printf("Log file size is over 2M, close log2file\n");
			}
		}
			
	}
	
	return 0;	
}




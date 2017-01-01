/**
 * @file   Reg_and_Remove_me.c
 * @author Ingo Assmus <ias@pandatel.com>
 * @date   Th Apr 01 15:03:47 2004
 * @version <pre> $Id: Reg_and_Remove_me.c,v 1.2 2005/06/08 11:55:05 sst Exp $ </pre>
 * 
 * @brief  A Simple Watchdog Application
 *
 * 
 * This file contains a simple Demo Application 
 */

/*
 * (c) COPYRIGHT 2004 by Pandatel AG Germany
 * All rights reserved.
 *
 * The Copyright to the computer program(s) herein is the property of
 * Pandatel AG Germany.
 * The program(s) may only be used and/or copied with the written permission
 * from Pandatel AG or in accordance with the terms and conditions
 * stipulated in the agreement contract under which the program(s) have been
 * supplied.
 * 
 */


#include <sys/stat.h>	// for open
#include <fcntl.h>		// for open
#include <unistd.h>		// for write
#include <stdlib.h>		// for exit
#include <stdio.h>		// for printf
#include <sys/types.h>	// for open
#include <sys/ioctl.h>  // for ioctl
#include <sys/types.h>	// for getpid()
#include <unistd.h>		// for getpid()
#include <string.h>		// for strcpy()

#include "TriggerWDT.h"						/* List head structure */
#include "ioctl_codes.h"					/* Codes uesed within ioctl_function */
#include "wdt.h"							/* WDT structures etc. */
#define	SEC	*100
#define	MAX_RENEW	1			/* No of times the task will send "still alive messages" before termination */
#define	MAX_RENEW_DELAYED	1	/* No of times the task will send "still alive messages" before termination */

int main(int argc, const char *argv[]) {

	int ioctl_return = 0;
	int fd;
	pid_t 	my_pid;							// in fact it's an int
	wdt_registerded_task_t	my_task;		// setup IOCTL data
	memset(&my_task,0,sizeof(my_task));		// clear my_task variable;
		
	fd=open("/dev/wdt",O_WRONLY);
   	printf("Try to open Watchdog-Device.\n");               
	
	if (fd==-1) 
	{
		perror("watchdog");
		exit(1);
	}

	my_pid=getpid();						// Find out our PID to register at Watchdog_Device
	my_task.pid =  my_pid;					// Fill struct
	my_task.call_period = 20 SEC;			// Melde mich spätestens alle 20 sec
	
// Register Task with current PID
	printf("----->  Use IOCTL_Function to register my PID: %d \n", my_pid);               
	strcpy(my_task.command, "\n**** Gleich bin ich wieder weg !!!! ****\n");
	ioctl_return = ioctl(fd, WDT_REGISTER_ME, &my_task);   
	printf("----->     IOCTL_Function returned: %d \n", ioctl_return);               

// Register 2nd Task with current PID+1
	printf("----->  Use IOCTL_Function to register my PID: %d \n", my_pid+1);
	my_task.pid += 1;               
	strcpy(my_task.command, "\n**** Gleich bin ich wieder weg !!!! ****\n");
	ioctl_return = ioctl(fd, WDT_REGISTER_ME, &my_task);   
	printf("----->     IOCTL_Function returned: %d \n", ioctl_return);               

// Register 3rd Task with current PID+2
	printf("----->  Use IOCTL_Function to register my PID: %d \n", my_pid+2);
	my_task.pid += 1;               
	strcpy(my_task.command, "\n**** Gleich bin ich wieder weg !!!! ****\n");
	ioctl_return = ioctl(fd, WDT_REGISTER_ME, &my_task);   
	printf("----->     IOCTL_Function returned: %d \n", ioctl_return);               

// Unregister 1st Task with current PID
	printf("----->  Use IOCTL_Function to unregister my PID: %d \n", my_pid);               
	ioctl(fd, WDT_REMOVE_ME, &my_pid);   

// Unregister 2nd Task with current PID+1
	my_pid += 1;
	printf("----->  Use IOCTL_Function to unregister my PID: %d \n", my_pid);               
	ioctl(fd, WDT_REMOVE_ME, &my_pid);   
	printf("\n----->     #### Good bye sweat world ..... ####\n");               

// Unregister 3rd Task with current PID+2
	my_pid += 1;
	printf("----->  Use IOCTL_Function to unregister my PID: %d \n", my_pid);               
	ioctl(fd, WDT_REMOVE_ME, &my_pid);   
	
	printf("\n----->     #### Good bye sweat world ..... ####\n");               

   // close port
    close(fd);
return 0;
}

/*************************************************************************
 *
 * History:
 *	$Log: Reg_and_Remove_me.c,v $
 *	Revision 1.2  2005/06/08 11:55:05  sst
 *	- prepared for using with etcb
 *	
 *	Revision 1.1  2004/04/07 14:37:40  ias
 *	initial switch driver
 *	
 *	
 *	
 ************************************************************************/


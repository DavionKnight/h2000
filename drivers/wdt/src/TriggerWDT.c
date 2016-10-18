/**
 * @file   TriggerWDT.c
 * @author Ingo Assmus <ias@pandatel.com>
 * @date   Mo Mar 29 16:23:17 2004
 * @version <pre> $Id: TriggerWDT.c,v 1.2 2005/06/08 11:55:05 sst Exp $ </pre>
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

#include "TriggerWDT.h"					/* List head structure */
#include "ioctl_codes.h"				/* Codes uesed within ioctl_function */
#include "wdt.h"						/* WDT structures etc. */
#define	SEC	*1000

int main(int argc, const char *argv[]) {

	union {
		short 	s;
		char	c[sizeof(short)];
		} un;
	
	int ioctl_return = 0;
	int fd;
	int time_out= 10000;
	pid_t 	my_pid;						// in fact it's an int
	wdt_registerded_task_t	my_task;	// setup IOCTL data

	un.s = 0x0102;
	printf("\n\nWelcome to MPC8260 on LINUX Endian test:  It has ");
	if (sizeof(short) == 2) {
		if (un.c[0] == 1 && un.c[1] == 2)
			printf("big endian \n");
		else 
		{
		 	if (un.c[0] == 2 && un.c[1] == 1)	
				printf("little endian \n");
			else
				printf("unknown \n");
		}
		}
		printf("sizeof(short) = %d\n", sizeof(short));

	memset(&my_task,0,sizeof(my_task));		// clear my_task variable;
		
	fd=open("/dev/wdt",O_WRONLY);
   	printf("Try to open Watchdog-Device.\n");               
	
	if (fd==-1) 
	{
		perror("watchdog");
		exit(1);
	}
// 	while(1) 
	{
	my_pid=getpid();					// Find out our PID to register at Watchdog_Device
	my_task.pid =  my_pid;				// Fill struct
	my_task.call_period = 10 SEC;		// Melde mich spätestens alle 10 sec
	
// Register Task with current PID
	printf("Use IOCTL_Function to register my PID: %d \n", my_pid);               
	strcpy(my_task.command, "Ich bin der erste registrierte Task");
	ioctl_return = ioctl(fd, WDT_REGISTER_ME, &my_task);   
	printf("IOCTL_Function returned: %d \n", ioctl_return);               

// Register Task with faked PID
	printf("Use IOCTL_Function to register my PID: %d \n", my_pid+1);               
	strcpy(my_task.command, "Ich bin der zweite registrierte Task");
	my_task.pid =  my_pid+1;
	ioctl(fd, WDT_REGISTER_ME, &my_task);   
	my_task.pid =  my_pid;

// Write to watchdog device
	printf("Keep watchdog alive...\n");               
   	printf("*** Write new Timeout_value of %d\n", time_out);               
	write(fd, (char *)&time_out, 1);   

// Call for watchdog status
	printf("*** Use IOCTL Get_STATUS\n");               
	ioctl(fd, WDT_GET_STATUS, NULL);   
	printf("*** Sleep 60\n");               
	sleep(60);

// enable Watchdog
	printf("*** Use IOCTL WDT_ENABLE to switch on Watchdog\n");               
	if (ioctl(fd, WDT_ENABLE, NULL))
		printf("**** Failure during switch on of watchdog \n");               
	else
		printf("**** Watchdog switched on served --> everything is fine.\n");               
	printf("*** Sleep 2\n");               
	sleep(2);

// Serve Watchdog for Task with 
	printf("Use IOCTL_Function to serve watchdog entry with PID: %d \n", my_pid);               
	if (ioctl(fd, WDT_SERVE_ME, &my_pid))
		printf("**** Failure during service of watchdog entry with PID: %d \n", my_pid);               
	else
		printf("**** Watchdog served --> everything is fine. (PID: %d) \n", my_pid);               
	printf("*** Sleep 20\n");               
	sleep(20);
	printf("Use IOCTL_Function to serve watchdog entry with PID: %d \n", my_pid);               
	if (ioctl(fd, WDT_SERVE_ME, &my_pid))
		printf("**** Failure during service of watchdog entry with PID: %d \n", my_pid);               
	else
		printf("**** Watchdog served --> everything is fine. (PID: %d) \n", my_pid);               
	printf("*** Sleep 20\n");               
	sleep(20);


// Unregister Task with current PID
	printf("Use IOCTL_Function to unregister my PID: %d \n", my_pid);               
	ioctl(fd, WDT_REMOVE_ME, &my_pid);   
	printf("*** Sleep 60\n");               
	sleep(60);
 
// Unregister Task with faked PID
	printf("Use IOCTL_Function to unregister my PID: %d \n", my_pid+1);               
	my_pid =  my_pid+1;
	ioctl(fd, WDT_REMOVE_ME, &my_pid);   

	}
return 0;
}

/*************************************************************************
 *
 * History:
 *	$Log: TriggerWDT.c,v $
 *	Revision 1.2  2005/06/08 11:55:05  sst
 *	- prepared for using with etcb
 *	
 *	Revision 1.1  2004/04/07 14:37:40  ias
 *	initial switch driver
 *	
 *	
 ************************************************************************/


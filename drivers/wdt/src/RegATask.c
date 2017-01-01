/**
 * @file   RegMEatWDT.c
 * @author Ingo Assmus <ias@pandatel.com>
 * @date   Mo Mar 29 16:23:17 2004
 * @version <pre> $Id: RegATask.c,v 1.2 2005/06/08 11:55:05 sst Exp $ </pre>
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


int register_a_task(int file, pid_t pid)
{
	wdt_registerded_task_t	my_task;		// setup IOCTL data
	int ioctl_return = 0;

	memset(&my_task,0,sizeof(my_task));		// clear my_task variable;

	my_task.pid =  pid;						// Fill struct
	my_task.call_period = 40 SEC;			// Melde mich spätestens alle 20 sec
	
// Register Task with  PID
	printf("----->  Use IOCTL_Function to register my PID: %d \n", pid);               
	strcpy(my_task.command, "\n**** Bitte achte auf mich. Ich bin doch neur ein kleiner Task !!!! ****\n");
	ioctl_return = ioctl(file, WDT_REGISTER_ME, &my_task);   
	printf("----->     IOCTL_Function returned: %d \n", ioctl_return);               

	return 0;
}

int serve_a_task(int file, pid_t pid, int times, int sec) {
// Serve Watchdog for Task pid n times after sec seconds 
	int i;
	for (i=0; i <= times; i++)
	{
	printf("----->  Use IOCTL_Function to serve watchdog entry with PID: %d \n", pid);               
	if (ioctl(file, WDT_SERVE_ME, &pid))
		printf("----->     **** Failure during service of watchdog entry with PID: %d \n", pid);               
	else
		printf("----->     **** Watchdog served --> everything is fine. (PID: %d) \n", pid);               
	printf("----->     Sleep %d Seconds \n", sec);               
	sleep(sec);
	}	
	return 0;
}

int unregister_a_task(int file, pid_t pid) {
	// Unregister Task with current PID
	printf("----->  Use IOCTL_Function to unregister my PID: %d \n", pid);               
	ioctl(file, WDT_REMOVE_ME, &pid);   
	return 0;
}


int main(int argc, const char *argv[]) {

	int fd;
	pid_t 	my_pid;							// in fact it's an int
	wdt_registerded_task_t	my_task;		// setup IOCTL data
	memset(&my_task,0,sizeof(my_task));		// clear my_task variable;
		
	fd=open("/dev/wdt",O_WRONLY);
   	printf("Try to open Watchdog-Device.\n");               
	
	if (fd==-1) 
	{
		perror("watchdog: could not open file");
		exit(1);
	}
	my_pid=getpid();						// Find out our PID to register at Watchdog_Device
	register_a_task(fd, my_pid);				// register me at the watchdog
	
	
	printf("\n----->     #### Uuups ..... forgot to unregister ...####\n");               
  	// close port
    close(fd);
	
return 0;
}

/*************************************************************************
 *
 * History:
 *	$Log: RegATask.c,v $
 *	Revision 1.2  2005/06/08 11:55:05  sst
 *	- prepared for using with etcb
 *	
 *	Revision 1.1  2004/04/07 14:37:40  ias
 *	initial switch driver
 *	
 *	
 *	
 ************************************************************************/


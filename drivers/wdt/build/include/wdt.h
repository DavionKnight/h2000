/**
 * @file   wdt.h
 * @author Ingo Assmus <ias@pandatel.com>
 * @date   Fri Jan 23 14:48:00 2004
 * @version <pre> $Id: wdt.h,v 1.1 2004/04/07 14:37:40 ias Exp $ </pre>
 *
 * @brief  local Header for WDT driver
 *
 * This file contains functions to handle the watchdog driver ...
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

#ifndef __WDT_H__
#define __WDT_H__

#include <linux/types.h>			// for pid_t and list_head
#include <linux/list.h>
//#include <TriggerWDT.h>

#define 	MAX_LEN	64				// 64 Byte space for comments

struct wdt_registerded_task_s {
  	struct list_head	listhead;		/* head of linked list 	*/
	pid_t				pid;			/* (int) PID of lock holder   */
	unsigned long 		call_period;	/* interval for task to call WDT in jiffies (HZ = 1sec, normaly HZ = 100) */
	unsigned long 		expires;		/* expiration of this task in jiffies (system jiffies >= expires: Oh, oh !*/
	unsigned long 		last_jiffies;	/* jiffies value during last service to avoid jiffies overflow every 497 days!*/
	char				command[MAX_LEN];	/* extra space for comments */
};
typedef struct wdt_registerded_task_s wdt_registerded_task_t;


#endif /* __WDT_H__ */

/**@page History
 * <pre>
 * $Log: wdt.h,v $
 * Revision 1.1  2004/04/07 14:37:40  ias
 * initial switch driver
 *
 *
 * </pre>
 */











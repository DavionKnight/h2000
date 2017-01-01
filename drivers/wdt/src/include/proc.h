/**
 * @file  proc.h
 * @brief Include file for internal watchdog proc handling
 */

/* 
 * (c) COPYRIGHT 2003 by Pandatel AG Germany
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
 
#ifndef __MY_PROC_H__
#define __MY_PROC_H__

extern struct list_head registered_tasks_list;

int wdt_proc_init (void);
void wdt_proc_cleanup (void);

#endif /* __MY_PROC_H__ */



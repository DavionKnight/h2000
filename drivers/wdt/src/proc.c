/**
 * @file   proc.c
 * @author Ingo Assmus <ias@pandatel.com>
 * @date   Mo Mar  29 17:59:38 2004
 * @version <pre>$Id: proc.c,v 1.2 2004/04/13 08:37:37 ias Exp $</pre>
 *
 * @brief  General methods for a consistent proc-file handling
 *
 * @todo better error handling when creating new proc files/dirs
 * @todo set max len of proc file name via const/define and not static
 *
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
//#include <linux/config.h>
#include <linux/jiffies.h>

#include <asm/uaccess.h>
#include <linux/list.h>				/* for list head	*/
#include <linux/module.h>
//#include "libproc.h"				/* Proc file handling */

#include "proc.h"
#include "wdt.h"

/*
* Defines just for dubugging the driver
*/
#define DBG
#undef DBG

#ifdef DBG
# define debugk(fmt,args...) printk(fmt ,##args)
#else
# define debugk(fmt,args...)
#endif

#define WDT_OK			     (0x00)	/* Operation succeeded                   */
#define WDT_FAIL			 (0x01)	/* Operation failed                      */

/* global variable which are available to all wdt driver modules. They should
 * influence the behaviour of all proc read functions*/


// ////////////////////////////////////////////////////////////////////////
// proc interface

/* global variable which are available to all wdt driver modules. They should
 * influence the behaviour of all proc read functions*/
int wdt_proc_reset=0;		// if set, reset all counters, which were shown in the proc output
int wdt_proc_verbose=0;		// the higher the variable the more verbose the output should be
int wdt_proc_debug=0;		// debug level for modules printk() output

/* *************************************************************************
 * Variables from the watchdog driver displayed or inflenced by proc
 ***************************************************************************/
extern int enabled;
extern int device_open;
extern int device_exclusiv_open_only;
extern int timeout_open_only;
extern unsigned long timer_count;
extern unsigned long timer_period;
extern int soft_margin;

/* static global variables for proc handling */


/* ********************************************************************
 *  start of driver specific code
 **********************************************************************/


// ////////////////////////////////////////////////////////////////////////
//  proc functions which show/manipulate wdt parameter

// show proc options for wdt proc system
static int
wdtproc_show_proc(char *buf,char **start,off_t offset, int count,int *eof,void *data)
{
  int len;
  len = sprintf(buf, "WDT: Watchdog Proc options\n");
  len += sprintf(buf+len, "Auto clear on show is %s\n", wdt_proc_reset?"on":"off");
  len += sprintf(buf+len, "Verbose level is %i\n", wdt_proc_verbose);
  *eof=1;
  return len;
}

/**
 * Set proc options for WDT proc files
 *
 * verbose - verbose level for output
 * reset   - reset timer for ...
 *
 * Syntax: echo FORMAT > /proc/path/to/this/file
 *
 * FORMAT may be
 *
 * - number  -- sets verbose to number and reset to 1 (on)
 * - verbose number -- sets verbose to number
 * - reset number -- sets reset to number (0=off, else 1)
 * - reset on|off -- sets reset
 * - off -- sets reset to 0 (off) and verbose to 0
 * - on -- sets reset to 1 (on) and verbose to 1
 *
 */
static void
wdtproc_set_proc(const char *buffer, void *data)
{
  int number;
  if (sscanf(buffer,"%u", &number)==1) {
    wdt_proc_verbose = number;
    wdt_proc_reset = 1;
  } else if (sscanf(buffer,"verbose %u", &number)==1) {
    wdt_proc_verbose=number;
  } else 	if (sscanf(buffer,"reset %u", &number)==1) {
    wdt_proc_reset=number;
  } else 	if (strcmp(buffer,"reset on")==0) {
    wdt_proc_reset = 1;
  } else 	if (strcmp(buffer,"reset off")==0) {
    wdt_proc_reset = 0;
  } else 	if (strcmp(buffer,"off")==0) {
    wdt_proc_reset = 0;
    wdt_proc_verbose = 0;
  } else 	if (strcmp(buffer,"on")==0) {
    wdt_proc_reset = 1;
    wdt_proc_verbose = 1;
  }
}

// show debug information for wdt proc system
static int
wdtproc_show_debug(char *buf,char **start,off_t offset, int count,int *eof,void *data)
{
  int len;
  len = sprintf(buf, "WDT: Watchdog Debug output\n");
  len += sprintf(buf+len, "Debug level is %i\n", wdt_proc_debug);
  *eof=1;
  return len;
}

/**
 * Set proc file behaviour via proc file
 *
 * verbose - verbose level for output
 * reset   - reset counter after displaying them
 *
 * Syntax: echo FORMAT > /proc/path/to/this/file
 *
 * FORMAT may be
 *
 * - number  -- sets debug level to number
 * - level number -- sets debug level to number
 *
 */
static void
wdtproc_set_debug(const char *buffer, void *data)
{
  int number;
  if ( (sscanf(buffer,"%u", &number)==1) ||
       (sscanf(buffer,"level %u", &number)==1) ) {
    if ( number > 9 )
      number = 9;
    wdt_proc_debug = number;
  }
}


/* ==================================================================== */
/*
* PROC Files servive routines
*/

// show registered tasks: Displays all registered tasks, which are under control of the watchdog driver
static int
wdtproc_show_registered_tasks(char *buf,char **start,off_t offset, int count,int *eof,void *data)
{
  	int 	len;
   	struct list_head 		*ptr      	= (struct list_head  *) NULL;
   	wdt_registerded_task_t 	*ptr2      	= (wdt_registerded_task_t  *) NULL;
   	int 	i = 0;

	debugk (__FUNCTION__ " show an overview of what the wdt will handle \n");

  	len = sprintf(buf, "\n*** Watchdog Timer overview: \n");

  	len += sprintf(buf+len, "enabled: (0=no, 1=yes)            %d (%s)\n", enabled, enabled?"enabled":"disabled");
  	len += sprintf(buf+len, "exclusiv open: (0=no, 1=yes)      %d (%s)\n", device_exclusiv_open_only, \
														device_exclusiv_open_only?"open exclusic":"multiple open");
  	len += sprintf(buf+len, "opend by No. of tasks:            %d \n", device_open);
  	len += sprintf(buf+len, "time out open only: (0=no, 1=yes) %d (%s)\n", timeout_open_only, timeout_open_only?"time out is opend":"will not time out if not opened");
  	len += sprintf(buf+len, "RESET after No. of sec:           %d \n", soft_margin);
  	len += sprintf(buf+len, "remaing jiffies until HW RESET    %ld \n", timer_count);
  	len += sprintf(buf+len, "internal trigger period:          %ld \n", timer_period);

	for (ptr = registered_tasks_list.next; ptr != &registered_tasks_list; ptr = ptr->next)
     	{
		i++;
  			len += sprintf(buf+len, "  %d. registered task: \n", i);
			ptr2 = list_entry(ptr, wdt_registerded_task_t, listhead);
  			len += sprintf(buf+len, "   # PID of task                  %d \n", ptr2->pid);
  			len += sprintf(buf+len, "   # Calling Period in jiffies    %ld \n", ptr2->call_period);
  			len += sprintf(buf+len, "   # Will expire in (jiffies)     %ld \n", (ptr2->expires - jiffies));
  			len += sprintf(buf+len, "   # Last jiffies                 %ld \n", ptr2->last_jiffies);
  			len += sprintf(buf+len, "   # Extra info (1st 20 Bytes)   %20s \n\n", ptr2->command);

		}

  		len += sprintf(buf+len, "currently registered tasks:       %d \n", i);

  	*eof=1;
  	return len;
	}



//---------------------------------------------------------------------------------------------------------------------------
static int wdt_enable_read_proc (char *buf,char **start,off_t offset, int count,int *eof,void *data)
	{
  	int 	len;

	debugk (__FUNCTION__ " WDT enable is: %d\n", enabled);
  	len = sprintf(buf, "Enabled (0=no, 1=yes): %d\n", enabled);

  	*eof=1;
  	return len;
	}

/**
 * Set proc options for WDT proc files
 *
 * enable - enables the watchdog
 *
 * Syntax: echo FORMAT > /proc/path/to/this/file
 *
 * FORMAT may be
 *
 * - enable number -- sets enable to number (0=off, else 1)
 * - enable on|off -- sets enable bit
 * - off -- sets enable to 0 (off)
 * - on -- sets enable to 1 (on)
 *
 */
static void
wdt_enable_write_proc(const char *buffer, void *data)
{
  int number;
  if (sscanf(buffer,"%u", &number)==1) {
    enabled = number;
  } else 	if (sscanf(buffer,"%u", &number)==0) {
    enabled = 0;
  } else 	if (strcmp(buffer,"enable on")==0) {
    enabled = 1;
  } else 	if (strcmp(buffer,"reset off")==0) {
    enabled = 0;
  }
}


//---------------------------------------------------------------------------------------------------------------------------
static int wdt_open_read_proc (char *buf,char **start,off_t offset, int count,int *eof,void *data)
	{
  	int 	len;

	debugk (__FUNCTION__ " WDT opened by %d applications\n", device_open);
  	len = sprintf(buf, " WDT opened by %d applications\n", device_open);

  	*eof=1;
  	return len;
	}

//---------------------------------------------------------------------------------------------------------------------------
static int wdt_timer_read_proc (char *buf,char **start,off_t offset, int count,int *eof,void *data)
	{
  	int 	len;

	debugk (__FUNCTION__ " only %ld ticks left until reset will occur \n", timer_count);
  	len = sprintf(buf, " only %ld ticks left until reset will occur \n", timer_count);

  	*eof=1;
  	return len;
	}

//---------------------------------------------------------------------------------------------------------------------------

#ifdef DBG
static void
wdtproc_unload_debug(const char *buffer, void *data)
{
  int number;
  if (sscanf(buffer,"%u", &number)==1) {

  /* Zaehler auf 0 zurücksetzen, um bei Problemen das Entladen
   * zu ermöglichen.
   */
#if LINUX_KERNEL_VERSION < KERNEL_VERSION(2,4,0)
  while (MOD_IN_USE) {
      MOD_DEC_USE_COUNT;
  }
#endif /* LINUX_KERNEL_VERSION < KERNEL_VERSION(2,4,0) */
  }
}
#endif

// ////////////////////////////////////////////////////////////////////////
// Init and cleanup functions for wdt proc handling

// Init function for wdt proc handling. Creates wdt subdirectory and
// registers some proc files */
int
wdt_proc_init (void)
{
#if 0 /* tian*/
  if ( dp_proc_init("wdt") != 0 ) {
	printk(KERN_INFO "WDT: proc initialization failed... Exiting\n");
    debugk ("---%s: could not create PROC-FILE-SYSTEM !!!! \n",__FUNCTION__);

    return WDT_FAIL;
	}
  else
	{
    register_simple_proc_file("my_debug/proc", wdtproc_show_proc, wdtproc_set_proc, NULL);
    register_simple_proc_file("my_debug/level", wdtproc_show_debug, wdtproc_set_debug, NULL);
    register_simple_proc_file("my_debug/enable", wdt_enable_read_proc, wdt_enable_write_proc, NULL);
    register_simple_proc_file("my_debug/open_files", wdt_open_read_proc, NULL, NULL);
    register_simple_proc_file("my_debug/remaining_ticks", wdt_timer_read_proc, NULL, NULL);
#ifdef DBG
    register_simple_proc_file("my_debug/unload", NULL, wdtproc_unload_debug, NULL);
#endif
    register_simple_proc_file("my_global_stats", wdtproc_show_registered_tasks, NULL, NULL);
	}
  #endif
  return WDT_OK;
}

// see header file proc.h
// Cleanup wdt proc handling
void
wdt_proc_cleanup (void)
{
    //dp_proc_cleanup();
}

// ////////////////////////////////////////////////////////////////////////
/**@page History
 * @section proc_c CVS History of proc.c
 * <pre>
 * $Log: proc.c,v $
 * Revision 1.2  2004/04/13 08:37:37  ias
 * using libproc
 *
 * Revision 1.1  2004/04/07 14:37:40  ias
 * initial switch driver
 *
 *
 * </pre>
 */


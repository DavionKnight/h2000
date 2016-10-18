/**
 * @file   ioctl_codes.h
 * @author Ingo Assmus <ias@pandatel.com>
 * @date   Mon Jan 26 10:46:34 2004
 * @version <pre> $Id: ioctl_codes.h,v 1.1 2004/04/07 14:37:40 ias Exp $ </pre>
 *
 * @brief  switch device driver handler and manager
 *
 * This file contains the ioctl commands ...
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

#ifndef __IOCTL_CODES_H__
#define __IOCTL_CODES_H__

#include <linux/ioctl.h>			/* declares MACROS for the ioctl usage */
#include <linux/types.h>			/* for pid_t and list_head */
#include <linux/watchdog.h>			/* for struct watchdog_info */

/*
 * Ioctl definitions
 */


/* Use 'w' as magic number */
#define WDT_IOC_MAGIC  'w'

/*
 * S means "Set" through a ptr,
 * G means "Get": reply by setting through a pointer
 * T means "Tell" directly with the argument value
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */

/* Konstanten für ioctl */

#define WDT_GET_STATUS	        _IOR(WDT_IOC_MAGIC, 1, int *)		// show current status of Watchdog (testing only)
#define WDT_OPEN_ONLY			_IO(WDT_IOC_MAGIC, 	2)				// watchdog will only time out if device is opened by at least one task
#define WDT_ALWAYS_TIME_OUT		_IO(WDT_IOC_MAGIC, 	3)				// watchdog will always time out even if driver ist not open by any task
#define WDT_ENABLE  			_IO(WDT_IOC_MAGIC, 	4)				// main switch to enable watchdog
#define WDT_DISABLE  			_IO(WDT_IOC_MAGIC, 	5)				// main switch to disable watchdog (stil running but no reset (restart) will occur in case of fault)
#define WDT_REGISTER_ME    		_IOW(WDT_IOC_MAGIC, 6, wdt_registerded_task_t *)		// register an new PID (task) to be supervised
#define WDT_REMOVE_ME    		_IOW(WDT_IOC_MAGIC, 7, pid_t *)		// remove a PID (task) forim watchdog control
#define WDT_SERVE_ME    		_IOW(WDT_IOC_MAGIC, 8, pid_t *)		// pid_t used to send "task alive" messages
#define WDT_EXCLUSIV_OPEN_ONLY  _IO(WDT_IOC_MAGIC, 	9)				// Device can be opend only by one task (if already opend by more than one task only new openings are forbidden)
#define WDT_ALWAYS_OPEN  		_IO(WDT_IOC_MAGIC, 	10)				// Device can be opend by unlimited tasks
#define WDT_GETSUPPORT			_IOR(WDT_IOC_MAGIC, 11, struct watchdog_info *)		// Return an identifier struct
#define IOCTL_HARDRESET			_IO(WDT_IOC_MAGIC,12)				// reable unloading in case of error

#define WDT_IOCTL_MAXNR 12


#endif /* __IOCTL_CODES_H__ */

/**@page History
 * @section dev_manage_c CVS History of dev_manage.c
 * <pre>
 * $Log: ioctl_codes.h,v $
 * Revision 1.1  2004/04/07 14:37:40  ias
 * initial switch driver
 *
 *
 * </pre>
 */









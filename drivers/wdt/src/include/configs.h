/**
 * @file   configs.h
 * @author Ingo Assmus <ias@pandatel.com>
 * @date   Thu Apr 01 18:48:17 2004
 * @version <pre> $Id: configs.h,v 1.8.10.1 2007/06/26 14:10:19 sst Exp $ </pre>
 *
 * @brief  local Header for WDT driver containing relevant Config settings
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

#ifndef __CONFIGS_H__
#define __CONFIGS_H__

//---------------------------------------------------------------------------------------------------------------------------
/* Force module to use the defined timer for watchdog activities even if no application is registerd
 * This module will setup an software timer to check peridically if every registerd application is ok.
 * this timer will not be stoped if <b>CONFIG_WATCHDOG_NOWAYOUT</b> is defined
 *
 */
#define CONFIG_WATCHDOG_NOWAYOUT  1		/* Don't ever relase the started timer if defined */
//---------------------------------------------------------------------------------------------------------------------------

/* <b>CONFIG_WDT_MPC82XX_TIMEOUT</b> defines the timeout value. In case an application
 * does not send "alive messges" a reset will occur after <b>CONFIG_WDT_MPC82XX_TIMEOUT</b> seconds
 * if <b>CONFIG_WDT_MPC82XX_TIMEOUT</b> is not defined the timeout value is set to 300 sec
 *
 */
#define CONFIG_WDT_MPC82XX_TIMEOUT 	20 	/* reset after 20 seconds	*/

//---------------------------------------------------------------------------------------------------------------------------

/* If <b>CONFIG_WDT_MPC82XX_TIMEOUT_OPEN_ONLY</b> is defined the watchdog will only time out if
 * it is opend by an application
 */

//#define CONFIG_WDT_MPC82XX_TIMEOUT_OPEN_ONLY	// WDT is enabled only if driver is opend by an application
#undef CONFIG_WDT_MPC82XX_TIMEOUT_OPEN_ONLY	// WDT is enabled only if driver is opend by an application

//---------------------------------------------------------------------------------------------------------------------------

/* This macro defines the trigger periode for internal checkups of registered tasks
 * re-trigger watchdog by default every 500 ms = HZ / 2. (HZ = No. of jiffies in 1 sec. see include/asm/param.h)
 *
 * The external interface is in seconds, but internal all calculation
 * is done in jiffies.
 */

/* Commented by Tom.Liu on 2009-1-15 17:55:41 */
/*
#if defined(CONFIG_coptype) || defined(CONFIG_invmux) || defined(CONFIG_etcc) \
    || defined(CONFIG_mts180) || defined(CONFIG_cmtsge) || defined (CONFIG_mts180p16)
# define TRIGGER_PERIOD   ( HZ / 4 )			// Sofwaretimer to check status of registered tasks (check every 1/4 sec)
#endif

#if defined(CONFIG_gmee)
# define TRIGGER_PERIOD   ( 6 * HZ )			// Sofwaretimer to check status of registered tasks (check every 6 sec)
#endif

#if defined(CONFIG_etcb)
# define TRIGGER_PERIOD   ( HZ / 4 )			// Sofwaretimer to check status of registered tasks (check every 1/4 sec)
#endif

#if defined(CONFIG_mmc)
# define TRIGGER_PERIOD   ( HZ )				// Sofwaretimer to check status of registered tasks (check every 1 sec)
#endif
*/
/* Added by Tom.Liu on 2009-1-15 17:55:41 */
# define TRIGGER_PERIOD   ( HZ / 4 )			// Sofwaretimer to check status of registered tasks (check every 1/4 sec)
//---------------------------------------------------------------------------------------------------------------------------

/*
 * Watchdog active? When disabled, it will get re-triggered
 * automatically without timeout, so it appears to be switched off
 * although actually it is still running.
 */
#define WDT_ENABLED

//---------------------------------------------------------------------------------------------------------------------------

/* If this macro is set the device can be opend by one application only
*/
//#define CONFIG_WDT_MPC82XX_OPEN_ONLY_BY_ONE	// WDT may be opend only by one application
#undef CONFIG_WDT_MPC82XX_OPEN_ONLY_BY_ONE		// WDT may be opend only every application

#endif /* __CONFIGS_H__ */

/**@page History
 * <pre>
 * $Log: configs.h,v $
 * Revision 1.8.10.1  2007/06/26 14:10:19  sst
 * - etcc
 *
 * Revision 1.11  2007/01/19 13:40:41  sst
 * - added support for ETCC
 *
 * Revision 1.10  2006/11/16 09:55:38  uma
 * adapted for invmux
 *
 * Revision 1.9  2006/10/17 08:19:47  sst
 * - prepared for etcc project
 *
 * Revision 1.8  2006/02/01 16:02:26  sst
 * - ernamed project mmmc to mmc
 *
 * Revision 1.7  2005/12/19 10:38:20  sst
 * - adapted gme project ro global config flags
 *
 * Revision 1.6  2005/12/16 16:10:35  sst
 * - new compiler flag: CONFIG_coptype
 *
 * Revision 1.5  2005/08/31 17:41:36  ias
 * wdt trigger periode 250ms
 *
 * Revision 1.4  2005/08/30 09:10:30  sst
 * - prepared for mmmc project
 *
 * Revision 1.3  2005/06/08 11:55:05  sst
 * - prepared for using with etcb
 *
 * Revision 1.2  2004/08/17 17:51:49  ias
 * trigger periode is now 250ms
 *
 * Revision 1.1  2004/04/07 14:37:40  ias
 * initial switch driver
 *
 *
 * </pre>
 */




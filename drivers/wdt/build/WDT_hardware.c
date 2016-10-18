/**
 * @file   WDT_hardware.c
 * @author Ingo Assmus <ias@pandatel.com>
 * @date   Mon Mar 22 11:26:16 2004
 * @version <pre> $Id: WDT_hardware.c,v 1.9.10.1 2007/06/26 14:10:19 sst Exp $ </pre>
 *
 * @brief Hardware related Watchdog functions
 *
 * This file contains functions to handle the Watchdog HW of the MPC8260
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
//#include <linux/config.h>
#include <linux/types.h>				// define Types which will be used for immap_t
#include <asm/uaccess.h>				// define Register and values for CPU (i.e IMMAP_ADR)
#include "WDT_hardware.h"
#if defined(CONFIG_8260)
#include <asm/immap_cpm2.h>				/* internal RAM Area of MPC8260 */
#elif defined(CONFIG_83xx)
#include <asm/io.h>
#elif defined(CONFIG_8xx)
#include <asm/8xx_immap.h>
#endif
#include <linux/delay.h>				/*udelay*/
#include <linux/io.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>



#define DEBUG					/* Debug-Ausgaben Einschalten 		*/
#undef DEBUG					/* Debug-Ausgaben Ausschalten 		*/

#ifdef DEBUG
# define debugk(fmt,args...) printk(fmt ,##args)
#else
# define debugk(fmt,args...)
#endif



volatile ccsr_gpio_t *ccsr_gpio = NULL;


extern void __iomem * ioremap(phys_addr_t addr, unsigned long size);


extern phys_addr_t get_immrbase(void);

extern int gpio_direction_output(unsigned gpio, int value);




/*
* function to set external watchdog into long-trigger-periode-mode
*/
int slow_down_watchdog_hw(void)
{

	debugk ("%s: WDT slow down mode activated\n", __FUNCTION__);

	WDT_LONG_BIT(1);		/* Set pin to high (long trigger periode)*/

	serve_watchdog_hw();	/* serve it one more time */

	return 0;
}

/*
* Serve function for GPIO controlled external Watchdog Hardware
*/
int serve_watchdog_hw(void)
{

	//debugk ("%s: WDT serviced\n", __FUNCTION__);

	clrbits32(&ccsr_gpio->gpdat, 0x40000000);
	WDTDELAY;
        setbits32(&ccsr_gpio->gpdat, 0x40000000);


	return 0;
}

/* **********************************************************
 * Hardware init function for GPIO Hardware watchdog *
 ************************************************************/

int hardware_init(void)
{
	void __iomem *immap;

	immap = ioremap(get_immrbase()+0xf000,64);
	
        //iounmap(immap);
	ccsr_gpio =(ccsr_gpio_t *)immap;

        setbits32(&ccsr_gpio->gpdir, 0x40000000);
        setbits32(&ccsr_gpio->gpdir, 0x10000000);

        clrbits32(&ccsr_gpio->gpdat, 0x10000000);
        setbits32(&ccsr_gpio->gpdat, 0x40000000);
        WDTDELAY;
        clrbits32(&ccsr_gpio->gpdat, 0x40000000);

	/* trigger now */
	serve_watchdog_hw();


	#if 1
	printk("Your Hardware Watchdog is enabled\n\n");
	#endif

	/* trigger now */
	serve_watchdog_hw();
	return 0;
}

void hardware_clean(void)
{



}




/**@page History
 * <pre>
 * $Log: WDT_hardware.c,v $
 * Revision 1.9.10.1  2007/06/26 14:10:19  sst
 * - etcc
 *
 * Revision 1.11  2006/10/17 11:17:50  sst
 * - improved CONFIGS for etcb end etcc
 *
 * Revision 1.10  2006/10/17 08:19:46  sst
 * - prepared for etcc project
 *
 * Revision 1.9  2006/02/01 16:02:26  sst
 * - ernamed project mmmc to mmc
 *
 * Revision 1.8  2005/08/30 09:10:29  sst
 * - prepared for mmmc project
 *
 * Revision 1.7  2005/07/11 17:05:12  ias
 * GPIO Pins adapted to ETCB
 *
 * Revision 1.6  2004/08/17 17:51:49  ias
 * trigger periode is now 250ms
 *
 * Revision 1.5  2004/08/17 17:21:00  ias
 * serving WDI but no change on WDS
 *
 * Revision 1.4  2004/08/17 13:15:53  ias
 * serve before going into short trigger mode
 *
 * Revision 1.3  2004/08/16 08:24:25  ias
 * Cop RelII HW added
 *
 * Revision 1.2  2004/08/12 14:56:21  ias
 * added support for Cop-Rel-II hardware
 *
 * Revision 1.1  2004/04/07 14:37:40  ias
 * initial switch driver
 *
 *
 * </pre>
 */


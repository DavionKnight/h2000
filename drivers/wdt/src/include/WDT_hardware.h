/**
 * @file   WDT_hardware.h
 * @author Zhongyong Tian<tianzy@huahuan.com>
 */


#ifndef __WDT_HARDWARE_H__
#define __WDT_HARDWARE_H__

#include "wdt.h"					/* contains main switch to define if Watchdog Hardware will be supportted */





//#include <asm/mpc83xx.h>
#include <asm/immap_85xx.h>
//#include <asm/fsl_guts.h>  /* 3048 */

#define WDT_LONG_PIN	0x10000000  /* GPIO3 */		

#define WDT_PIN			0x40000000  /* GPIO1 */ 		
#define EXTERNAL_HARDWARE_WDT
#undef	USE_HARDWARE_WDT		


#if defined(USE_HARDWARE_WDT) || defined(EXTERNAL_HARDWARE_WDT) 	/* enable only if Hardware is supported */
int hardware_init(void);
void hardware_clean(void);
int serve_watchdog_hw(void);
int slow_down_watchdog_hw(void);
#endif


#ifdef EXTERNAL_HARDWARE_WDT	


typedef struct ccsr_gpio {
	u32	gpdir;
	u32	gpodr;
	u32	gpdat;
	u32	gpier;
	u32	gpimr;
	u32	gpicr;
} ccsr_gpio_t;

extern volatile ccsr_gpio_t *ccsr_gpio;


#define WDT_ACTIVE		(ccsr_gpio->gpdat &= (~WDT_LONG_PIN))
#define WDT_ENA        (ccsr_gpio->gpdir |= (WDT_LONG_PIN |WDT_PIN) )

#define WDT_BIT(bit)		if(bit) ccsr_gpio->gpdat |= WDT_PIN;\
				else	ccsr_gpio->gpdat &= ~WDT_PIN


#define WDT_LONG_ACTIVE		(ccsr_gpio->gpdat  |= WDT_LONG_PIN)
#define WDT_LONG_TRISTATE	(ccsr_gpio->gpdat  &= ~WDT_LONG_PIN)

#define WDT_LONG_BIT(bit)		if(bit) ccsr_gpio->gpdat  |= WDT_LONG_PIN;\
				else	ccsr_gpio->gpdat  &= ~WDT_LONG_PIN
				
#define WDTDELAY		udelay(20)
//#define WDTDELAY		udelay(1)

#endif /* __CONFIGS_H__ */

#endif










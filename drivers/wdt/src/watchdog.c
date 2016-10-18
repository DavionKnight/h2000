/**
 * @file   watchdog.c.c
 * @author Ingo Assmus <ias@pandatel.com>
 * @date   Fri Jan 23 14:48:00 2004
 * @version <pre> $Id: watchdog.c,v 1.11.10.1 2007/06/26 14:10:19 sst Exp $ </pre>
 *
 * @brief  the watchdog driver handler and manager
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
//#include <linux/config.h>

//#include <linux/config.h>

#include <linux/module.h>
#include <linux/types.h>				// pid_t
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/reboot.h>
//#include <linux/smp_lock.h>  /* 3048*/
#include <linux/init.h>
#include <linux/notifier.h>
#include <asm/uaccess.h>
#include <linux/slab.h>					/* get and free memory (kmalloc, kfree) */

#include <linux/version.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
//#include <asm/immap_cpm2.h>			/* internal RAM Area of MPC8260 */
#include <linux/list.h>				/* for list head	*/


#include "wdt.h"
#include "ioctl_codes.h"				/* Codes uesed within ioctl_function */
#include "WDT_hardware.h"				/* service functions for Watchdog Hardware */
#include "configs.h"					/* configuration for that driver */

#if defined(USE_HARDWARE_WDT) || defined(EXTERNAL_HARDWARE_WDT) 	/* enable only if Hardware is supported */
//int hardware_init(void);
//int serve_watchdog_hw(void);
#endif
//---------------------------------------------------------------------------------------------------------------------------
/*
* Defines just for dubugging the driver
*/

//#define DEBUG					/* Debug-Ausgaben Einschalten 		*/
#undef DEBUG					/* Debug-Ausgaben Ausschalten 		*/
//#define EXT_DEBUG				/* Show detailed debuginformations		*/
//#define ONLY_TESTING			1	/* Nur Testmode im Fehlerfall kein Restart auslösen */
//#define SHOW_LINKED_LIST

#ifdef DEBUG
# define debugk(fmt,args...) printk(fmt ,##args)
#else
# define debugk(fmt,args...)
#endif


//---------------------------------------------------------------------------------------------------------------------------

#define	WDT_VERSION	"1.0"							/* version of this device driver		*/
static const char banner[] __initdata = KERN_INFO "Software Watchdog Driver: Ver 1.0 Check Interval: %d sec\n$Id: watchdog.c,v 1.11.10.1 2007/06/26 14:10:19 sst Exp $\n";


#ifdef	CONFIG_WDT_MPC82XX_TIMEOUT
#define TIMEOUT_VALUE CONFIG_WDT_MPC82XX_TIMEOUT	/* configurable timeout		*/
#else
#define TIMEOUT_VALUE 300							/* reset after five minutes = 300 seconds	*/
//#define TIMEOUT_VALUE 90							/* reset after five minutes = 300 seconds	*/
#endif
//---------------------------------------------------------------------------------------------------------------------------

/* If <b>CONFIG_WDT_MPC82XX_TIMEOUT_OPEN_ONLY</b> is defined the watchdog will only time out if
 * it is opend by an application
 */

#ifdef CONFIG_WDT_MPC82XX_TIMEOUT_OPEN_ONLY

int timeout_open_only = 1;
#else
int timeout_open_only = 0;
#endif
//---------------------------------------------------------------------------------------------------------------------------

/* This macro defines the trigger periode for internal checkups of registered tasks
 * re-trigger watchdog by default every 500 ms = HZ / 2. (HZ = No. of jiffies in 1 sec. see include/asm/param.h)
 *
 * The external interface is in seconds, but internal all calculation
 * is done in jiffies.
 */


unsigned long timer_count = TIMEOUT_VALUE * HZ;			/* remaining time until system will setup an reset 	*/
unsigned long timer_period= TRIGGER_PERIOD;				/* period to trigger WD	*/
unsigned int  do_hard_reset = 0;
//---------------------------------------------------------------------------------------------------------------------------

/*
 * Watchdog active? When disabled, it will get re-triggered
 * automatically without timeout, so it appears to be switched off
 * although actually it is still running.
 */

#ifdef WDT_ENABLED
int enabled = 1;
#else
int enabled = 0;
#endif

//---------------------------------------------------------------------------------------------------------------------------

/*
 * Watchdog opened by an external user programm?
 */
int device_open = 0;

/* If this macro is set the device can be opend by one application only
*/

#ifdef CONFIG_WDT_MPC82XX_OPEN_ONLY_BY_ONE
int device_exclusiv_open_only = 1;					/* only one application may open device wdt	*/
#else
int device_exclusiv_open_only = 0;					/* more than one application may open device wdt*/
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*
 * The name for our device, as it will appear in /proc/devices
 */
#define DEVICE_NAME "wdt"


//---------------------------------------------------------------------------------------------------------------------------
/*
* setup list for registered tasks. Entry point (list_head structure)
*/
LIST_HEAD(registered_tasks_list);

//---------------------------------------------------------------------------------------------------------------------------

int wdt_proc_init (void);
void wdt_proc_cleanup (void);


int soft_margin = TIMEOUT_VALUE;	/* Reset will occur in seconds if registered taskes are not served*/

#ifdef MODULE
//extern int init_module(void);
//extern int cleanup_module(void);

module_param(soft_margin, int, 0);

//MODULE_PARM(soft_margin,"i");


#endif /* def MODULE */
//---------------------------------------------------------------------------------------------------------------------------



//---------------------------------------------------------------------------------------------------------------------------
/*
 *	used functions
*/

static void watchdog_fire(unsigned long);						// WDT service
static ssize_t softdog_write(struct file *file, const char *data, size_t len, loff_t *ppos);		// Write: nothing specific
static ssize_t softdog_read (struct file *filp, char *data, size_t len,   loff_t *ppos);			// Read: nothing specific
static int softdog_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);	// IOCTL: control of WDT
static int softdog_open(struct inode *inode, struct file *file);					// simple open
static int softdog_release(struct inode *inode, struct file *file);				// close

/*
 *	register tsaks and related functions
*/
//static int wdt_test_register_task (unsigned long count, const char *buffer, int *parm);
static int wdt_unregister_task (pid_t taskpid);
static int wdt_serve_task (pid_t taskpid);
static int wdt_task_already_registered (pid_t taskpid);
static int serve_all_tasks (void);
static int wdt_register_task (wdt_registerded_task_t *task);
static int wdt_cleanup_tasklist (void);
static int check_registered_tasks(void);
#ifdef EXT_DEBUG
static int  wdt_show_linked_list (void);
#endif

/*
 *	clean up driver for unload
*/
#ifdef MODULE
static
#else
extern
#endif
int __init watchdog_init(void);
static void watchdog_cleanup(void);

//---------------------------------------------------------------------------------------------------------------------------

static struct file_operations softdog_fops = {
	owner:		THIS_MODULE,
	write:		softdog_write,
	read:		softdog_read,
	//ioctl:		softdog_ioctl,
	unlocked_ioctl:		softdog_ioctl, /* 3048*/
	open:		softdog_open,
	release:	softdog_release,
};

//---------------------------------------------------------------------------------------------------------------------------
static struct miscdevice softdog_miscdev = {
	minor:		MISC_DYNAMIC_MINOR,
	name:		"wdt watchdog driver",
	fops:		&softdog_fops,
};

//---------------------------------------------------------------------------------------------------------------------------
struct timer_list watchdog_ticktock = {
	function:	watchdog_fire,			// soft timer-function called every TIMEOUT_VALUE seconds
};

/*
*/
//---------------------------------------------------------------------------------------------------------------------------
/*
 *	Allow only one person (task) to hold it open
 */

static int softdog_open(struct inode *inode, struct file *filp)
{
	debugk ("ENTER %s (%p, %p)\n", __FUNCTION__, inode, filp);
//	printk ("ENTER %s (%p, %p)\n", __FUNCTION__, inode, filp);

	if (device_open && device_exclusiv_open_only) {		/* exclusive open only */
		debugk ("%s: WDT is busy\n", __FUNCTION__);
		return -EBUSY;
	}
	device_open++;						/* increment usage counter */
	debugk ("%s: WDT is open\n", __FUNCTION__);
	return 0;
}

//---------------------------------------------------------------------------------------------------------------------------

static int softdog_release(struct inode *inode, struct file *filp)
{
	debugk ("ENTER %s (%p, %p)\n", __FUNCTION__, inode, filp);

	device_open--;						/* decrement usage counter */

#ifndef CONFIG_WATCHDOG_NOWAYOUT
	lock_kernel();
	del_timer(&watchdog_ticktock);
	unlock_kernel();
#endif
	debugk ("%s: WDT is closed\n", __FUNCTION__);
	return 0;
}

//---------------------------------------------------------------------------------------------------------------------------

static ssize_t softdog_read (struct file *filp, char *data, size_t len,   loff_t *ppos)
{
	unsigned int rest_count = timer_count / HZ;

	debugk ("ENTER %s (%p, %p, %d, %p)\n",
		__FUNCTION__, filp, data, len,  ppos);

	if (len < sizeof(rest_count)) {
		debugk ("wdt_mpc82xx_release/kernel: invalid argument\n");

		return -EINVAL;
	}
	put_user (rest_count, (int *) data);				/* copy value into userspace */

	debugk ("%s: rest_count=%i\n", __FUNCTION__, rest_count);

	return (sizeof(rest_count));					/* read always exactly 4 bytes */
}

//---------------------------------------------------------------------------------------------------------------------------
static ssize_t softdog_write(struct file *filp, const char *data, size_t len, loff_t *ppos)
{
	int error;
	unsigned int new_count;

	debugk ("ENTER %s (%p, %p, %d, %p)\n",
		__FUNCTION__, filp, data, len,  ppos);

	/*  Can't seek (pwrite) on this device  */
	if (ppos != &filp->f_pos)
		return -ESPIPE;

	if (len != sizeof(new_count)) {
		debugk ("%s: invalid length (%d instead of %d)\n",
			__FUNCTION__, len, sizeof(new_count));

		return -EINVAL;
	}

	/* copy count value into kernel space */
	if ((error = get_user (new_count, (int *) data)) != 0) {
		debugk ("%s: get_user failed: rc=%d\n",
			__FUNCTION__, error);

		return (error);
	}

	timer_count = new_count;

	return (sizeof(new_count));
}


//---------------------------------------------------------------------------------------------------------------------------
static int softdog_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{

	static struct watchdog_info ident = {
		identity: "Software Watchdog",
	};
	wdt_registerded_task_t 	tmp;
	pid_t			tmp_pid;

	memset(&tmp,0,sizeof(tmp));		// clear tmp variable;

	debugk ("---ENTER %s \n",__FUNCTION__);


	if (_IOC_TYPE(cmd) != WDT_IOC_MAGIC)	return -ENOTTY;
	if (_IOC_NR(cmd) > WDT_IOCTL_MAXNR)	return -ENOTTY;

	switch (cmd) {

/*-----------------------------------------------------------------------------------------------------------------------------------*/
 		case WDT_SERVE_ME:					// Still alive message from task for WDT
			debugk ("---WDT_SERVE_ME entered with arg %08lx \n", arg);
			if (arg != 0) {
				if (copy_from_user(&tmp_pid, (char *) arg, sizeof(tmp_pid))) {
					debugk ("---Could not copy every Byte of %d \n", sizeof(tmp_pid));
					return -EFAULT;	// Not every Byte could be copied
					}
				debugk ("---Copied %d Bytes \n", sizeof(tmp));
				debugk ("---Your PID is %d  \n", tmp_pid);

				if (wdt_serve_task (tmp_pid)) {
					debugk ("---Something went wrong during service for your PID: %d  \n", tmp_pid);
					return -EAGAIN;		// Try again
				}

			} else {
				debugk ("---Es wurde nichts übergeben ... \n");
				return -EINVAL;			// Invalid argument
			}
			break;
/*-----------------------------------------------------------------------------------------------------------------------------------*/
 		case WDT_REGISTER_ME:		// register a new task to be supervised by WDT
			debugk ("----WDT_REGISTER_ME entered with arg %08lx \n", arg);
			if (arg != 0) {
				if (copy_from_user(&tmp, (char *) arg, sizeof(tmp))) {
					debugk ("---Could not copy every Byte of %d \n", sizeof(tmp));
					return -EFAULT;	// Not every Byte could be copied
					}
				debugk ("---Copied %d Bytes into new structure \n", sizeof(tmp));
				debugk ("---tmp.pid = %d  \n", tmp.pid);
				debugk ("---tmp.expires = %ld  \n", tmp.expires);

				if (wdt_register_task (&tmp)) {
					debugk ("---Something went wrong during registration of PID %d  \n", tmp.pid);
					return -EAGAIN;		// Try again
				}

			} else {
				debugk ("---Es wurde nichts übergeben ... \n");
				return -EINVAL;			// Invalid argument
			}
			break;
/*-----------------------------------------------------------------------------------------------------------------------------------*/

		case WDT_REMOVE_ME:		// remove a task from supervision of WDT
			debugk ("---WDT_REMOVE_ME entered with arg %08lx \n", arg);
			if (arg != 0) {
				if (copy_from_user(&tmp_pid, (char *) arg, sizeof(tmp_pid))) {
					debugk ("---Could not copy every Byte of %d \n", sizeof(tmp_pid));
					return -EFAULT;	// Not every Byte could be copied
					}
				debugk ("---Copied %d Bytes into kernel space \n", sizeof(tmp_pid));
				debugk ("---PID = %d  \n", tmp_pid);
				if (wdt_unregister_task (tmp_pid)) {
					debugk ("---Something went wrong during unregistration of PID %d  \n", tmp_pid);
					return -ENXIO;
					}
				}
			break;

/*-----------------------------------------------------------------------------------------------------------------------------------*/

		case WDT_OPEN_ONLY:					// set time_out only if open. No args needed
			timeout_open_only = 1;
			break;

/*-----------------------------------------------------------------------------------------------------------------------------------*/
		case WDT_ALWAYS_TIME_OUT:			// allow time_out even if not opend by any device. No args needed
			timeout_open_only = 0;
			break;

/*-----------------------------------------------------------------------------------------------------------------------------------*/
		case WDT_EXCLUSIV_OPEN_ONLY:		// set exclusiv open by one device. No args needed
			device_exclusiv_open_only = 1;	// already opend filehandles will countinue working even if more than one
			break;

/*-----------------------------------------------------------------------------------------------------------------------------------*/
		case WDT_ALWAYS_OPEN:				// allow multiple open of device. No args needed
			device_exclusiv_open_only = 0;
			break;

/*-----------------------------------------------------------------------------------------------------------------------------------*/
		case WDT_ENABLE:					// enable WDT. No args needed
			if (enabled !=1)				// was enabled before
			{
				if (!serve_all_tasks())		// updatte all Tasks (just to be sure no "quick-reset" will occur)
				{
					enabled = 1;			// switch on WDT supervisor
				}
				else
				{
					enabled = 0;			// failure during task update I will not swtich on WDT
				 	return -EFAULT;
				}
			}
			break;

/*-----------------------------------------------------------------------------------------------------------------------------------*/
		case WDT_DISABLE:					// disable WDT completely. No args needed
			enabled = 0;
			break;

/*-----------------------------------------------------------------------------------------------------------------------------------*/
		case WDT_GETSUPPORT:				// put identitiy to user
			if(copy_to_user((struct watchdog_info *)arg, &ident, sizeof(ident)))
				return -EFAULT;
			return 0;
/*-----------------------------------------------------------------------------------------------------------------------------------*/

		case WDT_GET_STATUS:				// show current status of WDT
			return put_user(0,(int *)arg);

/*-----------------------------------------------------------------------------------------------------------------------------------*/
		case IOCTL_HARDRESET:
            do_hard_reset = 1;
			debugk ("Reset through IOCTL command... \n");
			/* Zaehler auf 1 zurücksetzen, um bei Problemen das Entladen
			 * zu ermöglichen. Wir verwenden hier 1 und nicht 0, weil die
			 * Geraetedatei im aufrufenden Prozess noch geschlossen werden mu?
			 */
			break;

		default:					// catch all unknown ioctl-commands
			return -ENOIOCTLCMD;			// No IOCTL Command

	}
	debugk ("Leaving %s \n",__FUNCTION__);

	return 0;
}

//---------------------------------------------------------------------------------------------------------------------------

/* Watchdog timer service routine
* we will check if everything is still ok or any task or process does not respond
*/

void watchdog_fire (unsigned long ptr)
{
   	struct list_head 		*ptr3      	= (struct list_head  *) NULL;
   	wdt_registerded_task_t 	*ptr2      	= (wdt_registerded_task_t  *) NULL;
   	int 	i = 0;

#ifdef EXT_DEBUG
	debugk ("%s: jiffies=%ld \n", __FUNCTION__, jiffies);
	debugk ("%s: timer_count=%d jiffies\n", __FUNCTION__, timer_count);
#endif
	if ((timer_count == 0) && enabled) {		/* try "graceful" shutdown */

//#define ONLY_TESTING
#undef ONLY_TESTING

#ifdef ONLY_TESTING
#warning WDT IS CURRENTLY NOT ACTIVATED (ONLY_TESTING is defined)
		printk(KERN_CRIT "SOFTDOG: Would Reboot now ... (RESET), but we won't yet !!! .\n");
		timer_count = TIMEOUT_VALUE * HZ;  // Would have called Restart next but we want to continue another timeout period
#else
		printk (KERN_CRIT "SOFTDOG: Initiating system reboot.\n");
		kernel_restart(NULL);
		printk("WATCHDOG: Reboot didn't work !?!?!?!?!?\n Whee... that should not happen ...");
#endif
	}

	if ((timer_count > 0) || (!enabled)) {


		/* execute Hardware Watchdog sequence */
#if defined(USE_HARDWARE_WDT) || defined(EXTERNAL_HARDWARE_WDT)
        if (!do_hard_reset) {
            serve_watchdog_hw();
        }
#ifdef EXT_DEBUG
		debugk ("%s: Hardware WDT serviced\n", __FUNCTION__);
#endif
#endif
		mod_timer(&watchdog_ticktock, jiffies+timer_period); 		/* ...re-activate check-timer for next check*/

		/*
		 * don't timeout if disabled
		 */
		if (!enabled) {
#ifdef EXT_DEBUG
			debugk ("%s: WDT not enabled\n", __FUNCTION__);
#endif
			return;
			}

		/*
		 * don't timeout if device isn't open
		 */
		if ((timeout_open_only) && (! device_open)) { // not opend by any device
			debugk ("%s: WDT timeout_open_only && !device_open \n", __FUNCTION__);
			return;
			}

		/* Check if registered Tasks have regular called
		 * don't reset if everthing is fine (all tasks are registered and alive)
	 	*/
		if (check_registered_tasks()== 0) {
			//debugk ("%s: WDT all registered tasks are alive and ok. !!!!\n", __FUNCTION__);
#ifdef ONLY_TESTING
			timer_count = 2 * TIMEOUT_VALUE * HZ;  // Set Time until Reset, just in case it already begann to count down
#else
			timer_count = TIMEOUT_VALUE * HZ;  // Set Time until Reset, just in case it already begann to count down
#endif
			return;
		}

		/* something went wrong !! (maybe a task is hanging)
		* decrement variable for timer-control
		*/
		if (timer_count > timer_period) {
			timer_count -= timer_period;
			debugk ("WDT: decrement counter\n");
			}
		else
			timer_count = 0;

#ifdef ONLY_TESTING
		if (  ( (timer_count <= TIMEOUT_VALUE * HZ) && (timer_count >= ((TIMEOUT_VALUE * HZ) - timer_period)))
			  || (timer_count == 0) )
#else
		if (	timer_count == 0 )
#endif
		{
			printk ("WDT: watchdog about to expire\n");	// Warning we are goint to reset ya soon

  			printk("\n*** Watchdog Timer overview: \n");

  			printk("enabled: (0=no, 1=yes)            %d (%s)\n", enabled, enabled?"enabled":"disabled");
  			printk("exclusiv open: (0=no, 1=yes)      %d (%s)\n", device_exclusiv_open_only, \
														device_exclusiv_open_only?"open exclusic":"multiple open");
  			printk("opend by No. of tasks:            %d \n", device_open);
  			printk("time out open only: (0=no, 1=yes) %d (%s)\n", timeout_open_only, timeout_open_only?"time out is opend":"will not time out if not opened");
  			printk("RESET after No. of sec:           %d \n", soft_margin);
  			printk("remaing jiffies until HW RESET    %ld \n", timer_count);
  			printk("internal trigger period:          %ld \n", timer_period);

			for (ptr3 = registered_tasks_list.next; ptr3 != &registered_tasks_list; ptr3 = ptr3->next)
     			{
				i++;
  				printk("  %d. registered task: \n", i);
				ptr2 = list_entry(ptr3, wdt_registerded_task_t, listhead);
  				printk("   # PID of task                  %d \n", ptr2->pid);
  				printk("   # Calling Period in jiffies    %ld \n", ptr2->call_period);
  				printk("   # Will expire in (jiffies)     %ld \n", (ptr2->expires - jiffies));
  				printk("   # Last jiffies                 %ld \n", ptr2->last_jiffies);
  				printk("   # Extra info (1st 20 Bytes)   %20s \n\n", ptr2->command);

			}

  			printk("currently registered tasks:       %d \n", i);

		}
	}

	return;
}


//---------------------------------------------------------------------------------------------------------------------------

/* Notifier function declared by "my_wdt_notifier" will be call directly before system restart
*/
static int my_wdt_notifier_sys(struct notifier_block *this, unsigned long code, void* unused)
{
	if (code == SYS_DOWN || code == SYS_HALT)
		printk( "\nWDT: I am going to reboot your system now ....\n");
		// call disable HW watchdog to prevent unwanted watchdog events
	return NOTIFY_DONE;
}


/* Notifier structure needed to register a notifier function within the kernel
*/
static struct notifier_block my_wdt_notifier = {
	my_wdt_notifier_sys,
	NULL,
	0
	};


//---------------------------------------------------------------------------------------------------------------------------
static int __init watchdog_init(void)
{
	int ret;

	//printk(banner, soft_margin);
	

	ret = register_reboot_notifier(&my_wdt_notifier);

	if (ret) 	{
		printk("Unable to register reboot notifier \n");
		return ret;
		}

	ret = misc_register(&softdog_miscdev);

	if (ret)	{
		printk("Unable to register miscdevice \n");
		unregister_reboot_notifier(&my_wdt_notifier);
		return ret;
		}
	/*
	 *	setup Proc_entries
	 */
	ret = wdt_proc_init();
	if (ret)	{
		printk("Unable to register proc files \n");
		return ret;
		}

#if defined(USE_HARDWARE_WDT) || defined(EXTERNAL_HARDWARE_WDT)
	/* Init HW watchdog if used */
	hardware_init();
#endif
	debugk ("%s: watchdog timer initialized - timer_count = %ld  period = %ld\n",
		__FUNCTION__, timer_count / HZ, HZ / timer_period);

	/*
	 *	Activate timer
	 */
	del_timer(&watchdog_ticktock);		 	/* Just in case a previous timer is still running: delete it */
	init_timer(&watchdog_ticktock);			/* Set up new timeout timer*/

	mod_timer(&watchdog_ticktock, jiffies+TRIGGER_PERIOD);		// start Softtimer to check watchdog status every TRIGGER_PERIOD


#if 0 /* Feng Lin, 2008-7-3 16:45:37 */
	printk ("WDT_82xx: Software Watchdog Timer version " WDT_VERSION);
	printk ("WDT_82xx: Software Watchdog Timer2 version " WDT_VERSION);

	if (enabled) {
		printk (", timeout %ld sec.\n", timer_count / HZ);
	} else {
		printk (" (disabled)\n");
	}
#endif /* #if 0 */


	return 0;
}

//---------------------------------------------------------------------------------------------------------------------------
static void watchdog_cleanup(void)
{
	debugk ("%s: cleanup WDT82xx\n", __FUNCTION__);

	wdt_cleanup_tasklist();
	del_timer (&watchdog_ticktock);

	misc_deregister(&softdog_miscdev);
	unregister_reboot_notifier(&my_wdt_notifier);

	/* Remove registered proc files */
	wdt_proc_cleanup();

#ifdef EXTERNAL_HARDWARE_WDT			// enable only if an external Watchdog Hardware will be supported
	/* lets see if we can gain enough time to reload the driver before the HW-Watchdog will catch us */
	slow_down_watchdog_hw();
#endif

	hardware_clean();

}


//---------------------------------------------------------------------------------------------------------------------------
static int  wdt_show_linked_list (void)
{
#ifdef SHOW_LINKED_LIST

     	struct list_head 	*ptr      	= (struct list_head  *) NULL;
     	wdt_registerded_task_t 	*ptr2      	= (wdt_registerded_task_t  *) NULL;
     	int 		i	= 0;

	for (ptr = registered_tasks_list.next; ptr != &registered_tasks_list; ptr = ptr->next)
     	{
		i++;
		ptr2 = list_entry(ptr, wdt_registerded_task_t, listhead);
		debugk(" Found %d entry at 0x%08lX with beginning of parent struct at 0x%08lX ... \n", i, (long) ptr,(long) ptr2);
		debugk(" ### Registered PID = %d. Registered Call periode = %ld . Extra command %s \n", ptr2->pid, ptr2->call_period, ptr2->command);

 	}
	return (i);		// Return No. of Entries registered within double linked task list
#else
	return (0);		// Return No. of Entries registered within double linked task list
#endif
}

//---------------------------------------------------------------------------------------------------------------------------
/*
* A new task must be inserted into the double linked list
*/
static int wdt_register_task (wdt_registerded_task_t *task)
{
	int i;
	wdt_registerded_task_t 	*ptr1      	= (wdt_registerded_task_t  *) NULL;
	// only during development to show what is already registered
	wdt_show_linked_list();
	if (wdt_task_already_registered(task->pid))
	{
			debugk("%s: This PID (%d) is already registered !!!!\n", __FUNCTION__, task->pid);
			return (-EEXIST);
	}
	else
	{
		ptr1 = (wdt_registerded_task_t *)kmalloc(sizeof(wdt_registerded_task_t), GFP_KERNEL);
		debugk(" Allocated mem at %08lx Size: %08lx\n", (long) ptr1, (long) sizeof(wdt_registerded_task_t));
		if (ptr1 == NULL) {
			debugk(" No Memory available to new Entry !!!! \n");
			return (-ENOMEM);
        			}
	   	INIT_LIST_HEAD(&(ptr1->listhead));				// Init List-Head pointer within these new element to itselfe
/*
* fill in registration values
*/
		ptr1->pid = task->pid;							// Pid of belonging task
		ptr1->call_period = task->call_period;			// this task will call WDT_control every (see value)
		ptr1->last_jiffies = jiffies;					// to avoid jiffies overflow problem every 497 days
		ptr1->expires = task->call_period+jiffies;		// expiration for WDT_control

		for (i=0; i<MAX_LEN; i++) // copy comment field
		    ptr1->command[i] = task->command[i];
	    debugk(" Adding Entry 1 to List \n");
		list_add(&(ptr1->listhead), &registered_tasks_list);

// only during development to show what is already registered
#ifdef EXT_DEBUG
		debugk(" Check what we have now ... \n");
		wdt_show_linked_list();
#endif

        		return (0);
	}
}

//---------------------------------------------------------------------------------------------------------------------------
/*
* A task must be removed from the double linked list
*/
static int wdt_unregister_task (pid_t taskpid)
{
   	struct list_head 		*ptr      	= (struct list_head  *) NULL;
	wdt_registerded_task_t 	*wdt_reg_ptr = NULL;
    int 					found_entry	= 0;

	debugk ("%s: entered ....\n", __FUNCTION__);

	ptr = registered_tasks_list.next;
	while ((ptr != &registered_tasks_list) && ( ptr != NULL))
     	{
		  wdt_reg_ptr = list_entry(ptr, wdt_registerded_task_t, listhead); 	 	// Get Address of the data element

		  if (wdt_reg_ptr->pid == taskpid)
		  {
			debugk(" Found entry with this PID (%d) at %08lx \n", taskpid, (long) ptr);
			found_entry = 1;						// Mark that we've found the entry
			list_del(ptr);							// Delete this entry from the list

		    debugk("Free Memory for of these Listentry at %08lx\n", (long) wdt_reg_ptr);
			if (wdt_reg_ptr != NULL) {
		    	kfree (wdt_reg_ptr);				// Free allocated Ram of this now useless entry
				/* stop the loop because we have already found what we were looking for */
		    	ptr = NULL;
				}
			else
		    	debugk(" Ups ... trying to free Memory at Address NULL !!!!!!! \n");

		  }	/* end if (wdt_reg_ptr->pid == taskpid) */
		  else {
		    /* try if the next entry of our list contains the PID we are looking for*/
		    ptr = ptr->next;
	    	debugk(" outside: ptr is now: %08lx\n", (long) ptr);
			} /* end of else tree */

	   } /* end of while loop */

	if (found_entry == 0) {
		debugk(" Sorry, but your PID %d wasn't registered in my list (You are not under WDT control)\n", taskpid);
		return (-ENXIO);
		}

	// only during development to show what is still registered
//	printk(" Check what we still have  \n");
//	wdt_show_linked_list();

	debugk ("%s: leaving ....\n", __FUNCTION__);
   	return (0);
}

//---------------------------------------------------------------------------------------------------------------------------
/*
* Check if a task is already registered within the double linked list
*/

static int wdt_task_already_registered (pid_t taskpid)
{
     	struct list_head 	*ptr      	= (struct list_head  *) NULL;

	for (ptr = registered_tasks_list.next; ptr != &registered_tasks_list; ptr = ptr->next)
     	{
		  if (list_entry(ptr, wdt_registerded_task_t, listhead)->pid == taskpid)
		  {
			debugk("%s: Found entry with this PID: %d \n", __FUNCTION__, taskpid);
			return (-EEXIST);				// File (PID) exists
		  }
	}
        	return (0);							// ok, not existing (registered)
}

//---------------------------------------------------------------------------------------------------------------------------
/*
* A task is sending a "I am alive" message
*
* Entry within the double linked list will be updated
*/
static int wdt_serve_task (pid_t taskpid)
{
     	struct list_head 	*ptr      	= (struct list_head  *) NULL;
//     	wdt_registerded_task_t 	*ptr2      	= (wdt_registerded_task_t  *) NULL;
     	int 		found_entry	= 0;

	for (ptr = registered_tasks_list.next; ptr != &registered_tasks_list; ptr = ptr->next)
     	{
		  if (list_entry(ptr, wdt_registerded_task_t, listhead)->pid == taskpid)
		  {
			debugk(" Found entry with this PID \n");
			found_entry = 1;				// Mark that we've found the entry
			list_entry(ptr, wdt_registerded_task_t, listhead)->expires = \
				list_entry(ptr, wdt_registerded_task_t, listhead)->call_period + jiffies;
			list_entry(ptr, wdt_registerded_task_t, listhead)->last_jiffies = jiffies;
		  }
	}
	if (found_entry == 0) {
		debugk(" Sorry, but your PID %d wasn't registered in my list\n", taskpid);
		return (-ENXIO);
		}

	// only during development to show what is still registered
//	printk(" Check what we still have  \n");
//	wdt_show_linked_list();

        	return (0);
}

//---------------------------------------------------------------------------------------------------------------------------
/*
* Check if every registered task has called before timeout  (has send a "I am alive" message)
*
*/

static int check_registered_tasks(void)
{
     	struct list_head 	*ptr      	= (struct list_head  *) NULL;
	unsigned long	comp_jiffies;

    //debugk ("%s: check if the registered tasks are timed out\n", __FUNCTION__);
	comp_jiffies = jiffies;					// compare every task with the same jiffy value

	for (ptr = registered_tasks_list.next; ptr != &registered_tasks_list; ptr = ptr->next)
     	{
			//debugk(" Found entry with this PID %d \n",list_entry(ptr, wdt_registerded_task_t, listhead)->pid );
			if (list_entry(ptr, wdt_registerded_task_t, listhead)->expires <= comp_jiffies)
				{
				   // This seems to be not ok, lets see if there has been an jiffies overflow !!!
				if (list_entry(ptr, wdt_registerded_task_t, listhead)->last_jiffies >= comp_jiffies)
					{
					// We've had an overflow
					debugk ("%s: jiffies overflow detected !!!\n", __FUNCTION__);
					serve_all_tasks();		// Wait for a real timeout
				        	return (0);
					}
				debugk ("%s: Found a task which has not called before timeout (PID: %d)\n", __FUNCTION__, list_entry(ptr, wdt_registerded_task_t, listhead)->pid );
				return (-EFAULT);		// Return imediatly
				}
	}
        	return (0);

}


//---------------------------------------------------------------------------------------------------------------------------
/*
* Send every registered task an "I am alive" message
*
* Entry within the double linked list will be updated
*/

static int serve_all_tasks (void)
{
     	struct list_head 	*ptr      	= (struct list_head  *) NULL;

	for (ptr = registered_tasks_list.next; ptr != &registered_tasks_list; ptr = ptr->next)
     	{
			debugk(" Found entry with this PID %d \n",list_entry(ptr, wdt_registerded_task_t, listhead)->pid );
			list_entry(ptr, wdt_registerded_task_t, listhead)->expires = \
			list_entry(ptr, wdt_registerded_task_t, listhead)->call_period + jiffies;
			list_entry(ptr, wdt_registerded_task_t, listhead)->last_jiffies = jiffies;
	}
        	return (0);
}
//---------------------------------------------------------------------------------------------------------------------------
/*
* Cleanup Taskentries: remove all registered tasks form the double linked list and free allocated memory
*/
static int wdt_cleanup_tasklist (void)
{
   	struct list_head 		*ptr      	= (struct list_head  *) NULL;
   	struct list_head 		*tmp_ptr   	= (struct list_head  *) NULL;
   	wdt_registerded_task_t 	*ptr2      	= (wdt_registerded_task_t  *) NULL;
   	int 					no_of_entries_cleaned	= 0;

	for (ptr = registered_tasks_list.next; ptr != &registered_tasks_list; )	// cleanup until list is empty
     	{
			debugk(" Found entry No. %d \n", ++no_of_entries_cleaned );
			tmp_ptr = ptr->next;											// remember pointer to next entry (or head)
			ptr2 = list_entry(ptr, wdt_registerded_task_t, listhead); 		// get Address of data element connected to that list_head
			list_del(ptr);													// Delete these entry from the list
		   	debugk(" Free Memory for Listentry found .. \n");
 		   	kfree (ptr2);										// Free allocated Ram of this now useless entry
			ptr = tmp_ptr;													// coninue with the next element if existing
		}
	// only during development to show what is still registered
	debugk(" We have cleand (and freed) %d entries from list and RAM \n", no_of_entries_cleaned);
    return (no_of_entries_cleaned);
}

//---------------------------------------------------------------------------------------------------------------------------


//EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("Ingo Assmus");
MODULE_DESCRIPTION("Soft Watchdog driver");
MODULE_LICENSE("GPL");

module_init(watchdog_init);

module_exit(watchdog_cleanup);

#endif /* def MODULE */

// ////////////////////////////////////////////////////////////////////////
/**@page History
 * @section watchdog_c CVS History of watchdog.c
 * <pre>
 * $Log: watchdog.c,v $
 * Revision 1.11.10.1  2007/06/26 14:10:19  sst
 * - etcc
 *
 * Revision 1.12  2006/12/22 07:16:45  sst
 * - fixed bug reset bug in (when compiled in DEBUG mode)
 * - made DEBUG mode a little bit less noisy
 *
 * Revision 1.11  2006/03/08 12:22:57  sst
 * - added feature killwdt now
 *
 * Revision 1.10  2005/09/23 16:30:59  ias
 * ONLY TESTING warning moved
 *
 * Revision 1.8  2005/09/23 13:53:56  ias
 * display WDT status before a reset will be issued
 *
 * Revision 1.7  2005/07/11 17:05:12  ias
 * GPIO Pins adapted to ETCB
 *
 * Revision 1.6  2004/10/19 15:16:35  sst
 * - fixed bug in removing module
 *
 * Revision 1.5  2004/10/08 16:41:12  ias
 * removed OPEN output
 *
 * Revision 1.4  2004/10/05 11:43:04  ias
 * added CVS Id
 *
 * Revision 1.3  2004/08/12 14:56:21  ias
 * added support for Cop-Rel-II hardware
 *
 * Revision 1.2  2004/04/08 09:46:14  ias
 * switch off debug prints
 *
 * Revision 1.1  2004/04/07 14:37:40  ias
 * initial switch driver
 *
 *
 * </pre>
 */


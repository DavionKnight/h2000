/*
 * Copyright (C) 2005,2006,2009-2011 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file   edc_linux_harness.c
*** \brief  SWIM Event Data Collector Harness for Linux
***
*** Key Functions:
***   - Locates performance monitor registers
***   - Provides hooks for EDC primitives (init, open, read, write, cleanup)
**/

/* ???: Two general notes for this file
   1) You may want to indicate where the file and inode structs are
      included from, as well as loff_t. Just a suggestion though.
   2) AFAIK, some of the #includes below are not being used. If they
      are not going to be used in the near future, please remove them. */

#include <linux/version.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <asm/ioctl.h>

#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/of.h>

#include <asm/io.h>        /* For ioremap */
#include <asm/reg_booke.h> /* For register definitions */

#define MPC8548

#include "perfmon.h"
#include "edc.h"

/*! \name Linux Module Definition Functions */
/*@{*/
MODULE_AUTHOR("Freescale Semiconductor, Inc");
MODULE_DESCRIPTION("SWIM Event Data Collector (EDC)");
MODULE_LICENSE("Dual BSD/GPL");
/* CLARIFY: If the commented out line below is no longer needed, remove it. */
/* MODULE_PARAM(arch, "s"); */
/*@}*/


/******************************************************************************
 Function Declarations
******************************************************************************/

/** \brief  Used to load the SWIM_EDC module
*** \return Zero on success, non-zero on failure
**/
int32_t __init edc_linux_init(void);

/** \brief  Method for opening the Linux device.
*** \arg    edc_inode FILLME
*** \arg    edc_file  FILLME
*** \return Zero for all cases.
**/
static int32_t edc_linux_open(struct inode *edc_inode, struct file *edc_file);

/** \brief  Releases and closes the Linux device.
*** \arg    edc_inode FILLME
*** \arg    edc_file  FILLME
*** \return Zero for all cases.
**/
static int32_t edc_linux_release(struct inode *edc_inode,
	struct file *edc_file);

/* ???: Original comment refers to a data buffer that I would have renamed to
   G_edc_linux. This buffer is nowhere to be found, so please define it or
   update the \brief tag below. */
/** \brief  CLARRIFY: Used to read out the G_edc_linux data buffer
*** \arg    edc_file FILLME
*** \arg    buf      A buffer in user space for duplicating the kernel domain
*** data buffer.
*** \arg    bsize    The array size of buf
*** \arg    ppos     FILLME
*** \return FILLME
*** \todo   It might be a good idea to check PVR and verify that the total
*** PMC counts are correct.
**/
static int32_t edc_linux_read(struct file *edc_file, char* buf, size_t bsize,
	loff_t *ppos);

/* CLARIFY: The original comment for this function was difficult to understand.
   I have entered what I thought it all meant, but this entire documentation
   block needs to be double-checked for errors and clarrification. */
/* CLARIFY: State where this function writes to. */
/** \brief  Writes performance monitoring mode control registers.
*** \arg    edc_file FILLME
*** \arg    buf      Points to collections of PMC events in size char.
*** \arg    count    Size of buf, which is the total PMC number
*** \arg    ppos     FILLME
*** \return Zero in all cases
*** \todo   This function is not yet implemented
*** \todo   It might be a good idea to check PVR and verify that the total
*** PMC counts are correct.
***
*** CLARRIFY: Data is passed from the application/user code. The user code
*** identifies CPU infor to determine how many PMC counters are available
*** in the CPU.
**/
static int32_t edc_linux_write(struct file *edc_file, const char* buf,
	size_t count, loff_t *ppos);

/* ???: The argument "arg" needs to be renamed to something less ambiguous. */
/* ???: The function stated in the \todo tag is not defined... */
/** \brief  FILLME
*** \arg    edc_inode FILLME
*** \arg    edc_file  FILLME
*** \arg    cmd       The command for the function to perform
*** \arg    arg       FILLME
*** \return EDC_NO_ERROR on success, otherwise a defined EDC_ERROR_* code
*** \todo Parse the command and pass it to edc_command_handler as well.
***
*** The following macros are valid for the cmd argument:
***   -# IOCTL_EDC_START_ALL
***   -# IOCTL_EDC_STOP_ALL
***   -# IOCTL_EDC_RESET_ALL
***   -# IOCTL_EDC_CONFIG_ALL
***   -# IOCTL_EDC_GETPMONREG
***   -# IOCTL_EDC_CONFIG (not yet implemented)
**/
/* NOTE:  We need to use unsigned long here for the last arg to avoid a
   compiler warning, because it is typedef'd in /usr/include/linux/fs.h to
   be a long. */
static int32_t edc_linux_ioctl(struct inode *edc_inode, struct file *edc_file,
	uint32_t cmd, unsigned long arg);

/** \brief Used to cleanup the SWIM_EDC module
*** \todo  For porting, clean up the memory mapped I/O register address.
**/
void __exit edc_linux_cleanup(void);


/******************************************************************************
 Global Variables
******************************************************************************/

/*! \brief Linux Device Name */
static char *G_device_name = "SWIM_EDC_Linux";

/*! \note Defined in edc.c */
extern uint32_t *G_pmon_regs;

/*! \note Defined in edc.c */
extern char *G_rmon_base;

/*! \note Defined in edc.c */
extern uint32_t *G_etsec1_rmon_ctrl_reg;
extern uint32_t *G_etsec2_rmon_ctrl_reg;
extern uint32_t *G_etsec3_rmon_ctrl_reg;
extern uint32_t *G_etsec4_rmon_ctrl_reg;

/*! \brief FILLME */
static int32_t G_major;

/** \name  Memory Scratchpad
*** \brief Temporary memory for data read and write operations.
**/
/*@{*/
static uint32_t G_read_buffer[TOTAL_CNT_REGS];
static uint32_t G_write_buffer[TOTAL_CTL_REGS + 2*TOTAL_RMON];
/*@}*/

/** \name Module standard operation registoration
*** \brief CLARIFY: The ordinary device operations
***  This structure is decleared in include/linux/fs.h.
***  Following members of this struct are defined,
***   -# owner   = Module owner
***   -# read    = File read operation
***   -# write   = File write operation
***   -# open    = File open operation
***   -# release = File release operation
***   -# ioctl   = File ioctl operation
**/
static struct file_operations G_edc_linux_fileops = {
	.owner   = THIS_MODULE,
	.read    = edc_linux_read,
	.write   = edc_linux_write,
	.open    = edc_linux_open,
	.release = edc_linux_release,
	.ioctl   = edc_linux_ioctl,
};


/******************************************************************************
 Function Definitions
******************************************************************************/

int num_etsec = 0;
extern phys_addr_t get_immrbase(void);
int32_t __init edc_linux_init(void)
{
	struct device_node *np;
	DEBUG_PRINT("SWIM EDC: Function = %s, Line = %i\n",
		__func__, __LINE__);

	/* CLARIFY: Explain what this is, or remove if no longer needed */
	/* mode_t mode =0; */

	printk(KERN_INFO "SWIM EDC: loaded the SWIM_EDC module\n");
	G_major = register_chrdev(0, G_device_name, &G_edc_linux_fileops);
	if (G_major  < 0) {
		printk(KERN_ERR "SWIM EDC: unable to register character device\n");
		return -EIO;
	}
	printk(KERN_INFO "SWIM EDC: major num is %d\n", G_major);

	for_each_compatible_node(np, NULL, "fsl,etsec2") {
		num_etsec++;
	}

	/* Set the performance monitor register base address */
	/* This maps 256 bytes worth of IO memory, offset at 0xe1000 */
	G_pmon_regs = (uint32_t *) ioremap(((uint32_t)get_immrbase() + 0xe1000),
		0x100);
	printk(KERN_INFO "SWIM EDC: get_immrbase()=%x. PerfMon base address= %x\n",
		(uint32_t)get_immrbase(), (uint32_t)G_pmon_regs);

	/* Set TSEC RMON registers base address */
	/* Each TSEC block has its additional offset from the base */
	G_rmon_base = (char *) ioremap(((uint32_t)get_immrbase() + 0x24000), 0x4000);
	printk(KERN_INFO "SWIM EDC: get_immrbase()=%x. eTSEC RMON base address= %x\n",
		(uint32_t)get_immrbase(), (uint32_t)G_rmon_base);

	/* Set TSEC RMON control registers base addresses for each TSEC block */
	if (num_etsec > 0) {
		G_etsec1_rmon_ctrl_reg =
			(uint32_t *) ioremap(((uint32_t)get_immrbase() + 0x24020), 0x4);
		printk(KERN_INFO "SWIM EDC: get_immrbase()=%x. TSEC1 RMON ctrl address= %x\n",
			(uint32_t)get_immrbase(), (uint32_t)G_etsec1_rmon_ctrl_reg);
	}
	if (num_etsec > 1) {
		G_etsec2_rmon_ctrl_reg =
			(uint32_t *) ioremap(((uint32_t)get_immrbase() + 0x25020), 0x4);
		printk(KERN_INFO "SWIM EDC: get_immrbase()=%x. TSEC2 RMON ctrl address= %x\n",
			(uint32_t)get_immrbase(), (uint32_t)G_etsec2_rmon_ctrl_reg);
	}
	if (num_etsec > 2) {
		G_etsec3_rmon_ctrl_reg =
			(uint32_t *) ioremap(((uint32_t)get_immrbase() + 0x26020), 0x4);
		printk(KERN_INFO "SWIM EDC: get_immrbase()=%x. TSEC3 RMON ctrl address= %x\n",
			(uint32_t)get_immrbase(), (uint32_t)G_etsec3_rmon_ctrl_reg);
	}
	if (num_etsec > 3) {
		G_etsec4_rmon_ctrl_reg =
			(uint32_t *) ioremap(((uint32_t)get_immrbase() + 0x27020), 0x4);
		printk(KERN_INFO "SWIM EDC: get_immrbase()=%x. TSEC4 RMON ctrl address= %x\n",
			(uint32_t)get_immrbase(), (uint32_t)G_etsec4_rmon_ctrl_reg);
	}

	return 0;
}


/* ???: This function doesn't appear to do anything but print a debug
   message. What is the point otherwise? */
/* ???: The function arguments do not appear to be used. */
/* ???: This function always returns zero. If this function is required
   (by the Linux kernel) to return a value, add a comment stating so.
   Otherwise if there is no projected need for more than one return value,
   the function should be changed to return void. */
int32_t edc_linux_open(struct inode *edc_inode, struct file *edc_file)
{
	DEBUG_PRINT("SWIM EDC: Function = %s, Line = %i:\n",
		__func__, __LINE__);

	/* CMME: Or remove this next line if it is no longer needed */
	/* MOD_INC_USE_COUNT; */

	return 0;
}


/* ???: Same comments apply for this function that applied for *_open.
   Why did the original comment state that this function releases the device
   yet the code does not appear to do so? */
int32_t edc_linux_release(struct inode *edc_inode, struct file *edc_file)
{
	DEBUG_PRINT("SWIM EDC: Function = %s, Line = %i:\n",
		__func__, __LINE__);

	/* CMME: Or remove this next line if it is no longer needed */
	/* MOD_DEC_USE_COUNT; */

	return 0;
}


/* ???: The edc_file argument is not used. Remove it if it is not needed */
/* ???: The ppos argument is not used. Remove it if it is not needed */
static int32_t edc_linux_read(struct file *edc_file, char* bsize,
	size_t count, loff_t *ppos)
{
	uint32_t max_length = 0;

	/* Retrieve register/iomem data to G_read_buffer */
	max_length = edc_read(count, G_read_buffer);

	DEBUG_PRINT("SWIM EDC: Function = %s, Line = %i\n",
		__func__, __LINE__);
	DEBUG_PRINT("count: %u, max_length:%u\n", (uint32_t)count, max_length);

	/* Dump in text. Always copy up to min(count, max_length) */
	max_length = min(max_length, (uint32_t)count);

	/* Copy max_length worth bytes of data to the user space */
	return copy_to_user(bsize, G_read_buffer, max_length);
}


static int32_t edc_linux_write(struct file *file, const char* buf,
	size_t count, loff_t *ppos)
{
	/* To be implemented */
	return 0;
}


/* Although edc_inode and edc_file are never used, they are
   required to match the prototype that Linux uses for file
   systems. */
/* NOTE:  We need to use unsigned long here for the last arg to avoid a
   compiler warning, because it is typedef'd in /usr/include/linux/fs.h to
   be a long. */
static int32_t edc_linux_ioctl(struct inode *edc_inode, struct file *edc_file,
	uint32_t cmd, unsigned long arg)
{
	uint32_t *config; /* CLARIFY: assuming data is passed uint32_t */
	uint32_t config_size;
	DEBUG_PRINT("SWIM EDC: Function = %s, Line = %i\n", __func__, __LINE__);

	switch (cmd) {
	case IOCTL_EDC_START_ALL:
		return edc_start(EDC_START_ALL);

	case IOCTL_EDC_STOP_ALL:
		return edc_stop(EDC_STOP_ALL);

	case IOCTL_EDC_IS_COUNTING:
		return edc_is_counting();

	case IOCTL_EDC_RESET_ALL:
		return edc_reset(EDC_RESET_ALL);

	case IOCTL_EDC_CONFIG_ALL:
		/* arg points to the buffer with configuration values for
		 * the registers */
		config = (uint32_t *)arg;
		DEBUG_PRINT("SWIM EDC: Function = %s, Line = %i\n",
			__func__, __LINE__);
		/* config_all assumes all registers will be configured. */
		config_size = edc_reg_count();
		/* transfer configuraiton data to the kernel space */
		copy_from_user(G_write_buffer, config,
				config_size * sizeof(uint32_t));

		/* Configure registers */
		/* Config all assumes G_write_buffer has all registers */
		return edc_config(EDC_CONFIG_SET_ALL, G_write_buffer);

	case IOCTL_EDC_GETPMONREG:
		/* ???: is this needed at all? */
		*((uint32_t *)arg) = (uint32_t)edc_get_pmon_base_addr();
		return EDC_NO_ERROR;

	case IOCTL_EDC_CONFIG:
		/* Not implemented yet */
		/* This is custom control */
		return EDC_ERROR_UNKNOWN_CMD;

	default:
		return EDC_ERROR_UNKNOWN_CMD;
	}
}


void __exit edc_linux_cleanup(void)
{
	/* Free IO memory map if it is mapped. */
	if (G_pmon_regs != NULL) {
		printk(KERN_INFO "SWIM EDC: unmap perfmon registers\n");
		iounmap(G_pmon_regs);
		G_pmon_regs = NULL;
		printk(KERN_INFO "SWIM EDC: unmap rmon registers\n");
		iounmap(G_rmon_base);
		G_rmon_base = NULL;
		if (G_etsec1_rmon_ctrl_reg) iounmap(G_etsec1_rmon_ctrl_reg);
		G_etsec1_rmon_ctrl_reg = NULL;
		if (G_etsec2_rmon_ctrl_reg) iounmap(G_etsec2_rmon_ctrl_reg);
		G_etsec2_rmon_ctrl_reg = NULL;
		if (G_etsec3_rmon_ctrl_reg) iounmap(G_etsec3_rmon_ctrl_reg);
		G_etsec3_rmon_ctrl_reg = NULL;
		if (G_etsec4_rmon_ctrl_reg) iounmap(G_etsec4_rmon_ctrl_reg);
		G_etsec4_rmon_ctrl_reg = NULL;
	}

	/* Unregister the device */
	unregister_chrdev(G_major, G_device_name);
	printk(KERN_INFO "SWIM EDC: removed the SWIM_EDC module\n");
	return;
}

/* Linux kernel module initialization and cleanup entry points */
module_init(edc_linux_init);
module_exit(edc_linux_cleanup);

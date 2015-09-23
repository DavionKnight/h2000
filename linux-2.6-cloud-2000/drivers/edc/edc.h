/*
 * Copyright (C) 2005,2006,2009,2010 Freescale Semiconductor, Inc.
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

/** \file   edc.h
*** \brief  Event Data Collector definitions and function declarations.
***
*** Event Data Collector Core Components:
***   - open
***   - read
***   - write
***   - close
***   - ioctl
**/

#ifndef __EDC_H__
#define __EDC_H__

#include <linux/types.h>

/** \name  IOCTL Number Definitions
*** \brief FILLME
*** \todo  Should we comply with Linux IOCTL ID numbering?
**/
/*@{*/
#define IOCTL_EDC_START_ALL  0x00000001
#define IOCTL_EDC_STOP_ALL   0x00000002
#define IOCTL_EDC_RESET_ALL  0x00000003
#define IOCTL_EDC_CONFIG_ALL 0x00000004
#define IOCTL_EDC_GETPMONREG 0x00000005
/* CMME: State how this macro is different from EDC_CONFIG_ALL */
#define IOCTL_EDC_CONFIG     0x00000006
/* Query whether any counters are incrementable */
#define IOCTL_EDC_IS_COUNTING 0x00000007
/*@}*/

/** \name  IOCTL Commands
*** \brief Commands used in ioctl() and various EDC functions.
**/
/*@{*/
#define EDC_START_CORE 0x0001
#define EDC_START_SYS  0x0002
#define EDC_START_RMON 0x0003
#define EDC_START_ALL  (EDC_START_CORE | EDC_START_SYS | EDC_START_RMON)
#define EDC_STOP_CORE  0x0001
#define EDC_STOP_SYS   0x0002
#define EDC_STOP_RMON  0x0003
#define EDC_STOP_ALL   (EDC_STOP_CORE | EDC_STOP_SYS | EDC_STOP_RMON)
#define EDC_RESET_CORE 0x0001
#define EDC_RESET_SYS  0x0002
#define EDC_RESET_RMON 0x0003
#define EDC_RESET_ALL  (EDC_RESET_CORE | EDC_RESET_SYS | EDC_RESET_RMON)
/*@}*/

/*! \brief EDC Configuration commands */
/*@{*/
#define EDC_CONFIG_SET_ALL    0x0001
#define EDC_CONFIG_SET_CORE   0x0002
#define EDC_CONFIG_SET_SYS    0x0004
#define EDC_CONFIG_SET_RMON   0x0008
/*@}*/

/* FIX: Added the following error codes to be shared by all EDC functions.
   Please make sure these values are appropriate for the code in
   edc_linux_harness.c */
/*! \brief EDC Error Codes */
/*@{*/
#define EDC_NO_ERROR             0
#define EDC_ERROR_UNKNOWN_ARCH  -1
#define EDC_ERROR_UNKNOWN_CMD   -2
/*@}*/

/** \brief Prints out debugging information
*** \arg   fmt Same as the first argument to printf()
*** \arg   args... Same as the following arguments to printf()
***
*** This function will print using the Linux kernel printk command if the
*** kernel is defined, otherwise it will print to stderr.
**/

/*#define DEBUG*/
#ifdef DEBUG
#ifdef __KERNEL__
#include <linux/kernel.h>
#define DEBUG_PRINT(fmt, args...) printk(fmt, ## args)

#else /* __KERNEL__ */
#define DEBUG_PRINT(fmt, args...) fprintf(stderr, fmt, ## args)
#endif /* __KERNEL__ */

#else /* DEBUG */
#define DEBUG_PRINT(fmt, args...)
#endif /* DEBUG */


/******************************************************************************
 Function Declarations
******************************************************************************/

/** \brief Returns the performance monitor register base address.
*** \return The value of G_pmon_regs
**/
uint32_t *edc_get_pmon_base_addr(void);

/** \brief  Starts specified parts of the EDC performance monitoring
*** \arg    cmd A command indicating what type of performance monitoring
*** to start
*** \return 0 in all cases
*** \todo Needs to be split into a core and system start method. For now,
*** both just start with the same cmd from upper method.
***
*** Valid argument values for this function include:
***   -# EDC_START_CORE
***   -# EDC_START_SYS
***   -# EDC_START_RMON
***   -# EDC_START_ALL
**/
/* ???: I disagree with the above todo tag. I think the current implementation
   is fine. Please remove it if you agree, otherwise explain why it is
   necessary to do so. */
int32_t edc_start(uint32_t cmd);

/** \brief  Stops specified parts of the EDC performance monitoring
*** \arg    cmd A command value indicating what type of performance monitoring
*** to stop
*** \return 0 on success, non-zero if there was an error.
***
*** Valid argument values for this function include:
***   -# EDC_STOP_CORE
***   -# EDC_STOP_SYS
***   -# EDC_STOP_RMON
***   -# EDC_STOP_ALL
**/
int32_t edc_stop(uint32_t cmd);

/** \brief  Query whether any counters are incrementable, i.e. unfrozen
*** \return Whether any counters are not frozen
**/
int32_t edc_is_counting(void);

/** \brief  Resets specified parts of the EDC performance monitoring
*** \arg    cmd A command value indicating what type of performance monitoring
*** to stop
*** \return 0 on success, non-zero if there was an error.
***
*** Valid argument values for this function include:
***   -# EDC_RESET_CORE
***   -# EDC_RESET_SYS
***   -# EDC_RESET_RMON
***   -# EDC_RESET_ALL
**/
int32_t edc_reset(uint32_t cmd);

/** \brief  Configures the EDC performance monitoring
*** \arg    cmd The configuration pattern for EDC
*** \arg    write_buffer The commmand buffer to write to.
*** \return Zero for success, or non-zero if an error occurs
*** \todo   Add support for more configuration patterns
***
*** Current configuration patterns available for this function:
***   -# EDC_CONFIG_SET_ALL
***   -# EDC_CONFIG_SET_CORE
***   -# EDC_CONFIG_SET_SYS
***   -# EDC_CONFIG_SET_RMON
**/
int32_t edc_config(uint32_t cmd, uint32_t *write_buffer);

/** \brief  Configures the EDC core performance monitoring
*** \arg    write_buffer contains configuration data for all counters
*** \return Zero if success, or non-zero if an error occurs
**/
int32_t edc_config_core(uint32_t **write_buffer);

/** \brief  Configures the EDC system performance monitoring
*** \arg    write_buffer contains configuration data for all counters
*** \return Zero if success, or non-zero if an error occurs
**/
int32_t edc_config_system(uint32_t **write_buffer);


/** \brief  Configures RMON offset for use at data retrieval time
*** \arg    write_buffer The commmand buffer to write to
*** \return Zero if success, or non-zero if an error occurs
***
*** fills in the array of offsets to RMON counters used during data retreival
***/
int32_t edc_config_rmon(uint32_t **write_buffer);

/** \brief  FILLME
*** \arg    count FILLME
*** \arg    read_buffer FILLME
*** \return TOTAL_CNT_REGS * sizeof(int32_t), or 0 on an error
**/
int32_t edc_read(uint32_t count, uint32_t *read_buffer);

/** \brief  FILLME
*** \arg    count FILLME
*** \arg    read_buffer FILLME
*** \return The value of TOTAL_CTL_REGS, or 0 if the architecture is not
*** defined.
**/
int32_t edc_read_config(uint32_t count, uint32_t *read_buffer);

/* ???: Is the \todo comment correct? I copy/pasted it, but there is a conflict
   between this function talking about register size and number of registers */
/** \brief  Access to the number of control registers on the architecture
*** \return The numbers of control registers
*** \todo   Return correct register size according to the architecture.
**/
/* ???: This function originally was called "*reg_size". This is misleading,
   because from what I understand from the comments in the code it returns
   not a register size but a number of available registers. I have changed
   the function name to reflect its true purpose. */
/* ???: Is this function even necessary? All it does is return TOTAL_CTL_REGS
   and print out a debug message, so why not just use the macro and remove
   the function altogether? Please either remove this function, or argue why
   we need it. */
int32_t edc_reg_count(void);

/** \brief Gets the processor version number from the PVR register
*** \return The 32-bit version number
**/
uint32_t edc_get_pvr(void);

/** \brief Gets the system version number from the SVR register
*** \return The 32-bit system number
**/
uint32_t edc_get_svr(void);

#endif /* __EDC_H__ */

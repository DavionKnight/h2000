/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains an implementation of log functions.
 */

#include <asm/tlu_osdef.h>
#include <asm/tlu_log.h>

uint32_t tlu_log_level;

/*******************************************************************************
 * Description:
 *   This function prints a memory buffer. Output format is as the following:
 *	  <prefix> <address> <32-bit data 0> ... <32-bit data 7>
 *	  <prefix> <address> <32-bit data 0> ... <32-bit data 7>
 *	  ......
 * Parameters:
 *   buf	- A pointer to the memory to be printed.
 *   len	- Total number of bytes to be printed.
 *   print_addr - The address value which is to be printed ahead of each line
 *                if it is not NULL. Otherwise ignored.
 *   prefix     - A pointer to a string which will be printed a head of each
 *                line. Ignored if it is NULL.
 * Return:
 *   None
 ******************************************************************************/
void _tlu_print_memory(const void *buf, int len, const void *print_addr,
		const char *prefix)
{
	int i;

	for (i = 0; i < len / 4; i++) {
		if (!(i & 7)) {
			if (prefix != NULL)
				printk(KERN_INFO "%s ", prefix);

			if (print_addr != NULL)
				printk(KERN_INFO "%p: ",
						&((uint32_t *)print_addr)[i]);

		}
		printk(KERN_INFO "%08x ", ((uint32_t *)buf)[i]);
		if ((i & 7) == 7)
			printk(KERN_INFO "\n");

	}
	for (i = len & (~3); i < len; i++) {
		if (!(i & 7)) {
			if (prefix != NULL)
				printk(KERN_INFO "%s ", prefix);

			if (print_addr != NULL)
				printk(KERN_INFO "%p: ",
						&((uint8_t *)print_addr)[i]);

		}
		printk(KERN_INFO "%02x", ((uint8_t *)buf)[i]);
	}
	if ((i & 31) != 0)
		printk(KERN_INFO "\n");

}

/*******************************************************************************
 * Description:
 *   This this function logs the given information. The format and variable
 *   variable parameters have exact same definitions as standard printf.
 *   File name, line number and function name will be logged if TLU_LOG_FILE is
 *   is enabled in the log configuration or TLU_LOG_CRIT or TLU_LOG_WARN is set
 *   in the parameter 'level'.
 * Parameters:
 *   level  - Log level of this information.
 *   prefix - A pointer to a string which will be printed a head of the message
 *	      specified by the succeeding parameters. Ignored if it is NULL.
 *   file   - File name
 *   line   - Line number
 *   func   - Function name
 *   format - Standard printf format.
 *   arg    - A list of args with type va_list.
 * Return:
 *   None
 ******************************************************************************/
void _tlu_log(uint32_t level, const char *prefix, const char *file, int line,
		const char *func, const char *format, va_list arg)
{
	if (prefix != NULL)
		printk(KERN_INFO "%s ", prefix);

	if ((tlu_log_level & TLU_LOG_FILE)
			|| (level & (TLU_LOG_CRIT | TLU_LOG_WARN))) {
		if (level & (TLU_LOG_CRIT | TLU_LOG_WARN)) {
			if (level & TLU_LOG_WARN)
				printk(KERN_INFO "WARN ");
			else if (level & TLU_LOG_CRIT)
				printk(KERN_INFO "CRIT ");
			printk(KERN_INFO "%s:%d:%s> ", file, line, func);
		} else
			printk(KERN_INFO "%s:%d> ", file, line);
	}
	vprintk(format, arg);
}

/*******************************************************************************
 * Description:
 *   This this function logs the given information. The format and variable
 *   variable parameters have exact same definitions as standard printf.
 *   File name, line number and function name will be logged if TLU_LOG_FILE is
 *   is enabled in the log configuration or TLU_LOG_CRIT or TLU_LOG_WARN is set
 *   in the parameter 'level'.
 * Parameters:
 *   level  - Log level of this information.
 *   prefix - A pointer to a string which will be printed a head of the message
 *	      specified by the succeeding parameters. Ignored if it is NULL.
 *   file   - File name
 *   line   - Line number
 *   func   - Function name
 *   format - Standard printf format.
 * Return:
 *   None
 ******************************************************************************/
void tlu_log(uint32_t level, const char *prefix, const char *file, int line,
		const char *func, const char *format, ...)
{
	va_list arg;

	if ((tlu_log_level | TLU_LOG_CRIT) & level) {
		va_start(arg, format);

		_tlu_log(level, prefix, file, line, func, format, arg);

		va_end(arg);
	}
}

/*******************************************************************************
 * Description:
 *   This this function logs a memory buffer with a given prefix and message if
 *   any.
 * Parameters:
 *   buf	- A pointer to the memory to be printed.
 *   len	- Total number of bytes to be printed.
 *   print_addr - The address value which is to be printed ahead of each line if
 *		  it is not NULL. Otherwise ignored.
 *   Others - please refer to those in tlu_log.
 * Return:
 *   None
 ******************************************************************************/
void tlu_log_mem(uint32_t level, const void *buf, int len, const char *prefix,
		const void *print_addr, const char *file, int line,
		const char *func, const char *format, ...)
{
	va_list arg;

	if ((tlu_log_level | TLU_LOG_CRIT) & level) {
		va_start(arg, format);

		if (format != NULL) {
			_tlu_log(level, prefix, file, line, func, format, arg);
			/* Do not print prefix any more */
			prefix = NULL;
		}

		_tlu_print_memory(buf, len, print_addr, prefix);

		va_end(arg);
	}
}

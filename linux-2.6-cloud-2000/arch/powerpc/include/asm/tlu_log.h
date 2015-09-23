/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains definitions and macros of TLU log module. It supports
 * run-time log level control.
 *
 * This file is part of the Linux kernel
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef TLU_LOG_H
#define TLU_LOG_H

#include <asm/tlu_osdef.h>

#define TLU_DEBUG

#define TLU_LOG_CRIT  0x00000001
#define TLU_LOG_WARN  0x00000002
#define TLU_LOG_FILE  0x80000000

#ifdef TLU_DEBUG
#define TLU_LOG_FILTER 0xFFFFFFFF
#else
#define TLU_LOG_FILTER (TLU_LOG_CRIT | TLU_LOG_WARN)
#endif

/*******************************************************************************
 * Description:
 *   This macro performs basic log with given format. The format and variable
 *   variable parameters have exact same definitions as standard printf.
 * Parameters:
 *   level  - Log level of this message. This function performs no-op if the
 * 	      level is disabled.
 *   prefix - A pointer to a string which will be printed a head of the
 *            message specified by the succeeding parameters. Ignored if it
 *            is NULL.
 *   format - Standard printf format.
 * Return:
 *   None
 ******************************************************************************/
#define TLU_LOG(level, prefix, format, ...) \
	if (TLU_LOG_FILTER & (level)) { \
		tlu_log(level, prefix, __FILE__, __LINE__, __func__, format, \
			##__VA_ARGS__); \
	}

/*******************************************************************************
 * Description:
 *   This macro performs memory log proceeded by a message specified by format
 *   and succeeding parameters if present. The format is as the following:
 *      <prefix> <format ....>
 *      <address> <32-bit data 0> ... <32-bit data 7>
 *      <address> <32-bit data 0> ... <32-bit data 7>
 *      ....
 * Parameters:
 *   level  - Log level this this message. This function performs no-op if the
 *   	      level is disabled.
 *   buf    - A pointer to the memory to be logged.
 *   len    - Total number of bytes to be logged
 *   prefix - A pointer to a string which will be printed a head of the message
 *	      specified by the succeeding parameters. Ignored if it is NULL.
 *   format - Standard printf format. Ignored if it is NULL.
 * Return:
 *   None
 ******************************************************************************/
#define TLU_LOG_MEM(level, buf, len, prefix, format, ...) \
	if (TLU_LOG_FILTER & (level)) { \
		tlu_log_mem(level, buf, len, prefix, buf, __FILE__, __LINE__, \
				__func__, format, ##__VA_ARGS__); \
	}

/*******************************************************************************
 * Description:
 *   Same as TLU_LOG_MEM ecxept no <address> is printed in each data line.
 ******************************************************************************/
#define TLU_LOG_DATA(level, buf, len, prefix, format, ...) \
	if (TLU_LOG_FILTER & (level)) { \
		tlu_log_mem(level, buf, len, prefix, NULL, __FILE__, __LINE__, \
				__func__, format, ##__VA_ARGS__); \
	}

extern uint32_t tlu_log_level;

void tlu_log(uint32_t level, const char *prefix, const char *file, int line,
		const char *func, const char *format, ...);
void tlu_log_mem(uint32_t level, const void *buf, int len, const char *prefix,
		const void *print_addr, const char *file, int line,
		const char *func, const char *format, ...);
void _tlu_print_memory(const void *buf, int len, const void *print_addr,
		const char *prefix);

extern inline void tlu_print_memory(void *buf, int len)
{
	_tlu_print_memory(buf, len, buf, NULL);
}

#endif

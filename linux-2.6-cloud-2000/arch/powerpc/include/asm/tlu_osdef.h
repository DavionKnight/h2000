/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains Operating System specific definitions.
 *
 * This file is part of the Linux kernel
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef TLU_OSDEF_H
#define TLU_OSDEF_H

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/io.h>
#include <asm/system.h>
#else
#include <stdint.h>
#include <string.h>
#define printk printf
#define vprintk vprintf
#define mb()   __asm__ __volatile__ ("sync" : : : "memory")
#endif

#endif

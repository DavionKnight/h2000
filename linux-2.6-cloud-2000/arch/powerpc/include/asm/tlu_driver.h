/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Zhichun Hua, zhichun.hua@freescale.com, Mon Mar 12 2007
 *
 * Description:
 * This file contains definitions of TLU driver functions.
 *
 * This file is part of the Linux kernel
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef TLU_DRIVER_H
#define TLU_DRIVER_H

struct tlu;

/*====================== Driver Public Kernel APIs ==========================*/
/*******************************************************************************
 * Description:
 *   This function get a TLU by ID from the driver.
 * Parameters:
 *   id  - TLU ID. It is 0, 1, ... or n. n is depend on the tlu configuration
 *         (tlu.conf).
 * Return:
 *   The corresponding TLU data structure is returned if success. Otherwise
 *   NULL is returned.
 ******************************************************************************/
struct tlu *tlu_get(int id);

#endif

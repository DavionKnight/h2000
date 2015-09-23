/* driver/tdm/tdm-dev.h
 *
 *  Copyright (C) 2010 Freescale Semiconductor, Inc, All rights reserved.
 *
 * tdm-dev.h - tdm-bus driver, char device interface
 *
 * Author:Hemant Agrawal <hemant@freescale.com>
 * 	Rajesh Gumasta <rajesh.gumasta@freescale.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _LINUX_TDM_DEV_H
#define _LINUX_TDM_DEV_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/compiler.h>
#endif /* __KERNEL__ */

#define NCSW_TYPE_BASE  	0xe0
#define TDM_TYPE_BASE		(NCSW_TYPE_BASE+1)
#define TDM_MAJOR_NUM		239	/* Device major number		*/
#define TDM_DEV_NAME		"tdm"

/* set slot value for this tdm */
#define IOCTL_TDM_SET_START_SLOT	IOW(TDM_TYPE_BASE,	\
						TDM_CHAN_SET_START_SLOT, int)

/* set slot width, this should be in consistent with the adapters support */
#define IOCTL_TDM_SET_SLOT_WIDTH	_IOW(TDM_TYPE_BASE,	\
						TDM_CHAN_SET_SLOT_WIDTH, int)

/* enable tdm for this channel */
#define IOCTL_TDM_ENABLE_TDM		_IOW(TDM_TYPE_BASE,	\
						TDM_CHAN_ENABLE_TDM, int)

 /* disable tdm for this channel */
#define IOCTL_TDM_DISABLE_TDM		_IOW(TDM_TYPE_BASE,	\
						TDM_CHAN_DISABLE_TDM, int)

/* set Rx Port Buffer Length */
#define IOCTL_TDM_SET_RX_LENGTH		_IOW(TDM_TYPE_BASE,	\
						TDM_CHAN_SET_RX_LENGTH, int)

#endif /* _LINUX_TDM_DEV_H */

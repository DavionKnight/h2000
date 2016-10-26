/*
*  COPYRIGHT NOTICE
*  Copyright (C) 2016 HuaHuan Electronics Corporation, Inc. All rights reserved
*
*  Author       	:Kevin_fzs
*  File Name        	:/home/kevin/works/projects/H20RN-2000/drivers/spidrv/epcs.c
*  Create Date        	:2016/10/26 14:39
*  Last Modified      	:2016/10/26 14:39
*  Description    	:
*/
#include "spdrv.h"


/*
 * Service routine to read status register until ready, or timeout occurs.
 * Returns non-zero if error.
 */
static int wait_till_ready(struct w25p *flash)
{
	int count;
	int sr;

	/* one chip guarantees max 5 msec wait here after page writes,
	 * but potentially three seconds (!) after page erase.
	 */
	for (count = 0; count < MAX_READY_WAIT_COUNT; count++) {
		if ((sr = read_sr(flash)) < 0)
			break;
		else if (!(sr & SR_WIP))
			return 0;
		msleep(10);
		/* REVISIT sometimes sleeping would be best */
	}

	return 1;
}

int epcs_erase_chip()
{

}

int epcs_erase_sector()
{
}

int epcs_read()
{
}

int epcs_write()
{
}


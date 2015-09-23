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
 * Author: Roy Pledge, Roy.Pledge@freescale.com
 *
 * Description:
 * This implements initialisation and teardown of the user-interface components
 * of the pattern-matcher driver.
 */

#include "user_private.h"

/* Setup and teardown for the module */
int pme_user_init(void)
{
	/* Initialization code here */
	int err =  pme_sg_tbl_pool_init();
	if (err) {
		printk(KERN_ERR "Unable to initialize pme_user\n");
		return err;
	}
	err =  pmscanner_init();
	if (err) {
		printk(KERN_ERR "Unable to initialize pmscanner\n");
		pme_sg_tbl_pool_destroy();
		return err;
	}
	err = database_init();
	if (err) {
		printk(KERN_ERR "Unable to initialize control\n");
		pmscanner_finish();
		pme_sg_tbl_pool_destroy();
		return err;
	}
	return 0;
}

void pme_user_exit(void)
{
	/* Cleanup code here */
	pme_sg_tbl_pool_destroy();
	pmscanner_finish();
	database_finish();
}

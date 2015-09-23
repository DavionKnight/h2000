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
 * This file exports API symbols in the Linux Operating System.
 */
#include <linux/module.h>
#include <asm/crt.h>
#include <asm/htk.h>
#include <asm/tlu.h>
#include <asm/tlu_bank.h>

/* == == == For Testing Purpose Only = == == = */
EXPORT_SYMBOL(_tlu_print_memory);
EXPORT_SYMBOL(tlu_log);
EXPORT_SYMBOL(tlu_hash_cont32);
EXPORT_SYMBOL(tlu_log_mem);
EXPORT_SYMBOL(tlu_log_level);

/* == == == Linux APIs = == == = */
/*----- TLU Access -----*/
EXPORT_SYMBOL(tlu_table_config);
EXPORT_SYMBOL(tlu_bank_config);
EXPORT_SYMBOL(tlu_read);
EXPORT_SYMBOL(tlu_write);
EXPORT_SYMBOL(tlu_write_byte);
EXPORT_SYMBOL(tlu_add);
EXPORT_SYMBOL(tlu_acchash);
EXPORT_SYMBOL(tlu_find);
EXPORT_SYMBOL(tlu_findr);
EXPORT_SYMBOL(tlu_findw);
EXPORT_SYMBOL(tlu_get_stats);
EXPORT_SYMBOL(tlu_reset_stats);
EXPORT_SYMBOL(tlu_bank_mem_alloc);
EXPORT_SYMBOL(tlu_bank_mem_free);
EXPORT_SYMBOL(tlu_acc_log);

/*----- Linux Init & Free -----*/
EXPORT_SYMBOL(tlu_init);
EXPORT_SYMBOL(tlu_free);

/*----- HTK Table -----*/
EXPORT_SYMBOL(tlu_htk_create);
EXPORT_SYMBOL(tlu_htk_free);
EXPORT_SYMBOL(tlu_htk_insert);
EXPORT_SYMBOL(tlu_htk_delete);
EXPORT_SYMBOL(tlu_htk_find);
EXPORT_SYMBOL(tlu_htk_findr);
EXPORT_SYMBOL(tlu_htk_findw);

/*----- CRT Table -----*/
EXPORT_SYMBOL(tlu_crt_create);
EXPORT_SYMBOL(tlu_crt_free);
EXPORT_SYMBOL(tlu_crt_insert);
EXPORT_SYMBOL(tlu_crt_delete);
EXPORT_SYMBOL(tlu_crt_find);
EXPORT_SYMBOL(tlu_crt_findr);
EXPORT_SYMBOL(tlu_crt_findw);

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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE US:E OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Author: Roy Pledge, Roy.Pledge@freescale.com
 *         Geoff Thorpe, Geoff.Thorpe@freescale.com
 *
 * Description:
 * This file implements contexts for the pattern-matcher driver. It is
 * 1-to-1 on top of the internal "object" type which represents the
 * resource as seen by the DMA engine, but associates it with activity
 * codes and other data-processing parameters that are used in the
 * decompression and pattern-scanning stages behind the DMA engine.
 *
 */

/*********************/
/* pme_context_object */
/*********************/

DECLARE_GLOBAL(restrict_exclusive, int, int,
		1, "Only privileged users can enter exclusive mode");
/* Returns 0 if a valid mode is specified,
 * -1 if the mode is not valid */
static inline int validate_mode(enum pme_mode mode)
{
	switch (mode) {
	case PME_MODE_CONTROL:
	case PME_MODE_DEFLATE_SCAN:
	case PME_MODE_DEFLATE_SCAN_RESIDUE:
	case PME_MODE_DEFLATE_RECOVER:
	case PME_MODE_DEFLATE_SCAN_RECOVER:
	case PME_MODE_ZLIB_SCAN:
	case PME_MODE_ZLIB_SCAN_RESIDUE:
	case PME_MODE_ZLIB_RECOVER:
	case PME_MODE_ZLIB_SCAN_RECOVER:
	case PME_MODE_GZIP_SCAN:
	case PME_MODE_GZIP_SCAN_RESIDUE:
	case PME_MODE_GZIP_RECOVER:
	case PME_MODE_GZIP_SCAN_RECOVER:
	case PME_MODE_PASSTHRU:
	case PME_MODE_PASSTHRU_SCAN:
	case PME_MODE_PASSTHRU_SCAN_RESIDUE:
	case PME_MODE_PASSTHRU_RECOVER:
	case PME_MODE_PASSTHRU_SCAN_RECOVER:
	case PME_MODE_PASSTHRU_SCAN_RESIDUE_RECOVER:
		return 0;
		break;
	default:
		return -1;
	}
}

/* Destructor called before the dma_object is freed */
static inline struct pme_context *obj2ctx(struct pme_object *obj)
{
	return (struct pme_context *)obj;
}
static inline struct pme_object *ctx2obj(struct pme_context *ctx)
{
	return (struct pme_object *)ctx;
}

/**********************************************************
 * pme_context_create:  Allocate the context structure and
 * initalize the parts
 *********************************************************/
int pme_context_create(struct pme_context **res, struct pme_channel *channel,
			 const struct pme_parameters *params,
			 u64 deflate_sid, u64 report_sid,
			 unsigned int gfp_mode, void *userctx,
			 void (*dtor)(void *userctx, enum pme_dtor reason))
{
	int ret;
	struct pme_object *ctx;
	struct pme_parameters local_params;

	ret = pme_object_alloc(&ctx, channel, gfp_mode,
				       userctx, dtor);
	if (ret)
		return ret;

	ctx->flags = 0;
	memcpy(&local_params, params, sizeof(*params));
	ctx->params.pattern_set = 0;
	ctx->params.pattern_subset = 0;
	ctx->params.session_id = 0;
	ctx->params.mode = PME_MODE_INVALID;
	ctx->params.report_verbosity = 0;
	ctx->report_sid = report_sid;
	ctx->deflate_sid = deflate_sid;
	atomic_set(&ctx->state, 0);
	if (params->flags & PME_PARAM_DONT_SLEEP)
		ctx->flags |= PME_FLAG_DONT_SLEEP;

	/* We'll always set all parameters on create */
	local_params.flags = ~0;

	ret = pme_context_update(obj2ctx(ctx), &local_params);
	if (ret) {
		ctx->dtor = NULL;
		pme_context_delete(obj2ctx(ctx));
	} else
		/* Success */
		*res = obj2ctx(ctx);
	return ret;
}
EXPORT_SYMBOL(pme_context_create);

/**********************************************************
 * pme_context_delete: Release the context and all its
 * parts
 *********************************************************/
void pme_context_delete(struct pme_context *__ctx)
{
	struct pme_object *ctx = ctx2obj(__ctx);
	/* If this context has exclusive access release the channel anyway */
	if (ctx->flags & PME_FLAG_EXCLUSIVITY)
		/* Ignoring error here.  Even if this fails, we continue
		 * with the deletion.  Otherwise, error recovery would be
		 * impossible */
		pme_context_set_exclusive_mode(obj2ctx(ctx), 0);
	/* The context will be cleaned up when the dma_obj destructor
	 * is called */
	pme_object_free(ctx);
}
EXPORT_SYMBOL(pme_context_delete);

/**************************************************
 * pme_context_update:  Manipulates the parameters
 * of a pme_context
 **************************************************/
int pme_context_update(struct pme_context *__ctx,
		const struct pme_parameters *params)
{
	int ret = -EINVAL;
	struct update_cmd_ctx *local_ctx;
	u64 residue_ptr = 0;
	int residue_change = 0;
	u32 high_val = 0, low_val = 0;
	struct pme_object *ctx = ctx2obj(__ctx);

	if (!params->flags)
		return -EINVAL;

	if (atomic_dec_return(&ctx->state) != -1) {
		atomic_inc(&ctx->state);
		return -EBUSY;
	}

	/* Process the flags, adding options as needed */
	if ((params->flags & PME_PARAM_MODE) && validate_mode(params->mode))
		goto done;
	if ((params->flags & PME_PARAM_SESSION_ID) &&
			(params->session_id >= sre_session_ctx_num))
		goto done;

	/* Calculate if residue usage is changing
	 * The check for INVALID is used to determine if
	 * this update is part of a call to _create*/
	if ((params->flags & PME_PARAM_MODE) &&
	    (pme_mode_is_residue_used(params->mode) !=
	     pme_mode_is_residue_used(ctx->params.mode) ||
	     ctx->params.mode == PME_MODE_INVALID))
		residue_change = 1;
	/* Setup memory for the key/value pairs */
	ret = update_command_init(ctx, &local_ctx);
	if (ret)
		goto done;
	if (params->flags & PME_PARAM_MODE)
		/* add the act_code to the update buffer */
		update_command_add(local_ctx, PME_UPDATE_KEY_ACTIVITY_CODE,
				     &params->mode);
	if (params->flags & PME_PARAM_SET)
		update_command_add(local_ctx, PME_UPDATE_KEY_PATTERN_SET,
				     &params->pattern_set);
	if (params->flags & PME_PARAM_SUBSET)
		update_command_add(local_ctx, PME_UDPATE_KEY_PATTERN_SUBSET,
				     &params->pattern_subset);
	if (params->flags & PME_PARAM_SESSION_ID)
		update_command_add(local_ctx, PME_UPDATE_KEY_SESSION_ID,
				     &params->session_id);
	if (residue_change || params->flags & PME_PARAM_RESET_RESIDUE)
		update_command_add(local_ctx, PME_UPDATE_KEY_RESIDUE_LEN, NULL);
	if (params->flags & PME_PARAM_RESET_SEQ_NUMBER)
		update_command_add(local_ctx, PME_UPDATE_KEY_SEQUENCE_NUMBER,
				NULL);

	/* Always update the sids */
	high_val = (u32) (ctx->report_sid >> 32);
	update_command_add(local_ctx, PME_UPDATE_KEY_REPORT_SID_HIGH,
			     &high_val);
	low_val = (u32) (ctx->report_sid & 0x00000000FFFFFFFF);
	update_command_add(local_ctx, PME_UPDATE_KEY_REPORT_SID_LOW,
			     &low_val);

	high_val = (u32) (ctx->deflate_sid >> 32);
	update_command_add(local_ctx,
			     PME_UPDATE_KEY_DEFLATE_SID_HIGH, &high_val);

	low_val = (u32) (ctx->deflate_sid & 0x00000000FFFFFFFF);
	update_command_add(local_ctx, PME_UPDATE_KEY_DEFLATE_SID_LOW,
			     &low_val);

	if (residue_change) {
		if (pme_mode_is_residue_used(params->mode))
			/* Allocate and add the residue.  Store the
			 * allocation in a local so we know to clean up
			 * if something fails */
			ret = pme_object_residue(ctx, 1,
						   &residue_ptr);
			if (ret) {
				update_command_free(local_ctx);
				goto done;
			}
		/* Add the pointer to the command */
		high_val = (u32) (residue_ptr >> 32);
		update_command_add(local_ctx,
			PME_UPDATE_KEY_RESIDUE_PTR_HIGH, &high_val);

		low_val = (u32) (residue_ptr & 0x00000000FFFFFFFF);
		update_command_add(local_ctx, PME_UPDATE_KEY_RESIDUE_PTR_LOW,
				     &low_val);
	}
	if (params->flags & PME_PARAM_END_OF_SUI_ENABLE)
		update_command_add(local_ctx, PME_UPDATE_KEY_END_OF_SUI_EVENT,
				     &params->end_of_sui_enable);

	if (params->flags & PME_PARAM_REPORT_VERBOSITY)
		update_command_add(local_ctx, PME_UPDATE_KEY_DEBUG_CTRL_CODE,
				     &params->report_verbosity);

	/* Execute the command, waiting for the result */
	ret = update_command_execute(ctx, local_ctx);
	if (ret) {
		/* Somthing went bad. We don't want to update any local values.
		 * Plus, we need to release any residue we allocated */
		if (residue_change && pme_mode_is_residue_used(params->mode))
			pme_object_residue(ctx, 0, &residue_ptr);
		/* Free local resources since no callback will occur*/
		update_command_free(local_ctx);
		goto done;
	}
	/* Release residue if no longer used */
	if (residue_change && pme_mode_is_residue_used(ctx->params.mode))
		pme_object_residue(ctx, 0, NULL);

	/* Update the cached values */
	if (params->flags & PME_PARAM_SET)
		ctx->params.pattern_set = params->pattern_set;
	if (params->flags & PME_PARAM_SUBSET)
		ctx->params.pattern_subset = params->pattern_subset;
	if (params->flags & PME_PARAM_SESSION_ID)
		ctx->params.session_id = params->session_id;
	if (params->flags & PME_PARAM_MODE)
		ctx->params.mode = params->mode;
	if (params->flags & PME_PARAM_REPORT_VERBOSITY)
		ctx->params.report_verbosity = params->report_verbosity;
	if (params->flags & PME_PARAM_END_OF_SUI_ENABLE)
		ctx->params.end_of_sui_enable = params->end_of_sui_enable;
done:
	atomic_inc(&ctx->state);
	return ret;
}
EXPORT_SYMBOL(pme_context_update);

/**********************************************************
 * pme_context_get: Gets the contexts debugging data
 * If a NULL is passed in any of the data slots,
 * that value isn't returned.
 *********************************************************/
int pme_context_get(struct pme_context *__ctx,
		    struct pme_parameters *params)
{
	struct pme_object *ctx = ctx2obj(__ctx);
	if (atomic_read(&ctx->state) < 0)
		return -EBUSY;

	if (params->flags & PME_PARAM_SET)
		params->pattern_set = ctx->params.pattern_set;
	if (params->flags & PME_PARAM_SUBSET)
		params->pattern_subset = ctx->params.pattern_subset;
	if (params->flags & PME_PARAM_SESSION_ID)
		params->session_id = ctx->params.session_id;
	if (params->flags & PME_PARAM_REPORT_VERBOSITY)
		params->report_verbosity = ctx->params.report_verbosity;
	if (params->flags & PME_PARAM_MODE)
		params->mode = ctx->params.mode;
	if (params->flags & PME_PARAM_END_OF_SUI_ENABLE)
		params->end_of_sui_enable = ctx->params.end_of_sui_enable;
	/* If the exclusive flag is set, check if this
	 * context has exclusive mode.  If it doesn't
	 * clear the flag.  The net effect is that if the caller
	 * sets the flag, then does a get, the flag will
	 * remain set if the context is exclusive and will
	 * be cleared if the context isn't exclusive */
	if (params->flags & PME_PARAM_EXCLUSIVE)
		if (!(ctx->flags & PME_FLAG_EXCLUSIVITY))
			params->flags &= ~PME_PARAM_EXCLUSIVE;
	return 0;
}
EXPORT_SYMBOL(pme_context_get);

/* Standard completion CB for commands at this layer.
 * The main goal is to invoke the users callback and
 * perform any cleanup that may be needed */
static int cmd_completion_cb(struct pme_object *dma_obj,
			     u32 pme_completion_flags,
			     u8 exception_code,
			     u64 stream_id,
			     struct pme_callback *dma_cb,
			     size_t output_used,
			     struct pme_fbchain *fb_output)
{
	struct pme_context *ctx = obj2ctx(pme_object_read(dma_obj));
	struct pme_context_callback *pme_cb =
	    (struct pme_context_callback *)&dma_cb->ctx;

	/* Invoking user callback. The only reason we can't bypass this layer
	 * is that we need to convert the pme_object to the corresponding
	 * pme_context. */
	return pme_cb->completion(ctx, pme_completion_flags,
			  exception_code, stream_id, pme_cb,
			  output_used, fb_output);
}

/* Variant for blocking operation. */
struct blocking_ctx {
	struct pme_context_callback cb;
	int refs;
};

static int blocking_cb(struct pme_object *dma_obj,
			     u32 pme_completion_flags,
			     u8 exception_code,
			     u64 stream_id,
			     struct pme_callback *dma_cb,
			     size_t output_used,
			     struct pme_fbchain *fb_output)
{
	int ret;
	struct pme_context *ctx = obj2ctx(pme_object_read(dma_obj));
	struct blocking_ctx *blocking_ctx =
		(struct blocking_ctx *)dma_cb->ctx.words[0];

	ret = blocking_ctx->cb.completion(ctx, pme_completion_flags,
			  exception_code, stream_id, &blocking_ctx->cb,
			  output_used, fb_output);
	blocking_ctx->refs--;
	/* If an abort occured, refs needs to be zero as this cmd is done*/
	if (pme_completion_flags & PME_COMPLETION_ABORTED)
		blocking_ctx->refs = 0;
	return ret;
}

/**********************************************************
 *
 * Performs an IO Command on the context
 *
 *********************************************************/
int pme_context_io_cmd(struct pme_context *__ctx, u32 flags,
		      struct pme_context_callback *cb,
		      struct pme_data *input_data,
		      struct pme_data *deflate_data,
		      struct pme_data *report_data)
{
	struct pme_object *ctx = ctx2obj(__ctx);
	u32 cmd_flags;
	int ret;
	struct pme_callback dma_cb;
	struct blocking_ctx blocking_ctx = { .refs = 0 };

	if (atomic_inc_return(&ctx->state) <= 0) {
		atomic_dec(&ctx->state);
		return -EBUSY;
	}

	/* Make sure the flags only contain valid values for
	 * PME_CONTEXT */
	cmd_flags = flags & PME_CONTEXT_VALID_FLAGS;

#ifdef IF_YOU_DONT_TRUST_THE_CALLER
	/* Validate that the correct output buffers have
	 * been specified */
	if (!(flags & PME_FLAG_NOP) && !deflate_data &&
			pme_mode_is_recovery_done(ctx->params.mode))
		panic("Recovery but no deflate data buffer specified\n");
	if (!(flags & PME_FLAG_NOP) && !report_data &&
			pme_mode_is_scan_done(ctx->params.mode))
		panic("Scan is done but no report data buffer specified\n");
	if ((flags & PME_FLAG_NOP) && (deflate_data || report_data))
		panic("NOP requires NULL report and deflate\n");
#endif

	/* The hardware does not support a combination that includes
	 * de-compression + residue processing + data recovery.
	 * The driver software needs to make sure that this combination
	 * is disallowed. */
	if (unlikely(!input_data->size &&
		pme_mode_is_residue_used(ctx->params.mode) &&
		pme_mode_is_recovery_done(ctx->params.mode)))
		return -EINVAL;

	if (flags & PME_FLAG_NOP)
		blocking_ctx.refs++;
	else {
		/* Compute which callbacks will be done by examining the
		 * activity code. Always get a deflate callback unless we are
		 * passthrough or control */
		if (pme_mode_is_recovery_done(ctx->params.mode) ||
				!(pme_mode_is_passthru(ctx->params.mode) ||
				ctx->params.mode == PME_MODE_CONTROL)) {
			cmd_flags |= PME_FLAG_CB_DEFLATE;
			blocking_ctx.refs++;
		}
		if (pme_mode_is_scan_done(ctx->params.mode) ||
				ctx->params.mode == PME_MODE_CONTROL){
			cmd_flags |= PME_FLAG_CB_REPORT;
			blocking_ctx.refs++;
		}
	}

	if (cmd_flags & PME_FLAG_CB_COMMAND)
		blocking_ctx.refs++;

	/* Add in the ctx flags (exclusive and don't sleep) */
	cmd_flags |= ctx->flags;

	if (unlikely(cmd_flags & PME_FLAG_POLL)) {
		memcpy(&blocking_ctx.cb, cb, sizeof(*cb));
		dma_cb.completion = blocking_cb;
		dma_cb.ctx.words[0] = (u32)&blocking_ctx;
		ret = pme_object_cmd_blocking(ctx, cmd_flags,
				&dma_cb, input_data, deflate_data, report_data,
				!blocking_ctx.refs);
		atomic_dec(&ctx->state);
		return ret;
	}

	/* Build the callback structure */
	dma_cb.completion = cmd_completion_cb;
	memcpy(&dma_cb.ctx, cb, sizeof(dma_cb.ctx));
	ret = pme_object_cmd(ctx, cmd_flags, &dma_cb,
				 input_data, deflate_data, report_data);
	atomic_dec(&ctx->state);
	return ret;
}
EXPORT_SYMBOL(pme_context_io_cmd);

/* Context for flush callbacks */
struct flush_cb_ctx {
	wait_queue_head_t wait;
	int done;
	int result;
};

/* Flush callback */
static int flush_cb_completion(struct pme_object *obj,
					  u32 flags,
					  u8 exception_code,
					  u64 stream_id,
					  struct pme_callback *cb,
					  size_t output_used,
					  struct pme_fbchain
					  *fb_output)
{
	struct flush_cb_ctx *cb_ctx = (struct flush_cb_ctx *)cb->ctx.words[0];
	if (exception_code || (flags & PME_COMPLETION_ABORTED))
		cb_ctx->result = -EIO;
	cb_ctx->done = 1;
	wake_up(&cb_ctx->wait);
	return 0;
}

int pme_context_flush(struct pme_context *__ctx)
{
	struct pme_object *ctx = ctx2obj(__ctx);
	/* Send a NOP and wait for it to complete */
	int ret;
	struct pme_callback dma_cb;
	struct flush_cb_ctx cb_ctx = {
		.done = 0,
		.result = 0,
	};
	struct flush_cb_ctx *cb_ctx_p = &cb_ctx;
	u32 flags;

	if (ctx->flags & PME_FLAG_DONT_SLEEP)
		return -EWOULDBLOCK;

	might_sleep();

	flags = ctx->flags | PME_FLAG_NOP;
	init_waitqueue_head(&cb_ctx.wait);
	dma_cb.ctx.words[0] = (u32) cb_ctx_p;

	dma_cb.completion = flush_cb_completion;

	ret = pme_object_cmd(ctx, flags, &dma_cb, NULL, NULL, NULL);
	if (ret)
		return ret;
	wait_event(cb_ctx.wait, cb_ctx.done);
	return cb_ctx.result;
}
EXPORT_SYMBOL(pme_context_flush);

/* Controls the exclusive mode of the context */
int pme_context_set_exclusive_mode(struct pme_context *__ctx, int enable)
{
	struct pme_object *ctx = ctx2obj(__ctx);
	int ret = 0;

	/* Only allow priviledged users to go into exclusive mode */
	if (restrict_exclusive && !capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (atomic_inc_return(&ctx->state) <= 0) {
		atomic_dec(&ctx->state);
		return -EBUSY;
	}

	if (enable) {
		if (ctx->flags & PME_FLAG_EXCLUSIVITY) {
			ret = -EINVAL;
			goto done;
		}
		ret = pme_object_topdog(ctx, 1);
		if (ret)
			goto done;
		ctx->flags |= PME_FLAG_EXCLUSIVITY;

		ret = pme_context_flush(obj2ctx(ctx));
		if (ret) {
			pme_object_topdog(ctx, 0);
			ctx->flags ^= PME_FLAG_EXCLUSIVITY;
			goto done;
		}
	} else {
		if (!(ctx->flags & PME_FLAG_EXCLUSIVITY)) {
			ret = -EINVAL;
			goto done;
		}
		ctx->flags ^= PME_FLAG_EXCLUSIVITY;

		ret = pme_context_flush(obj2ctx(ctx));
		if (ret) {
			ctx->flags |= PME_FLAG_EXCLUSIVITY;
			goto done;
		}
		/* Release exclusive lock */
		ret = pme_object_topdog(ctx, 0);
		if (ret) {
			ctx->flags |= PME_FLAG_EXCLUSIVITY;
			goto done;
		}
	}
done:
	atomic_dec(&ctx->state);
	return ret;
}
EXPORT_SYMBOL(pme_context_set_exclusive_mode);

/* pimp_mod_context.c -- The rendering-context for a module
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include "pimp_mod_context.h"

void __pimp_mod_context_init(pimp_mod_context *ctx, const pimp_module *mod, const u8 *sample_bank, pimp_mixer *mixer)
{
	int i;
	ASSERT(ctx != NULL);
	
	ctx->mod = mod;
	ctx->sample_bank = sample_bank;
	ctx->mixer = mixer;
	
	/* setup default player-state */
	ctx->tick_len      = 0;
	ctx->curr_tick_len = 0;

	ctx->curr_row      = -1;
	ctx->curr_order    = -1;
	ctx->curr_pattern  = NULL;
	ctx->curr_tick     = 0;
	
	ctx->next_row      = 0;
	ctx->next_order    = 0;
	ctx->next_pattern = __pimp_module_get_pattern(ctx->mod, __pimp_module_get_order(ctx->mod, ctx->next_order));
	
	ctx->curr_bpm   = 125;
	ctx->curr_tempo = 5;
	ctx->remainder  = 0;
	
	ctx->global_volume = 1 << 9; /* 24.8 fixed point */
	
	ctx->curr_pattern = __pimp_module_get_pattern(mod, __pimp_module_get_order(mod, ctx->curr_order));
	__pimp_mod_context_set_bpm(ctx, ctx->mod->bpm);
	ctx->curr_tempo = mod->tempo;
	
	for (i = 0; i < CHANNELS; ++i)
	{
		pimp_channel_state *chan = &ctx->channels[i];
		chan->instrument  = (const pimp_instrument*)NULL;
		chan->sample      = (const pimp_sample*)    NULL;
		chan->vol_env.env = (const pimp_envelope*)  NULL;
		
		chan->loop_target_order = 0;
		chan->loop_target_row   = 0;
		chan->loop_counter      = 0;

		__pimp_envelope_reset(&chan->vol_env);
	}
	
	ctx->callback = (pimp_callback)NULL;

	__pimp_mixer_reset(ctx->mixer);
}

/* "hard" jump in a module */
void __pimp_mod_context_set_pos(pimp_mod_context *ctx, int row, int order)
{
	ASSERT(ctx != NULL);
	
	ctx->curr_tick = 0;
	ctx->curr_row = row;
	ctx->curr_order = order;
	if (ctx->curr_order >= ctx->mod->order_count)
	{
		ctx->curr_order = ctx->mod->order_repeat;
	}
	
	ctx->next_order = ctx->curr_order;
	ctx->next_pattern = __pimp_module_get_pattern(ctx->mod, __pimp_module_get_order(ctx->mod, ctx->curr_order));
	__pimp_mod_context_update_next_pos(ctx);
}

/* make sure next pos isn't outside a pattern or the order-list */
static void __pimp_mod_context_fix_next_pos(pimp_mod_context *ctx)
{
	if (ctx->next_row == ctx->curr_pattern->row_count)
	{
		ctx->next_row = 0;
		ctx->next_order++;
		
		/* check for pattern loop */
		if (ctx->next_order >= ctx->mod->order_count) ctx->next_order = ctx->mod->order_repeat;
	}
	
	if (ctx->next_order != ctx->curr_order)
	{
		ctx->next_pattern = __pimp_module_get_pattern(ctx->mod, __pimp_module_get_order(ctx->mod, ctx->next_order));
	}
}

/* setup next position to be one row advanced in module */
void __pimp_mod_context_update_next_pos(pimp_mod_context *ctx)
{
	ctx->next_row = ctx->curr_row + 1;
	ctx->next_order = ctx->curr_order;
	__pimp_mod_context_fix_next_pos(ctx);
}

/* setup next position to be a specific position. useful for jumping etc */
void __pimp_mod_context_set_next_pos(pimp_mod_context *ctx, int row, int order)
{
	ASSERT(ctx != NULL);
	
	ctx->next_row = row;
	ctx->next_order = order;
	__pimp_mod_context_fix_next_pos(ctx);
}


void __pimp_mod_context_set_bpm(pimp_mod_context *ctx, int bpm)
{
	ASSERT(ctx != NULL);
	ASSERT(bpm > 0);
	
	/* we're using 8 fractional-bits for the tick-length */
	const int temp = (int)(((SAMPLERATE) * 5) * (1 << 8));
	ctx->tick_len = temp / (bpm * 2);
}

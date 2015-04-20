/* pimp_mod_context.h -- The rendering-context for a module
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#ifndef PIMP_MOD_CONTEXT_H
#define PIMP_MOD_CONTEXT_H

#include "../include/pimpmobile.h" /* needed for pimp_callback */
#include "pimp_module.h"
#include "pimp_mixer.h"

#include "pimp_instrument.h"
#include "pimp_envelope.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	/* some current-states */
	const pimp_instrument *instrument;
	const pimp_sample     *sample;
	
	pimp_envelope_state vol_env;
	BOOL sustain;
	
	u8  note;
	u8  effect;
	u8  effect_param;
	u8  volume_command;

	s32 period;
	s32 final_period;
	
	s8  volume;
	u8  pan;
	
	/* vibrato states */
	u8 vibrato_speed;
	u8 vibrato_depth;
	u8 vibrato_waveform;
	u8 vibrato_counter;
	
	/* pattern loop states */
	u8 loop_target_order;
	u8 loop_target_row;
	u8 loop_counter;
		
	s32 porta_target;
	s32 fadeout;
	u16 porta_speed;
	s8  volume_slide_speed;
	u8  note_delay;
		
	u8  note_retrig;
	u8  retrig_tick;
} pimp_channel_state;

typedef struct
{
	u32 tick_len;
	u32 curr_tick_len;
	u32 remainder;

	u32 curr_row;
	u32 curr_order;
	
	u32 next_row;
	u32 next_order;
	
	/* used to delay row / order getters. usefull for demo-synching */
	u32 report_row;
	u32 report_order;
	
	u32 curr_bpm;
	u32 curr_tempo;
	u32 curr_tick;
	s32 global_volume; /* 24.8 fixed point */

	pimp_pattern *curr_pattern;
	pimp_pattern *next_pattern;
	
	pimp_channel_state channels[CHANNELS];
	
	const u8          *sample_bank;
	const pimp_module *mod;
	pimp_mixer        *mixer;
	
	pimp_callback callback;
} pimp_mod_context;

void __pimp_mod_context_init(pimp_mod_context *ctx, const pimp_module *mod, const u8 *sample_bank, pimp_mixer *mixer);
void __pimp_mod_context_set_bpm(pimp_mod_context *ctx, int bpm);
void __pimp_mod_context_set_tempo(pimp_mod_context *ctx, int tempo);

/* position manipulation */
void __pimp_mod_context_set_pos(pimp_mod_context *ctx, int row, int order);
void __pimp_mod_context_set_next_pos(pimp_mod_context *ctx, int row, int order);
void __pimp_mod_context_update_next_pos(pimp_mod_context *ctx);

static INLINE int __pimp_mod_context_get_row(const pimp_mod_context *ctx)
{
	return ctx->curr_row;
}

static INLINE int __pimp_mod_context_get_order(const pimp_mod_context *ctx)
{
	return ctx->curr_order;
}

static INLINE int __pimp_mod_context_get_bpm(const pimp_mod_context *ctx)
{
	return ctx->curr_bpm;
}

static INLINE int __pimp_mod_context_get_tempo(const pimp_mod_context *ctx)
{
	return ctx->curr_tempo;
}

#ifdef __cplusplus
}
#endif

#endif /* PIMP_MOD_CONTEXT_H */

/* pimp_render.c -- The actual rendering-code of Pimpmobile
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include <stdio.h>

#include "pimp_internal.h"
#include "pimp_render.h"
#include "pimp_debug.h"
#include "pimp_mixer.h"
#include "pimp_math.h"
#include "pimp_effects.h"

#define EFFECT_MISSING(ctx, eff_id) do {                           \
	DEBUG_PRINT(DEBUG_LEVEL_ERROR, ("** eff: %x\n", eff_id));      \
	if ((ctx)->callback != NULL)                                   \
	{                                                              \
		ctx->callback(PIMP_CALLBACK_UNSUPPORTED_EFFECT, (eff_id)); \
	}                                                              \
} while(0)

#define VOLUME_EFFECT_MISSING(ctx, eff_id) do {                           \
	DEBUG_PRINT(DEBUG_LEVEL_ERROR, ("** vol eff: %x\n", eff_id));         \
	if ((ctx)->callback != NULL)                                          \
	{                                                                     \
		ctx->callback(PIMP_CALLBACK_UNSUPPORTED_VOLUME_EFFECT, (eff_id)); \
	}                                                                     \
} while(0)


static int __pimp_channel_get_volume(pimp_channel_state *chan)
{
	ASSERT(chan != 0);
	
	int volume = chan->volume;
	
	if (chan->vol_env.env != 0)
	{
		/* envelope */
		volume = (volume * __pimp_envelope_sample(&chan->vol_env)) >> 8;
		__pimp_envelope_advance_tick(&chan->vol_env, chan->sustain);
		
		/* fadeout  */
		volume = (volume * chan->fadeout) >> 16;
		chan->fadeout -= chan->instrument->volume_fadeout;
		if (chan->fadeout <= 0)
		{
			/* TODO: kill sample */
			chan->fadeout = 0;
		}
	}
	else
	{
		if (!chan->sustain) volume = 0;
	}
	
	return volume;
}

static void note_on(pimp_channel_state *chan, pimp_pattern_entry *note)
{
}

static void __pimp_mod_context_update_row(pimp_mod_context *ctx)
{
	u32 c;
	ASSERT(ctx != 0);
	
	ctx->curr_tick    = 0;
	ctx->curr_row     = ctx->next_row;
	ctx->curr_order   = ctx->next_order;
	ctx->curr_pattern = ctx->next_pattern;
	__pimp_mod_context_update_next_pos(ctx);
	
	for (c = 0; c < ctx->mod->channel_count; ++c)
	{
		BOOL period_dirty = FALSE;
		BOOL volume_dirty = FALSE;
		
		pimp_channel_state *chan = &ctx->channels[c];
		volatile pimp_mixer_channel_state *mc = &ctx->mixer->channels[c];
		
		const pimp_pattern_entry *note = &get_pattern_data(ctx->curr_pattern)[ctx->curr_row * ctx->mod->channel_count + c];
		
#ifdef PRINT_PATTERNS
		print_pattern_entry(*note);
#endif
		
		chan->effect           = note->effect_byte;
		chan->effect_param     = note->effect_parameter;
		chan->volume_command   = note->volume_command;
		
		if (note->note == KEY_OFF)
		{
			chan->sustain = FALSE;
			volume_dirty  = TRUE; /* we need to update volume if note off killed note */
		}
		else
		{
			if (note->instrument > 0)
			{
				chan->instrument = __pimp_module_get_instrument(ctx->mod, note->instrument - 1);
				
				chan->vol_env.env = get_vol_env(chan->instrument);
				__pimp_envelope_reset(&chan->vol_env);
				chan->sustain = TRUE;
				chan->fadeout = 1 << 16;
				
				if (NULL != chan->sample)
				{
					chan->volume = chan->sample->volume;
					volume_dirty = TRUE;
				}
			}
			
			if (note->note > 0)
			{
				if (
					chan->instrument != 0 &&
					chan->effect != EFF_PORTA_NOTE &&
					chan->effect != EFF_PORTA_NOTE_VOLUME_SLIDE &&
					!(chan->effect == EFF_MULTI_FX && chan->effect_param ==  EFF_NOTE_DELAY)
					)
				{
					/* according to mixer_comments2.txt, vibrato counter is reset at new notes */
					chan->vibrato_counter = 0;
					
					if (chan->instrument->sample_count == 0)
					{
						/* TODO: this should be handeled in the converter, and as an assert. */
						
						/* stupid musician, tried to play an empty instrument... */
						mc->sample_data = NULL;
						mc->sample_cursor = 0;
						mc->sample_cursor_delta = 0;
					}
					else
					{
						chan->sample = get_sample(chan->instrument, chan->instrument->sample_map[note->note]);
						mc->sample_cursor = 0;
						mc->sample_data = ctx->sample_bank + chan->sample->data_ptr;
						mc->sample_length = chan->sample->length;
						mc->loop_type = (pimp_mixer_loop_type)chan->sample->loop_type;
						mc->loop_start = chan->sample->loop_start;
						mc->loop_end = chan->sample->loop_start + chan->sample->loop_length;
						
						if (ctx->mod->flags & FLAG_LINEAR_PERIODS)
						{
							chan->period = __pimp_get_linear_period(((s32)note->note) + chan->sample->rel_note, chan->sample->fine_tune);
						}
						else
						{
							chan->period = __pimp_get_amiga_period(((s32)note->note) + chan->sample->rel_note, chan->sample->fine_tune);
						}
						
						chan->final_period = chan->period;
						period_dirty = TRUE;
					}
				}
				
				if (chan->effect == EFF_SAMPLE_OFFSET)
				{
					mc->sample_cursor = (chan->effect_param * 256) << 12;
					
					if (mc->sample_cursor > (mc->sample_length << 12))
					{
						if (ctx->mod->flags & FLAG_SAMPLE_OFFSET_CLAMP) mc->sample_cursor = mc->sample_length << 12;
						else mc->sample_data = NULL; /* kill sample */
					}
				}
			}
		}
		
		if (note->instrument > 0)
		{
			chan->volume = chan->sample->volume;
			volume_dirty = TRUE;
		}

		switch (chan->volume_command >> 4)
		{
/*
  0       Do nothing
$10-$50   Set volume Value-$10
  :          :        :
  :          :        :
$60-$6f   Volume slide down
$70-$7f   Volume slide up
$80-$8f   Fine volume slide down
$90-$9f   Fine volume slide up
$a0-$af   Set vibrato speed
$b0-$bf   Vibrato
$c0-$cf   Set panning
$d0-$df   Panning slide left
$e0-$ef   Panning slide right
$f0-$ff   Tone porta
*/
			case 0x0:
			break;
			case 0x1:
			case 0x2:
			case 0x3:
			case 0x4:
			case 0x5:
				if (note->volume_command > 0x50)
				{
					/* something else */
					DEBUG_PRINT(DEBUG_LEVEL_ERROR, ("unsupported volume-command %02X\n", note->volume_command));
				}
				else
				{
					chan->volume = note->volume_command - 0x10;
					DEBUG_PRINT(DEBUG_LEVEL_INFO, ("setting volume to: %02X\n", chan->volume));
					volume_dirty = TRUE;
				}
			break;
			case 0x6: break;
			case 0x7: break;
			
			case 0x8:
				note_slide(chan, -(chan->volume_command & 0xF));
				volume_dirty = TRUE;
			break;
			
			case 0x9:
				note_slide(chan, chan->volume_command & 0xF);
				volume_dirty = TRUE;
			break;
			
			case 0xc:
				/* COMPLETELY UNTESTED CODE!!! */
				chan->pan = (chan->volume_command & 0xF) << 4;
			break;
			
			case 0xd:
				{
					/* COMPLETELY UNTESTED CODE!!! */
					int new_pan = ((int)chan->pan) - ((chan->volume_command & 0xF) << 4);
					if (new_pan < 0) new_pan = 0;
					chan->pan = new_pan;
				}
			break;
			
			case 0xe:
				{
					/* COMPLETELY UNTESTED CODE!!! */
					int new_pan = ((int)chan->pan) + ((chan->volume_command & 0xF) << 4);
					if (new_pan > 255) new_pan = 255;
					chan->pan = new_pan;
				}
			break;
			
			default:
				VOLUME_EFFECT_MISSING(ctx, chan->volume_command);
		}
		
		switch (chan->effect)
		{
			case EFF_NONE: break;
			
			case EFF_PORTA_UP:
				if (chan->effect_param != 0) chan->porta_speed = chan->effect_param * 4;
			break;
			
			case EFF_PORTA_DOWN:
				if (chan->effect_param != 0) chan->porta_speed = chan->effect_param * 4;
			break;
			
			case EFF_PORTA_NOTE:
				if (note->note > 0)
				{
					/* fine tune and relative note are taken into account */
					if (ctx->mod->flags & FLAG_LINEAR_PERIODS) chan->porta_target = __pimp_get_linear_period(note->note + chan->sample->rel_note, chan->sample->fine_tune);
					else chan->porta_target = __pimp_get_amiga_period(note->note + chan->sample->rel_note, chan->sample->fine_tune);
					
					/* clamp porta-target period (should not be done for S3M) */
					if (chan->porta_target > ctx->mod->period_high_clamp) chan->porta_target = ctx->mod->period_high_clamp;
					if (chan->porta_target < ctx->mod->period_low_clamp)  chan->porta_target = ctx->mod->period_low_clamp;
				}
				if (chan->effect_param != 0) chan->porta_speed = chan->effect_param * 4;
			break;

			case EFF_VIBRATO:
				if (0 != (chan->effect_param & 0xF0)) chan->vibrato_speed = chan->effect_param >> 4;
				if (0 != (chan->effect_param & 0x0F)) chan->vibrato_depth = chan->effect_param & 0x0F;
			break;
			
			case EFF_PORTA_NOTE_VOLUME_SLIDE:
				if (note->note > 0)
				{
					/* no fine tune or relative note here, boooy */
					if (ctx->mod->flags & FLAG_LINEAR_PERIODS) chan->porta_target = __pimp_get_linear_period(note->note + chan->sample->rel_note, 0);
					else chan->porta_target = __pimp_get_amiga_period(note->note, 0);
					
					/* clamp porta-target period (should not be done for S3M) */
					if (chan->porta_target > ctx->mod->period_high_clamp) chan->porta_target = ctx->mod->period_high_clamp;
					if (chan->porta_target < ctx->mod->period_low_clamp)  chan->porta_target = ctx->mod->period_low_clamp;
				}
				
				if (chan->effect_param & 0xF0)
				{
					chan->volume_slide_speed = chan->effect_param >> 4;
				}
				else if (chan->effect_param & 0x0F)
				{
					chan->volume_slide_speed = -(chan->effect_param & 0xF);
				}
			break;

			case EFF_VIBRATO_VOLUME_SLIDE: EFFECT_MISSING(ctx, chan->effect); break;
			case EFF_TREMOLO:              EFFECT_MISSING(ctx, chan->effect); break;

			case EFF_SET_PAN:
				chan->pan = chan->effect_param;
			break;

			case EFF_SAMPLE_OFFSET: break;
			
			case EFF_VOLUME_SLIDE:
				if (chan->effect_param & 0xF0)
				{
					chan->volume_slide_speed = chan->effect_param >> 4;
				}
				else if (chan->effect_param & 0x0F)
				{
					chan->volume_slide_speed = -(chan->effect_param & 0xF);
				}
			break;
			
			case EFF_JUMP_ORDER:
				/* go to order xy */
				__pimp_mod_context_set_next_pos( ctx, 0, chan->effect_param );
			break;

			case EFF_SET_VOLUME:
				chan->volume = chan->effect_param;
				if (chan->volume > 64) chan->volume = 64;
				volume_dirty = TRUE;
			break;

			case EFF_BREAK_ROW:
				/* go to next order, row xy (decimal) */
				__pimp_mod_context_set_next_pos(
					ctx,
					(chan->effect_param >> 4) * 10 + (chan->effect_param & 0xF), /* row */
					ctx->curr_order + 1 /* order */
				);
			break;
			
			case EFF_MULTI_FX:
				switch (chan->effect_param >> 4)
				{
					case EFF_AMIGA_FILTER: break;

					case EFF_FINE_PORTA_UP:
						porta_up(chan, ctx->mod->period_low_clamp);
						period_dirty = TRUE;
					break;
					
					case EFF_FINE_PORTA_DOWN:
						porta_down(chan, ctx->mod->period_high_clamp);
						period_dirty = TRUE;
					break;
					
					case EFF_PATTERN_LOOP:
						if (0 == (chan->effect_param & 0xF))
						{
							chan->loop_target_order = ctx->curr_order;
							chan->loop_target_row   = ctx->curr_row;
						}
						else
						{
							if (0 == chan->loop_counter)
							{
								chan->loop_counter = chan->effect_param & 0xF;
							}
							else chan->loop_counter--;
							
							if (0 < chan->loop_counter)
							{
								__pimp_mod_context_set_next_pos(ctx, chan->loop_target_row, chan->loop_target_order);
							}
						}
					break;
					
					case EFF_RETRIG_NOTE:
						if ((note->effect_parameter & 0xF) != 0)
						{
							chan->note_retrig = note->effect_parameter & 0xF;
						}
					break;
					
					case EFF_FINE_VOLUME_SLIDE_UP:
						note_slide(chan, chan->effect_param & 0xF);
						volume_dirty = TRUE;
					break;
					
					case EFF_FINE_VOLUME_SLIDE_DOWN:
						note_slide(chan, -(chan->effect_param & 0xF));
						volume_dirty = TRUE;
					break;
					
					case EFF_NOTE_DELAY:
						chan->note_delay = chan->effect_param & 0xF;
						chan->note = note->note;
					break;
					
					default:
						EFFECT_MISSING(ctx, (chan->effect << 4) | (chan->effect_param >> 4)); 
				}
			break;
			
			case EFF_TEMPO:
				if (note->effect_parameter < 0x20) ctx->curr_tempo = chan->effect_param;
				else __pimp_mod_context_set_bpm(ctx, chan->effect_param);
			break;
			
			case EFF_SET_GLOBAL_VOLUME:     EFFECT_MISSING(ctx, chan->effect); break;
			case EFF_GLOBAL_VOLUME_SLIDE:   EFFECT_MISSING(ctx, chan->effect); break;
			case EFF_KEY_OFF:               EFFECT_MISSING(ctx, chan->effect); break;
			case EFF_SET_ENVELOPE_POSITION: EFFECT_MISSING(ctx, chan->effect); break;
			case EFF_PAN_SLIDE:             EFFECT_MISSING(ctx, chan->effect); break;

			case EFF_MULTI_RETRIG:
				if ((note->effect_parameter & 0xF0) != 0) DEBUG_PRINT(DEBUG_LEVEL_ERROR, ("multi retrig x-parameter != 0 not supported\n"));
				if ((note->effect_parameter & 0xF) != 0) chan->note_retrig = note->effect_parameter & 0xF;
			break;
			
			case EFF_TREMOR: EFFECT_MISSING(ctx, chan->effect); break;
			
			case EFF_SYNC_CALLBACK:
				if (ctx->callback != NULL) ctx->callback(PIMP_CALLBACK_SYNC, chan->effect_param);
			break;
			
			case EFF_ARPEGGIO:  EFFECT_MISSING(ctx, chan->effect); break;
			case EFF_SET_TEMPO: EFFECT_MISSING(ctx, chan->effect); break;
			case EFF_SET_BPM:   EFFECT_MISSING(ctx, chan->effect); break;
			
			default:
				DEBUG_PRINT(DEBUG_LEVEL_ERROR, ("unsupported effect %02X\n", chan->effect));
				ASSERT(0);
		}
		
		if (period_dirty)
		{
			if (ctx->mod->flags & FLAG_LINEAR_PERIODS)
			{
				mc->sample_cursor_delta = __pimp_get_linear_delta(chan->final_period);
			}
			else
			{
				mc->sample_cursor_delta = __pimp_get_amiga_delta(chan->final_period);
			}
		}
		
		if (volume_dirty || chan->vol_env.env != 0)
		{
			mc->volume = (__pimp_channel_get_volume(chan) * ctx->global_volume) >> 8;
		}
	}

#ifdef PRINT_PATTERNS	
	iprintf("\n");
#endif
}

static void __pimp_mod_context_update_tick(pimp_mod_context *ctx)
{
	u32 c;
	if (ctx->mod == NULL) return; /* no module active (sound-effects can still be playing, though) */

	if (ctx->curr_tick == ctx->curr_tempo)
	{
		__pimp_mod_context_update_row(ctx);
		ctx->curr_tick++;
		return;
	}
	
	for (c = 0; c < ctx->mod->channel_count; ++c)
	{
		pimp_channel_state       *chan = &ctx->channels[c];
		pimp_mixer_channel_state *mc   = &ctx->mixer->channels[c];
		BOOL period_dirty = FALSE;
		BOOL volume_dirty = FALSE;
		
		switch (chan->volume_command >> 4)
		{
/*
  0       Do nothing
$10-$50   Set volume Value-$10
  :          :        :
  :          :        :
$60-$6f   Volume slide down
$70-$7f   Volume slide up
$80-$8f   Fine volume slide down
$90-$9f   Fine volume slide up
$a0-$af   Set vibrato speed
$b0-$bf   Vibrato
$c0-$cf   Set panning
$d0-$df   Panning slide left
$e0-$ef   Panning slide right
$f0-$ff   Tone porta
*/
			case 0x0:
			case 0x1:
			case 0x2:
			case 0x3:
			case 0x4:
			case 0x5: break;
	
			case 0x6:
				note_slide(chan, -(chan->volume_command & 0xF));
				volume_dirty = TRUE;
			break;
			
			case 0x7:
				note_slide(chan, chan->volume_command & 0xF);
				volume_dirty = TRUE;
			break;
			
			case 0x8: break;
			case 0x9: break;
			case 0xc: break;
			case 0xd: break;
			case 0xe: break;
		}
		
		switch (chan->effect)
		{
			case EFF_NONE: break;
			
			case EFF_PORTA_UP:
				porta_up(chan, ctx->mod->period_low_clamp);
				period_dirty = TRUE;
			break;
			
			case EFF_PORTA_DOWN:
				porta_down(chan, ctx->mod->period_high_clamp);
				period_dirty = TRUE;
			break;
			
			case EFF_PORTA_NOTE:
				porta_note(chan);
				period_dirty = TRUE;
			break;
			
			case EFF_VIBRATO:
				vibrato(chan, ctx->mod->period_low_clamp, ctx->mod->period_high_clamp);
				period_dirty = TRUE;
			break;
			
			case EFF_PORTA_NOTE_VOLUME_SLIDE:
				porta_note(chan);
				period_dirty = TRUE;
				
				note_slide(chan, chan->volume_slide_speed);
				volume_dirty = TRUE;
			break;
			
			case EFF_SET_PAN: break;
			case EFF_SAMPLE_OFFSET: break;
			
			case EFF_VOLUME_SLIDE:
				note_slide(chan, chan->volume_slide_speed);
				volume_dirty = TRUE;
			break;
			
			case EFF_MULTI_FX:
				switch (chan->effect_param >> 4)
				{
					case EFF_AMIGA_FILTER: break;
					case EFF_FINE_PORTA_UP: break;
					case EFF_FINE_PORTA_DOWN: break;
					case EFF_PATTERN_LOOP: break;
					
					case EFF_RETRIG_NOTE:
						chan->retrig_tick++;
						if (chan->retrig_tick == chan->note_retrig)
						{
							mc->sample_cursor = 0;
							chan->retrig_tick = 0;
						}
					break;

					case EFF_FINE_VOLUME_SLIDE_UP:
					case EFF_FINE_VOLUME_SLIDE_DOWN:
					break; /* fine volume slide is only done on tick0 */
					
					case EFF_NOTE_DELAY:
						/* note on */
						if (--chan->note_delay == 0)
						{
							/* TODO: replace with a note_on-function */
							if (chan->instrument != 0)
							{
								chan->sample = get_sample(chan->instrument, chan->instrument->sample_map[chan->note]);
								mc->sample_cursor = 0;
								mc->sample_data = ctx->sample_bank + chan->sample->data_ptr;
								mc->sample_length = chan->sample->length;
								mc->loop_type = (pimp_mixer_loop_type)chan->sample->loop_type;
								mc->loop_start = chan->sample->loop_start;
								mc->loop_end = chan->sample->loop_start + chan->sample->loop_length;
								
								if (ctx->mod->flags & FLAG_LINEAR_PERIODS)
								{
									chan->period = __pimp_get_linear_period(((s32)chan->note) + chan->sample->rel_note, chan->sample->fine_tune);
								}
								else
								{
									chan->period = __pimp_get_amiga_period(((s32)chan->note) + chan->sample->rel_note, chan->sample->fine_tune);
								}
								chan->final_period = chan->period;
								period_dirty = TRUE;
							}
						}
					break;
				}
			break;
			
			case EFF_MULTI_RETRIG:
				chan->retrig_tick++;
				if (chan->retrig_tick == chan->note_retrig)
				{
					mc->sample_cursor = 0;
					chan->retrig_tick = 0;
				}
			break;
			
/*				default: ASSERT(0); */
		}
		
		/* period to delta-conversion */
		if (period_dirty)
		{
			if (ctx->mod->flags & FLAG_LINEAR_PERIODS)
			{
				mc->sample_cursor_delta = __pimp_get_linear_delta(chan->final_period);
			}
			else
			{
				mc->sample_cursor_delta = __pimp_get_amiga_delta(chan->final_period);
			}
		}
		
		if (volume_dirty || chan->vol_env.env != 0)
		{
			mc->volume = (__pimp_channel_get_volume(chan) * ctx->global_volume) >> 8;
		}
	}
	
	ctx->curr_tick++;
}

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x, y) ((x) > (y) ? (y) : (x))
#endif

void __pimp_render(pimp_mod_context *ctx, s8 *buf, u32 samples)
{
	while (TRUE)
	{
		int samples_to_mix = MIN(ctx->remainder, samples);
		if (samples_to_mix != 0) __pimp_mixer_mix(ctx->mixer, buf, samples_to_mix);
		
		buf            += samples_to_mix;
		samples        -= samples_to_mix;
		ctx->remainder -= samples_to_mix;
		
		if (samples == 0) break;
		
		__pimp_mod_context_update_tick(ctx);
		
		/* fixed point tick length */
		ctx->curr_tick_len += ctx->tick_len;
		ctx->remainder      = ctx->curr_tick_len >> 8;
		ctx->curr_tick_len -= (ctx->curr_tick_len >> 8) << 8;
	}
}

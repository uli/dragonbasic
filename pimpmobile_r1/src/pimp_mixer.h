/* pimp_mixer.h -- The state and interface for the various mixers
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#ifndef PIMP_MIXER_H
#define PIMP_MIXER_H

#include "pimp_base.h"
#include "pimp_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	LOOP_TYPE_NONE,
	LOOP_TYPE_FORWARD,
	LOOP_TYPE_PINGPONG
} pimp_mixer_loop_type;

typedef struct
{
	u32                   sample_length;
	u32                   loop_start;
	u32                   loop_end;
	pimp_mixer_loop_type  loop_type;
	const u8             *sample_data;
	u32                   sample_cursor;
	s32                   sample_cursor_delta;
	u32                   event_cursor; /* the position of the next event */
	s32                   volume;
} pimp_mixer_channel_state;

typedef struct
{
	pimp_mixer_channel_state channels[CHANNELS];
	s32 *mix_buffer;
} pimp_mixer;
	
void __pimp_mixer_reset(pimp_mixer *mixer);
void __pimp_mixer_mix(pimp_mixer *mixer, s8 *target, int samples);
void __pimp_mixer_mix_channel(pimp_mixer_channel_state *chan, s32 *target, u32 samples);

void __pimp_mixer_clear(s32 *target, u32 samples);
u32  __pimp_mixer_mix_samples(s32 *target, u32 samples, const u8 *sample_data, u32 vol, u32 sample_cursor, s32 sample_cursor_delta);
void __pimp_mixer_clip_samples(s8 *target, s32 *source, u32 samples, u32 dc_offs);


#ifdef __cplusplus
}
#endif

#endif /* PIMP_MIXER_H */

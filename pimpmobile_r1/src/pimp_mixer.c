/* pimp_mixer.c -- High level mixer code
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include "pimp_mixer.h"
#include "pimp_debug.h"

void __pimp_mixer_reset(pimp_mixer *mixer)
{
	u32 c;
	ASSERT(mixer != NULL);
	
	for (c = 0; c < CHANNELS; ++c)
	{
		mixer->channels[c].sample_data = 0;
		mixer->channels[c].sample_cursor = 0;
		mixer->channels[c].sample_cursor_delta = 0;
	}
}

/*
	TODO: this all needs a rewrite.
	next implementation should always keep track of the next loop-event for all active channels,
	to make binary searching for hit-point easier.
*/

static s32 event_delta = 0;
static s32 event_cursor = 0;

static BOOL detect_loop_event(pimp_mixer_channel_state *chan, int samples)
{
	ASSERT(samples != 0);
	
	s32 check_point;
	s32 end_sample = chan->sample_cursor + chan->sample_cursor_delta * samples;
	
	switch (chan->loop_type)
	{
		case LOOP_TYPE_NONE:
			check_point = chan->sample_length;
		break;
		
		case LOOP_TYPE_FORWARD:
			check_point = chan->loop_end;
		break;
		
		case LOOP_TYPE_PINGPONG:
			if (chan->sample_cursor_delta >= 0)
			{
				/* moving forwards through the sample */
				check_point = chan->loop_end;
			}
			else
			{
				/* moving backwards through the sample */
				check_point = chan->loop_start;
				if (end_sample <= (check_point << 12))
				{
					event_delta = -chan->sample_cursor_delta;
					event_cursor = -((check_point << 12) - chan->sample_cursor - 1);
					return TRUE;
				}
				return FALSE;
			}
		break;
	}
	
	if (end_sample >= (check_point << 12))
	{
		event_delta = chan->sample_cursor_delta;
		event_cursor = (check_point << 12) - chan->sample_cursor;
		return TRUE;
	}
	
	return FALSE;
}

/* returns false if we hit sample-end */
BOOL process_loop_event(pimp_mixer_channel_state *chan)
{
	switch (chan->loop_type)
	{
		case LOOP_TYPE_NONE:
			return FALSE;
		break;
		
		case LOOP_TYPE_FORWARD:
			do
			{
				chan->sample_cursor -= (chan->loop_end - chan->loop_start) << 12;
			}
			while (chan->sample_cursor >= (chan->loop_end << 12));
		break;
		
		case LOOP_TYPE_PINGPONG:
			do
			{
//				printf("");
				if (chan->sample_cursor_delta >= 0)
				{
					chan->sample_cursor -=  chan->loop_end << 12;
					chan->sample_cursor  = -chan->sample_cursor;
					chan->sample_cursor +=  chan->loop_end << 12;
					ASSERT(chan->sample_cursor > 0);
					chan->sample_cursor -= 1;
				}
				else
				{
					chan->sample_cursor -=  (chan->loop_start) << 12;
					chan->sample_cursor  = -chan->sample_cursor;
					chan->sample_cursor +=  (chan->loop_start) << 12;
					ASSERT(chan->sample_cursor > (chan->loop_start << 12));
					chan->sample_cursor -= 1;
				}
				chan->sample_cursor_delta = -chan->sample_cursor_delta;
			}
			while ((chan->sample_cursor > (chan->loop_end << 12)) || (chan->sample_cursor < (chan->loop_start << 12)));
		break;
	}

	return TRUE;
}

void __pimp_mixer_mix_channel(pimp_mixer_channel_state *chan, s32 *target, u32 samples)
{
	ASSERT(NULL != chan);
	ASSERT(samples > 0);
	
	while (samples > 0 && detect_loop_event(chan, samples) == TRUE)
	{
		/* TODO: iterative binary search here instead */

		do
		{
			ASSERT((chan->sample_cursor >> 12) < chan->sample_length);
			ASSERT(chan->sample_data != 0);

			
			const s32 samp = ((u8*)chan->sample_data)[chan->sample_cursor >> 12];
			chan->sample_cursor += chan->sample_cursor_delta;
			*target++ += samp * chan->volume;

			
			samples--;
			event_cursor -= event_delta;
		}
		while (event_cursor > 0);
		
		ASSERT(samples >= 0);
		
		if (process_loop_event(chan) == FALSE)
		{

			/* the sample has stopped, we need to fill the rest of the buffer with the dc-offset, so it doesn't ruin our unsigned mixing-thing */
			while (samples--)
			{
				*target++ += chan->volume * 128;
			}
			/* terminate sample */
			chan->sample_data = 0;
			return;
		}
		
		/* check that process_loop_event() didn't put us outside the sample */
		ASSERT((chan->sample_cursor >> 12) < chan->sample_length);
	}

	ASSERT(chan->sample_data != 0);
#if 1
	chan->sample_cursor = __pimp_mixer_mix_samples(target, samples, chan->sample_data, chan->volume, chan->sample_cursor, chan->sample_cursor_delta);
#else
	chan->sample_cursor = chan->sample_cursor + chan->sample_cursor_delta * samples;
#endif
}

void __pimp_mixer_mix(pimp_mixer *mixer, s8 *target, int samples)
{
	u32 c;
	int dc_offs;
	
	ASSERT(NULL != mixer);
	ASSERT(NULL != mixer->mix_buffer);
	ASSERT(NULL != target);
	ASSERT(samples >= 0);

	__pimp_mixer_clear(mixer->mix_buffer, samples);
	
	dc_offs = 0;
	for (c = 0; c < CHANNELS; ++c)
	{
		pimp_mixer_channel_state *chan = &mixer->channels[c];
		if ((NULL != chan->sample_data) && (chan->volume > 1))
		{
			__pimp_mixer_mix_channel(chan, mixer->mix_buffer, samples);
			dc_offs += chan->volume * 128;
		}
	}
	
	dc_offs >>= 8;
	
	__pimp_mixer_clip_samples(target, mixer->mix_buffer, samples, dc_offs);
}

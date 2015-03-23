/* pimp_envelope.h -- Envelope routines
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include "pimp_envelope.h"
#include "pimp_debug.h"

#include <stdio.h>

static int __pimp_envelope_find_node(const pimp_envelope *env, int tick)
{
	int i;
	
	ASSERT(NULL != env);
	ASSERT(tick >= 0);
	
	for (i = 1; i < env->node_count; ++i)
	{
		if (env->node_tick[i] > tick)
		{
			return i - 1;
		}
	}
	
	return env->node_count - 1;
}

int __pimp_envelope_sample(pimp_envelope_state *state)
{
	s32 delta;
	u32 internal_tick;
	
	ASSERT(NULL != state);

	/* the magnitude of the envelope at tick N:
	 * first, find the last node at or before tick N - its position is M
	 * then, the magnitude of the envelope at tick N is given by
	 * magnitude = node_magnitude[M] + ((node_delta[M] * (N - M)) >> 8)
	 */
	
	delta = state->env->node_delta[state->current_node];
	internal_tick = state->current_tick - state->env->node_tick[state->current_node];
	
	int val = state->env->node_magnitude[state->current_node];
	val += ((long long)delta * internal_tick) >> 9;
	
	return val << 2;
}

void __pimp_envelope_advance_tick(pimp_envelope_state *state, BOOL sustain)
{
	ASSERT(NULL != state);
	
	/* advance a tick */
	state->current_tick++;
	
	/* check for sustain loop */
	if ((state->env->flags & (1 << 1)) && (sustain == TRUE))
	{
		if (state->current_tick >= state->env->sustain_loop_end)
		{
			state->current_node = state->env->sustain_loop_start;
			state->current_tick = state->env->node_tick[state->current_node];
		}
	}
	
	/* check if we have passed the current node
	 * we don't need to clamp the envelope-pos to the end, since the last delta is zero
	 */
	if (state->current_node < (state->env->node_count - 1))
	{
		if (state->current_tick >= state->env->node_tick[state->current_node + 1])
		{
			state->current_node++;
		}
	}
}

void __pimp_envelope_set_tick(pimp_envelope_state *state, int tick)
{
	state->current_node = __pimp_envelope_find_node(state->env, tick);
	state->current_tick = tick;
}

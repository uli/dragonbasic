/* pimp_envelope.h -- Envelope data structure and routines
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#ifndef PIMP_ENVELOPE_H
#define PIMP_ENVELOPE_H

#include "pimp_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	u16 node_tick[25];
	s16 node_magnitude[25];
	s16 node_delta[25];

	u8 node_count;
	u8 flags; /* bit 0: loop enable, bit 1: sustain loop enable */
	u8 loop_start, loop_end;
	u8 sustain_loop_start, sustain_loop_end;
} pimp_envelope;

typedef struct
{
	const pimp_envelope *env;
	s8  current_node;
	u32 current_tick;
} pimp_envelope_state;

static INLINE void __pimp_envelope_reset(pimp_envelope_state *state)
{
	state->current_node = 0;
	state->current_tick = 0;
}

int __pimp_envelope_sample(pimp_envelope_state *state);
void __pimp_envelope_advance_tick(pimp_envelope_state *state, BOOL sustain);
void __pimp_envelope_set_tick(pimp_envelope_state *state, int tick);

#ifdef __cplusplus
}
#endif

#endif /* PIMP_ENVELOPE_H */

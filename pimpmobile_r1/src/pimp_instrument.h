/* pimp_instrument.h -- Instrument data structure and getter functions
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#ifndef PIMP_INSTRUMENT_H
#define PIMP_INSTRUMENT_H

#include "pimp_types.h"

typedef struct
{
	u32 sample_ptr;
	u32 vol_env_ptr;
	u32 pan_env_ptr;

#if 0 /* IT ONLY (later) */
	u32 pitch_env_ptr;
#endif

	u16 volume_fadeout;
	
	u8 sample_count;                 /* number of samples tied to instrument */
	u8 sample_map[120];

#if 0 /* IT ONLY (later) */
	new_note_action_t        new_note_action;
	duplicate_check_type_t   duplicate_check_type;
	duplicate_check_action_t duplicate_check_action;
	
	s8 pitch_pan_separation; /* no idea what this one does */
	u8 pitch_pan_center;     /* not this on either; this one seems to be a note index */
#endif

} pimp_instrument;

#include "pimp_internal.h"
#include "pimp_debug.h"
#include "pimp_envelope.h"
#include "pimp_sample.h"

static INLINE pimp_sample *get_sample(const pimp_instrument *instr, int i)
{
	ASSERT(instr != NULL);
	return &((pimp_sample*)get_ptr(&instr->sample_ptr))[i];
}

static INLINE pimp_envelope *get_vol_env(const pimp_instrument *instr)
{
	ASSERT(instr != NULL);
	return (pimp_envelope*)(instr->vol_env_ptr == 0 ? NULL : get_ptr(&instr->vol_env_ptr));
}

#endif /* PIMP_INSTRUMENT_H */

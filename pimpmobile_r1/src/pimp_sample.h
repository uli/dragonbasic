/* pimp_gba.c -- GameBoy Advance c-interface for Pimpmobile
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#ifndef PIMP_SAMPLE_H
#define PIMP_SAMPLE_H

typedef struct pimp_sample
{
	/* offset relative to sample-bank */
	u32 data_ptr;

	u32 length;
	u32 loop_start;
	u32 loop_length;
	
/*
	IT ONLY (later)
	u32 sustain_loop_start;
	u32 sustain_loop_end;
*/
	s16 fine_tune;
	s16 rel_note;

	u8  volume;
	u8  loop_type;
	u8  pan;
	
	u8 vibrato_speed;
	u8 vibrato_depth;
	u8 vibrato_sweep;
	u8 vibrato_waveform;

/*
	IT ONLY (later)
	u8  sustain_loop_type;
*/

} pimp_sample;


#endif /* PIMP_SAMPLE_H */

/* pimp_math.c -- Math routines for use in Pimpmobile
 * Copyright (C) 2005-2006 J�rn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include "pimp_types.h"
#include "pimp_math.h"
#include "pimp_debug.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

const u8 __pimp_clz_lut[256] =
{
	0x8, 0x7, 0x6, 0x6, 0x5, 0x5, 0x5, 0x5, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4,
	0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
	0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2,
	0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
};


#ifndef NO_LINEAR_PERIODS
unsigned __pimp_get_linear_period(int note, int fine_tune)
{
	ASSERT(fine_tune >= -8);
	ASSERT(fine_tune <   8);
	
	int xm_note = note - (12 * 1) - 1; /* we extended our note-range with one octave. */
	
	return (10 * 12 * 16 * 4 - xm_note * 16 * 4 - fine_tune / 2);
/*	return 10 * 12 * 16 * 4 - note * 16 * 4 - fine_tune / 2; */
}

#include "linear_delta_lut.h"
unsigned __pimp_get_linear_delta(unsigned period)
{
	unsigned p = (12 * 16 * 4 * 14) - period;
	unsigned octave        = p / (12 * 16 * 4);
	unsigned octave_period = p % (12 * 16 * 4);
	unsigned delta = linear_delta_lut[octave_period] << octave;

	/* BEHOLD: the expression of the devil (this compiles to one arm-instruction) */
	const unsigned int scale = (unsigned int)((1.0 / (SAMPLERATE)) * (1 << 3) * (1ULL << 32));
	delta = ((long long)delta * scale + (1ULL << 31)) >> 32;
	return delta;
}
#endif /* NO_LINEAR_PERIODS */

#ifndef NO_AMIGA_PERIODS
#include "amiga_period_lut.h"
unsigned __pimp_get_amiga_period(int note, int fine_tune)
{
	fine_tune /= 8; /* todo: interpolate instead? */
	ASSERT(fine_tune >= -8);
	ASSERT(fine_tune <=  8);
	
	/* bias up one octave to prevent from negative values due to fine tune*/
	unsigned index = 12 * 8 + note * 8 + fine_tune;
	
	/* handle notes outside of the mod-range by shifting up or down */
	if (index < (12 * 8 * 5))
	{
		unsigned octave       = index / (12 * 8);
		unsigned octave_index = index % (12 * 8);
		return (((u32)amiga_period_lut[octave_index]) * 4) << (5 - octave);
	}

	if (index >= ARRAY_SIZE(amiga_period_lut) + 12 * 8 * 5)
	{
		unsigned octave       = index / (12 * 8);
		unsigned octave_index = index % (12 * 8);
		return (((u32)amiga_period_lut[octave_index]) * 4) >> (octave - 5);
	}

	return ((u32)amiga_period_lut[index - (12 * 8 * 5)]) * 4;
}

#include "amiga_delta_lut.h"
#define AMIGA_DELTA_LUT_SIZE (1 << AMIGA_DELTA_LUT_LOG2_SIZE)
#define AMIGA_DELTA_LUT_FRAC_BITS (15 - AMIGA_DELTA_LUT_LOG2_SIZE)
unsigned __pimp_get_amiga_delta(unsigned period)
{
	unsigned shamt = clz16(period) - 1;
	unsigned p = period << shamt;
	unsigned p_frac = p & ((1 << AMIGA_DELTA_LUT_FRAC_BITS) - 1);
	p >>= AMIGA_DELTA_LUT_FRAC_BITS;

	/* interpolate table-entries for better result */
	int d1 = amiga_delta_lut[p     - (AMIGA_DELTA_LUT_SIZE / 2)]; /* (8363 * 1712) / float(p) */
	int d2 = amiga_delta_lut[p + 1 - (AMIGA_DELTA_LUT_SIZE / 2)]; /* (8363 * 1712) / float(p + 1) */
	unsigned delta = (d1 << AMIGA_DELTA_LUT_FRAC_BITS) + (d2 - d1) * p_frac;

	if (shamt > AMIGA_DELTA_LUT_FRAC_BITS) delta <<= shamt - AMIGA_DELTA_LUT_FRAC_BITS;
	else delta >>= AMIGA_DELTA_LUT_FRAC_BITS - shamt;

	/* BEHOLD: the expression of the devil 2.0 (this compiles to one arm-instruction) */
	const unsigned int scale = (unsigned int)(((1.0 / (SAMPLERATE)) * (1 << 6)) * (1LL << 32));
	delta = ((long long)delta * scale + (1ULL << 31)) >> 32;
	
	return delta;
}
#endif /* NO_AMIGA_PERIODS */


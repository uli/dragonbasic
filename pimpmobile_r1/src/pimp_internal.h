#ifndef PIMP_INTERNAL_H
#define PIMP_INTERNAL_H

/* pimp_internal.h -- Internal enums and routines for Pimpmobile
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include "pimp_base.h"
#include "pimp_config.h"

static INLINE void *get_ptr(const unsigned int *offset)
{
	return (char*)offset + *offset;
}

static INLINE void set_ptr(unsigned int *dst, const void *ptr)
{
	*dst = (int)((char *)ptr - (char *)dst);
}


#define KEY_OFF 121

typedef enum
{
	EFF_NONE                    = 0x00,
	EFF_PORTA_UP                = 0x01,
	EFF_PORTA_DOWN              = 0x02,
	EFF_PORTA_NOTE              = 0x03,
	EFF_VIBRATO                 = 0x04,
	EFF_PORTA_NOTE_VOLUME_SLIDE = 0x05,
	EFF_VIBRATO_VOLUME_SLIDE    = 0x06,
	EFF_TREMOLO                 = 0x07,
	EFF_SET_PAN                 = 0x08,
	EFF_SAMPLE_OFFSET           = 0x09,
	EFF_VOLUME_SLIDE            = 0x0A,
	EFF_JUMP_ORDER              = 0x0B,
	EFF_SET_VOLUME              = 0x0C,
	EFF_BREAK_ROW               = 0x0D,
	EFF_MULTI_FX                = 0x0E,
	EFF_TEMPO                   = 0x0F,
	EFF_SET_GLOBAL_VOLUME       = 0x10,
	EFF_GLOBAL_VOLUME_SLIDE     = 0x11,
	EFF_KEY_OFF                 = 0x14,
	EFF_SET_ENVELOPE_POSITION   = 0x15,
	EFF_PAN_SLIDE               = 0x19,
	EFF_MULTI_RETRIG            = 0x1B,
	EFF_TREMOR                  = 0x1D,
	EFF_SYNC_CALLBACK           = 0x20,
	/* missing 0x21 here, multi-command (X1 = extra fine porta up, X2 = extra fine porta down) */
	EFF_ARPEGGIO                = 0x24,
	EFF_SET_TEMPO               = 0x25,
	EFF_SET_BPM                 = 0x26,

/*
	TODO: fill in the rest
	27xx: set channel volume (impulse-tracker)
	28xx: slide channel volume (impulse-tracker)
	29xx: panning vibrato (impulse-tracker)
	2Axx: fine vibrato
	2Bxy: multi-effect (impulse-tracker only)
		5x: panning vibrato waveform( sane as vibrato/tremolo waveform)
		70: past note cut
		71: past note off
		72: past note fade
		73: set NNA to note cut
		74: set NNA to continue
		75: set NNA to note-off
		76: set NNA to note-fade
		77: turn off volume envelope
		78: turn on volume envelope
		79: turn off panning envelope
		7A: turn on panning envelope
		7B: turn off pitch envelope
		7C: turn on pitch envelope
		91: "set surround sound" ???
	
*/
} pimp_effect;

typedef enum
{
	EFF_AMIGA_FILTER           = 0x0,
	EFF_FINE_PORTA_UP          = 0x1,
	EFF_FINE_PORTA_DOWN        = 0x2,
	EFF_PATTERN_LOOP           = 0x6,
	EFF_RETRIG_NOTE            = 0x9,
	EFF_FINE_VOLUME_SLIDE_UP   = 0xA,
	EFF_FINE_VOLUME_SLIDE_DOWN = 0xB,
	EFF_NOTE_CUT_AFTER_X_TICKS = 0xC,
	EFF_NOTE_DELAY             = 0xD,
/*
		0x: Amiga filter on/off (ignored; according to everybody, it sucks)
		1x: fine porta up
		2x: fine porta down
		3x: glissando control (0=off, 1=on)
		4x: set vibrato waveform/control:
			0=sine, 1=ramp-down, 2=square, 3=random
			+4: vibrato waveform position is not reset on new-note
		5x: set fine-tune for note
		6x: pattern loop command (x=0 marks start of loop,
			x != 0 marks end of loop, with x=loop iterations to apply)
		7x: set tremolo control (same syntax as vibrato control)
		8x: normally unused; Digitrakker uses this command
			to control the looping mode of the currently-playing sample
			(0=off, 1=forward, 3=pingpong)
		9x: retrig note
		Ax: fine volume slide up
		Bx: fine volume slide down
		Cx: note cut after x ticks
		Dx: note delay by x ticks
		Ex: pattern delay by x frames
		Fx: FunkInvert (does anyone actually have an idea what this does?)
*/
} pimp_multi_effect;

/* packed, because it's all bytes. no member-alignment or anything needed */
typedef struct __attribute__((packed))
{
	u8 note;
	u8 instrument;
	u8 volume_command;
	u8 effect_byte;
	u8 effect_parameter;
} pimp_pattern_entry;

enum
{
	FLAG_LINEAR_PERIODS          = (1 <<  0),
	FLAG_LINEAR_VIBRATO          = (1 <<  1),
	FLAG_VOL_SLIDE_TICK0         = (1 <<  2),
	FLAG_VIBRATO_TICK0           = (1 <<  3),
	FLAG_VOL0_OPTIMIZE           = (1 <<  4),
	FLAG_TEMOR_EXTRA_DELAY       = (1 <<  5),
	FLAG_TEMOR_MEMORY            = (1 <<  6),
	FLAG_RETRIG_KILLS_NOTE       = (1 <<  7),
	FLAG_NOTE_CUT_KILLS_NOTE     = (1 <<  8),
	FLAG_ALLOW_NESTED_LOOPS      = (1 <<  9),
	FLAG_RETRIG_NOTE_PERIOD      = (1 << 10),
	FLAG_DELAY_GLOBAL_VOLUME     = (1 << 11),
	FLAG_SAMPLE_OFFSET_CLAMP     = (1 << 12),
	FLAG_PORTA_NOTE_SHARE_MEMORY = (1 << 13),
	FLAG_PORTA_NOTE_MEMORY       = (1 << 14),
};

#endif /* PIMP_INTERNAL_H */

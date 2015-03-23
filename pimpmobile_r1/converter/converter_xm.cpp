/* converter_xm.cpp -- FastTracker XM converting code
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "converter.h"

typedef struct
{
	unsigned int   header_size;
	unsigned short len;
	unsigned short restart_pos;
	unsigned short channels;
	unsigned short patterns;
	unsigned short instruments;
	unsigned short flags;
	unsigned short tempo;
	unsigned short bpm;
} xm_header_t;

typedef struct
{
	unsigned int   header_size;
	unsigned char  packing_type;
	unsigned short rows;
	unsigned short data_size;
} xm_pattern_header_t;

typedef struct
{
	// part 1
	unsigned int   header_size;
	char           name[22 + 1];
	unsigned char  type;
	unsigned short samples;
	
	// part 2
	unsigned int   sample_header_size;
	unsigned char  sample_number[96];
	unsigned short vol_env[48];
	unsigned short pan_env[48];
	unsigned char  vol_env_points;
	unsigned char  pan_env_points;
	unsigned char  vol_sustain;
	unsigned char  vol_loop_start;
	unsigned char  vol_loop_end;
	unsigned char  pan_sustain;
	unsigned char  pan_loop_start;
	unsigned char  pan_loop_end;
	unsigned char  vol_type;
	unsigned char  pan_type;
	unsigned char  vibrato_type;
	unsigned char  vibrato_sweep;
	unsigned char  vibrato_depth;
	unsigned char  vibrato_rate;
	unsigned short volume_fadeout;
} xm_instrument_header_t;

typedef struct
{
	unsigned int  length;
	unsigned int  loop_start;
	unsigned int  loop_length;
	unsigned char volume;
	signed char   fine_tune;
	unsigned char type;
	unsigned char pan;
	signed char   rel_note;
	char          name[22 + 1];
} xm_sample_header_t;

module_t *load_module_xm(FILE *fp)
{
	rewind(fp);	
	char temp[17];
	fread(temp, 17, 1, fp);
	if (memcmp(temp, "Extended Module: ", 17) != 0) return 0;
	
	// song name
	char name[20 + 1];
	fread(name, 20, 1, fp);
	name[20] = '\0';
/*	printf("name : \"%s\"\n", name); */

	// just another file-sanity-check
	unsigned char magic;
	fread(&magic, 1, 1, fp);
	if (magic != 0x1a) return 0;
	
	// tracker name (seems to not be very reliable for bug-emulating)
	char tracker_name[20 + 1];
	fread(tracker_name, 20, 1, fp);
	tracker_name[20] = '\0';
/*	printf("tracker-name : \"%s\"\n", tracker_name); */
	
	unsigned short version;
	fread(&version, 2, 1, fp);
	if (version < 0x104)
	{
		printf("too old file format version, please open and resave in ft2\n");
		fclose(fp);
		exit(1);
	}
	
	module_t *mod = (module_t *)malloc(sizeof(module_t));
	if (mod == 0)
	{
		printf("out of memory\n");
		fclose(fp);
		exit(1);		
	}
	memset(mod, 0, sizeof(module_t));
	strcpy(mod->name, name);
	
	xm_header_t xm_header;
	memset(&xm_header, 0, sizeof(xm_header));
	
	// load song-header
	fread(&xm_header.header_size, 4, 1, fp);
	fread(&xm_header.len,         2, 1, fp);
	fread(&xm_header.restart_pos, 2, 1, fp);
	fread(&xm_header.channels,    2, 1, fp);
	fread(&xm_header.patterns,    2, 1, fp);
	fread(&xm_header.instruments, 2, 1, fp);
	fread(&xm_header.flags,       2, 1, fp);
	fread(&xm_header.tempo,       2, 1, fp);
 	fread(&xm_header.bpm,         2, 1, fp);
	
	/* this is the index of the highest pattern,
	not the number of patterns as documented.
	ohwell, let's convert. */
	
// #define PRINT_HEADER
#ifdef PRINT_HEADER
	printf("header size: %u\n", xm_header.header_size);
	printf("len:         %u\n", xm_header.len);
	printf("restart pos: %u\n", xm_header.restart_pos);
	printf("channels:    %u\n", xm_header.channels);
	printf("patterns:    %u\n", xm_header.patterns);
	printf("instruments: %u\n", xm_header.instruments);
	printf("flags:       %u\n", xm_header.flags);
	printf("tempo:       %u\n", xm_header.tempo);
	printf("bpm:         %u\n", xm_header.bpm);
#endif
#if 0
	if ((xm_header.flags & 1) == 0) printf("AAAAMIIIIIGAH!\n");
	else printf("LINEAR!!\n");
#endif
	if ((xm_header.flags & 1) == 0)
	{
		mod->use_linear_frequency_table = false;            // behavour flag
		mod->period_low_clamp = 1;
		mod->period_high_clamp = 32767;
		mod->instrument_vibrato_use_linear_frequency_table = false; // behavour flag
	}
	else
	{
		mod->use_linear_frequency_table = true;            // behavour flag
		mod->period_low_clamp = 0;
		mod->period_high_clamp = 10752;
		mod->instrument_vibrato_use_linear_frequency_table = true; // behavour flag
	}
	
	mod->initial_global_volume = 64;
	mod->initial_tempo = xm_header.tempo;
	mod->initial_bpm   = xm_header.bpm;
	
	
	// this is behavour-flags for the player, used to flag differences between MOD, XM, S3M and IT
	mod->volume_slide_in_tick0        = false;
	mod->vibrato_in_tick0             = false;
	mod->vol0_optimizations           = false;
	mod->tremor_extra_delay           = true;
	mod->tremor_has_memory            = true;
	mod->retrig_kills_note            = false;
	mod->note_cut_kills_note          = false;
	mod->allow_nested_loops           = true;
	mod->retrig_note_source_is_period = false;
	mod->delay_global_volume          = false;
	mod->sample_offset_clamp          = false;
	mod->tone_porta_share_memory      = false; // unsure, need to check
	mod->remember_tone_porta_target   = true;
	
	mod->order.resize(xm_header.len);
	fread(&mod->order[0], sizeof(u8), xm_header.len, fp);
	mod->repeat_pos = xm_header.restart_pos;
	
	mod->channels.resize(xm_header.channels);
	
	// setup default pr channel settings.
	for (unsigned i = 0; i < mod->channels.size(); ++i)
	{
		mod->channels[i].default_pan = 127;
		mod->channels[i].initial_volume = 64;
		mod->channels[i].mute_state = CHANNEL_NOT_MUTED;
	}
	
	// seek to start of pattern
	fseek(fp, 60 + xm_header.header_size, SEEK_SET);
	
/*	printf("loading pattern-data..."); */
	mod->patterns.resize(xm_header.patterns);
	
	// load patterns
	for (unsigned p = 0; p < xm_header.patterns; ++p)
	{
		unsigned last_pos = ftell(fp);
		xm_pattern_header_t pattern_header;
		memset(&pattern_header, 0, sizeof(pattern_header));
		
		// load pattern-header
		fread(&pattern_header.header_size,  4, 1, fp);
		fread(&pattern_header.packing_type, 1, 1, fp);
		fread(&pattern_header.rows,         2, 1, fp);
		fread(&pattern_header.data_size,    2, 1, fp);
		
		pattern_header_t *pat = &mod->patterns[p];
		pat->num_rows = pattern_header.rows;
		
//		pat->pattern_data = (pattern_entry_t *)malloc(sizeof(pattern_entry_t) * xm_header.channels * pattern_header.rows);
		pat->pattern_data.resize(xm_header.channels * pattern_header.rows);
//		assert(pat->pattern_data != 0);
//		memset(pat->pattern_data, 0, sizeof(pattern_entry_t) * xm_header.channels * pattern_header.rows);
		
#ifdef PRINT_PATTERNS
		printf("pattern:              %u\n", p);
		printf("pattern-header size:  %u\n", pattern_header.header_size);
		printf("pattern packing type: %u\n", pattern_header.packing_type);
		printf("pattern rows:         %u\n", pattern_header.rows);
		printf("pattern data size:    %u\n", pattern_header.data_size);
#endif

/*
		if (pattern_header.data_size == 0)
		{
			printf("you piece of shit! (go fuck yourself)\n");
			exit(1);
		}
*/
		
		// seek to start of pattern data
		fseek(fp, last_pos + pattern_header.header_size, SEEK_SET);
/*
		mod->instrument_count = ;
		mod->instruments = ;
*/
		// load pattern data
		if (pattern_header.data_size > 0)
		{
			for (unsigned r = 0; r < pattern_header.rows; ++r)
			{
#ifdef PRINT_PATTERNS
				printf("%02X - ", r);
#endif
				for (unsigned n = 0; n < xm_header.channels; ++n)
				{
					unsigned char
						note      = 0,
						instr     = 0,
						vol       = 0,
						eff       = 0,
						eff_param = 0;
					
					fread(&note, 1, 1, fp);
					
					unsigned char pack = 0x1E;
					if (note & (1 << 7))
					{
						pack = note;
						note = 0;
					}
					
					note &= (1 << 7) - 1; // we never need the top-bit.
					
					if (pack & (1 << 0)) fread(&note,      1, 1, fp);
					if (pack & (1 << 1)) fread(&instr,     1, 1, fp);
					if (pack & (1 << 2)) fread(&vol,       1, 1, fp);
					if (pack & (1 << 3)) fread(&eff,       1, 1, fp);
					if (pack & (1 << 4)) fread(&eff_param, 1, 1, fp);
					
					pattern_entry_t *dat = &pat->pattern_data[r * xm_header.channels + n];
					
					if (note == 0x61) dat->note = 121;
					else if (note == 0) dat->note = 0;
					else dat->note = note + 12 * 1;
					
					dat->instrument       = instr;
					dat->volume_command   = vol;
					dat->effect_byte      = eff;
					dat->effect_parameter = eff_param;
					
#ifdef PRINT_PATTERNS
					if ((note) != 0)
					{
						const int o = (note - 1) / 12;
						const int n = (note - 1) % 12;
						/* C, C#, D, D#, E, F, F#, G, G, A, A#, B */
						printf("%c%c%X ",
							"CCDDEFFGGAAB"[n],
							"-#-#--#-#-#-"[n], o);
					}
					else printf("--- ");
					printf("%02X %02X %X%02X\t", instr, vol, eff, eff_param);
#endif
				}
#ifdef PRINT_PATTERNS
				printf("\n");
#endif
			}
		}

		// seek to end of block
		fseek(fp, last_pos + pattern_header.header_size + pattern_header.data_size, SEEK_SET);
	}
/*
	printf("done!\n");
	printf("loading instrument-data...\n");
*/

	// load instruments
	mod->instruments.resize(xm_header.instruments);
	for (unsigned i = 0; i < xm_header.instruments; ++i)
	{
		unsigned last_pos = ftell(fp);
		
		xm_instrument_header_t ih;
		memset(&ih, 0, sizeof(ih));
		
		instrument_t &instr = mod->instruments[i];
		memset(&instr, 0, sizeof(instrument_t));
		
		fread(&ih.header_size,  4,  1, fp);
		fread(&ih.name,         1, 22, fp);
		fread(&ih.type,         1,  1, fp);
		fread(&ih.samples,      2,  1, fp);
		ih.name[22] = '\0';
		
		if (ih.samples != 0)
		{
			unsigned last_pos2 = ftell(fp);
			
			fread(&ih.sample_header_size, 4,  1, fp);
			fread(&ih.sample_number,      1, 96, fp);
			fread(&ih.vol_env,            2, 24, fp);
			fread(&ih.pan_env,            2, 24, fp);
			fread(&ih.vol_env_points,     1,  1, fp);
			fread(&ih.pan_env_points,     1,  1, fp);
			fread(&ih.vol_sustain,        1,  1, fp);
			fread(&ih.vol_loop_start,     1,  1, fp);
			fread(&ih.vol_loop_end,       1,  1, fp);
			fread(&ih.pan_sustain,        1,  1, fp);
			fread(&ih.pan_loop_start,     1,  1, fp);
			fread(&ih.pan_loop_end,       1,  1, fp);
			fread(&ih.vol_type,           1,  1, fp);
			fread(&ih.pan_type,           1,  1, fp);
			fread(&ih.vibrato_type,       1,  1, fp);
			fread(&ih.vibrato_sweep,      1,  1, fp);
			fread(&ih.vibrato_depth,      1,  1, fp);
			fread(&ih.vibrato_rate,       1,  1, fp);
			fread(&ih.volume_fadeout,     2,  1, fp);
		}
		
//		printf("points: %i\n", ih.vol_env_points);
		strcpy(instr.name, ih.name);
		
//		if (ih.vol_env_points > 0)
		if ((ih.vol_type & 1) == 1)
		{
			instr.volume_envelope = (envelope_t*)malloc(sizeof(envelope_t));
			if (NULL == instr.volume_envelope)
			{
				/* TODO: cleanup */
				return NULL;
			}
			
			memset(instr.volume_envelope, 0, sizeof(envelope_t));

			envelope_t *env = instr.volume_envelope;
			env->node_count = ih.vol_env_points;
			for (int i = 0; i < ih.vol_env_points; ++i)
			{
				env->node_tick[i]      = ih.vol_env[i * 2 + 0];
				env->node_magnitude[i] = ih.vol_env[i * 2 + 1];
				
				if (i > 0)
				{
					int diff_mag  = env->node_magnitude[i] - env->node_magnitude[i - 1];
					int diff_tick = env->node_tick[i] - env->node_tick[i - 1];
					
					env->node_delta[i - 1] = (diff_mag << 9) / diff_tick;
					// 7.9 fixedpoint. should this be auto-calculated somewhere for all mod-types instead?
				}
			}
			
			env->loop_enable = false;
			env->sustain_loop_enable = false;
			
			env->sustain_loop_start = env->sustain_loop_end = ih.vol_sustain;
			env->loop_start  = ih.vol_loop_start;
			env->loop_end    = ih.vol_loop_end;
			
			env->sustain_loop_enable = ((ih.vol_type >> 1) & 1) == 1;
			env->loop_enable         = ((ih.vol_type >> 2) & 1) == 1;
		}
		
//		if (ih.pan_env_points > 0)
		if ((ih.pan_type & 1) == 1)
		{
			/* panning envelope */
			instr.panning_envelope = (envelope_t*)malloc(sizeof(envelope_t));
			assert(instr.panning_envelope != 0);
			memset(instr.panning_envelope, 0, sizeof(envelope_t));

			envelope_t *env = instr.panning_envelope;
			env->node_count = ih.pan_env_points;
			for (int i = 0; i < ih.pan_env_points; ++i)
			{
				env->node_tick[i]      = ih.pan_env[i * 2 + 0];
				env->node_magnitude[i] = ih.pan_env[i * 2 + 1];
				
				if (i > 0)
				{
					int diff_mag  = env->node_magnitude[i] - env->node_magnitude[i - 1];
					int diff_tick = env->node_tick[i] - env->node_tick[i - 1];
					
					env->node_delta[i - 1] = (diff_mag << 9) / diff_tick; // 7.9 fixedpoint. should this be auto-calculated somewhere for all mod-types instead?
					
//					printf("pan delta: %i (%i %i)\n", env->node_delta[i - 1], env->node_magnitude[i - 1], env->node_magnitude[i]);
//					printf("pan %i hitpoint: %i\n", i, env->node_magnitude[i - 1] + ((env->node_delta[i - 1] * diff_tick + (1 << 8)) >> 9));
				}
			}
			env->sustain_loop_start = env->sustain_loop_end = ih.pan_sustain;
			env->loop_start = ih.pan_loop_start;
			env->loop_end   = ih.pan_loop_end;
			
			env->sustain_loop_enable = (ih.pan_type & (1 << 1)) == 1;
			env->loop_enable         = (ih.pan_type & (1 << 2)) == 1;
		}
		
		instr.fadeout_rate = ih.volume_fadeout;
		
		/* "emulate" FT2-behavour on IT-fields */
		instr.new_note_action = NNA_CUT;
		instr.duplicate_check_type = DCT_OFF;
		instr.duplicate_check_action = DCA_CUT;
		
		instr.samples.resize(ih.samples);
		
		fseek(fp, ih.header_size - (ftell(fp) - last_pos), SEEK_CUR);
		
		xm_sample_header_t *sample_headers = (xm_sample_header_t *)malloc(sizeof(xm_sample_header_t) * ih.samples);
		assert(sample_headers != 0);
		memset(sample_headers, 0, sizeof(xm_sample_header_t) * ih.samples);
		
		/* all sample headers are stored first, and THEN comes all sample-data. now it's time for them headers */
		for (unsigned s = 0; s < ih.samples; ++s)
		{
			xm_sample_header_t &sh = sample_headers[s];
			
			// load sample-header
			fread(&sh.length,      4,  1, fp);
			fread(&sh.loop_start,  4,  1, fp);
			fread(&sh.loop_length, 4,  1, fp);
			fread(&sh.volume,      1,  1, fp);
			fread(&sh.fine_tune,    1,  1, fp);
			fread(&sh.type,        1,  1, fp);
			fread(&sh.pan,         1,  1, fp);
			fread(&sh.rel_note,    1,  1, fp);
			fseek(fp, 1, SEEK_CUR);
			fread(&sh.name,        1, 22, fp);
			sh.name[22] = '\0';
			
			/* fill converter-struct */
			sample_header_t &samp = instr.samples[s];
			strcpy(samp.name, sh.name);
			
			samp.is_stereo = false; /* no stereo samples in XM */
			switch (sh.type & ((1 << 2) - 1))
			{
				case 0:
					samp.loop_type = LOOP_NONE;
				break;
				case 1:
					samp.loop_type = LOOP_FORWARD;
				break;
				case 2:
					samp.loop_type = LOOP_PINGPONG;
				break;
				default: assert(0);
			}
			
			samp.loop_start = sh.loop_start;
			samp.loop_end   = sh.loop_start + sh.loop_length;
			samp.rel_ptr = -1;
			
			/* IT only */
			samp.sustain_loop_type = LOOP_NONE;
			samp.sustain_loop_start = 0;
			samp.sustain_loop_end = 0;
			
			samp.default_volume = sh.volume;
			samp.default_pan_position = sh.pan;
			samp.ignore_default_pan_position = false; /* allways pan from instrument in XM */
			
			samp.sample_note_offset = sh.rel_note; /* + 12; */
			samp.fine_tune = sh.fine_tune;

			samp.note_period_translation_map_index = 0;
			
			samp.vibrato_speed = ih.vibrato_rate;
			samp.vibrato_depth = ih.vibrato_depth;
			samp.vibrato_sweep = ih.vibrato_sweep;
			
			/* no documentation, this is reversed from MPT */
			switch (ih.vibrato_type)
			{
				case 0:
					samp.vibrato_waveform = SAMPLE_VIBRATO_SINE;
				break;
				case 1:
					samp.vibrato_waveform = SAMPLE_VIBRATO_SQUARE;
				break;
				case 2:
					samp.vibrato_waveform = SAMPLE_VIBRATO_RAMP_UP;
				break;
				case 3:
					samp.vibrato_waveform = SAMPLE_VIBRATO_RAMP_DOWN;
				break;
				case 4:
					samp.vibrato_waveform = SAMPLE_VIBRATO_RANDOM;
				break;
				default: assert(0);
			}
			
			samp.waveform = malloc(sh.length);
			assert(samp.waveform != NULL);
			memset(samp.waveform, 0, sh.length);
			
			if (sh.type & (1 << 4))
			{
				samp.format = SAMPLE_SIGNED_16BIT;
				samp.length = sh.length / 2;
				samp.loop_start /= 2;
				samp.loop_end   /= 2;
			}
			else
			{
				samp.format = SAMPLE_SIGNED_8BIT;
				samp.length = sh.length;
			}
		}
		
		/* now load all samples */
		for (unsigned s = 0; s < ih.samples; ++s)
		{
			xm_sample_header_t &sh = sample_headers[s];
			sample_header_t &samp = instr.samples[s];
			
			if (sh.type & (1 << 4))
			{
				signed short prev = 0;
				for (unsigned i = 0; i < sh.length / 2; ++i)
				{
					signed short data = 0;
					fread(&data, 1, 2, fp);
					prev += data;
					((signed short*)(samp.waveform))[i] = prev;
				}
			}
			else
			{
				signed char prev = 0;
				for (unsigned i = 0; i < sh.length; ++i)
				{
					signed char data = 0;
					fread(&data, 1, 1, fp);
					prev += data;
					((signed char*)samp.waveform)[i] = prev;
				}
			}
		}
		
		for (unsigned i = 0; i < 96; ++i)
		{
			instr.sample_map[i + 12] = ih.sample_number[i]; /* some offset here? */
		}
		
#if 0
		/* some fields left: */
		s8 pitch_pan_separation; // no idea what this one does
		u8 pitch_pan_center; // not this on either; this one seems to be a note index
#endif

	}
/*	printf("done!\n"); */
	
	return mod;
}

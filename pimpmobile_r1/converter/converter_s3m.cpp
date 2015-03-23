/* converter_s3m.cpp -- ScreamTracker S3M converting code
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "converter.h"

module_t *load_module_s3m(FILE *fp)
{
	
	/* check file-signature */
	char sig[4];
	fseek(fp, 0x2C, SEEK_SET);
	fread(sig, 1, 4, fp);
	if (memcmp(sig, "SCRM", 4) != 0) return NULL;
	
	/* check a second signature. */
	unsigned char sig2;
	fseek(fp, 0x1C, SEEK_SET);
	fread(&sig2, 1, 1, fp);
	if (sig2 != 0x1A) return NULL;
	
	/* check that the type is correct. just another sanity-check. */
	unsigned char type;
	fread(&type, 1, 1, fp);
	if (type != 16) return NULL;

	printf("S3M not yet supported, sorry!\n");
	return NULL;
	
	printf("huston, we have s3m.\n");
	
	module_t *mod = (module_t *)malloc(sizeof(module_t));
	if (mod == 0)
	{
		printf("out of memory\n");
		fclose(fp);
		exit(1);		
	}
	memset(mod, 0, sizeof(module_t));
	
	mod->use_linear_frequency_table = false;
	mod->instrument_vibrato_use_linear_frequency_table = false;
	mod->volume_slide_in_tick0        = false;
	mod->vibrato_in_tick0             = false;
	mod->tremor_extra_delay           = false;
	mod->tremor_has_memory            = false;
	mod->retrig_kills_note            = true;
	mod->note_cut_kills_note          = true;
	mod->allow_nested_loops           = false;
	mod->retrig_note_source_is_period = true;
	mod->delay_global_volume          = true;
	mod->sample_offset_clamp          = false; // unsure, need to check
	mod->tone_porta_share_memory      = false; // unsure, need to check
	mod->remember_tone_porta_target   = false;
	
	/* load module-name */
	char name[28 + 1];
	fseek(fp, 0x0, SEEK_SET);
	fread(&name, 1, 28, fp);
	name[28] = '\0';
	strcpy(mod->name, name);
	
	printf("it's called \"%s\"\n", mod->name);
	
	unsigned short ordnum, insnum, patnum, flags;
	fseek(fp, 0x20, SEEK_SET);
	fread(&ordnum, 1, 2, fp);
	fread(&insnum, 1, 2, fp);
	fread(&patnum, 1, 2, fp);
	fread(&flags,  1, 2, fp);
	printf("orders: %d\ninstruments: %d\npatterns: %d\nflags: %x\n", ordnum, insnum, patnum, flags);
	fseek(fp, 0x1, SEEK_CUR); /* skip tracker version */
	
	mod->order.resize(ordnum);
	mod->patterns.resize(patnum);
	mod->instruments.resize(insnum);
	
	mod->vol0_optimizations         = (flags & 8) != 0;
	
	if (flags & 16) // amiga-limits
	{
		printf("AMIGAH limits.\n");
		mod->period_low_clamp  = 113; // linear period is wrong (not the amiga-limit, but rather the xm-limit)
		mod->period_high_clamp = 856; // linear period is wrong (not the amiga-limit, but rather the xm-limit)
	}
	else // no amiga-limits
	{
		mod->period_low_clamp  = 128;   // fist value is linear, second is amiga
		mod->period_high_clamp = 32767; // fist value is linear, second is amiga
	}

	fseek(fp, 0x40, SEEK_SET);
	int chans = 0;

	unsigned enable_map = 0;
	assert(sizeof(enable_map) >= 4);
	
	for (int i = 0; i < 32; ++i)
	{
		char state;
		fread(&state, 1, 1, fp);
		if (0 == (state & 1 << 8))
		{
			chans++;
			enable_map |= 1 << i;
		}
		printf("%02x ", state);
	}
	printf("\n%d active channels\n", chans);
	
//	mod->num_channels = ;                 /* number of channels in the pattern-data of the module. */
/*
	u8  play_order_repeat_position;
	u8  *play_order;
*/
	
	mod->initial_global_volume        = 64;
	mod->initial_tempo                = 6;
	mod->initial_bpm                  = 125;

	
	/* todo: load more */
	
	return mod;
}

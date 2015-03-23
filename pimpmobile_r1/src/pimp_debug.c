/* pimp_debug.c -- Debugging helpers
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include <stdio.h>
#include "pimp_internal.h"
#include "pimp_debug.h"

#if 0
void print_pattern_entry(const pimp_pattern_entry *pe)
{
	ASSERT(pe != NULL);
	
	if (pe->note != 0)
	{
		const int o = (pe->note - 1) / 12;
		const int n = (pe->note - 1) % 12;
		/* C, C#, D, D#, E, F, F#, G, G, A, A#, B */
		iprintf("%c%c%X ",
			"CCDDEFFGGAAB"[n],
			"-#-#--#-#-#-"[n], o);
	}
	else iprintf("--- ");

/*	iprintf("%02X ", pe->volume_command);
	iprintf("%02X ", pe->effect_byte); */
	iprintf("%02X %02X %X%02X\t", pe->instrument, pe->volume_command, pe->effect_byte, pe->effect_parameter);
}

void print_pattern(const pimp_module *mod, pimp_pattern *pat)
{
	pimp_pattern_entry *pd = get_pattern_data(pat);

	iprintf("row count: %02x\n", pat->row_count);
	
	for (unsigned i = 0; i < 5; ++i)
	{
		for (unsigned j = 0; j < 4; ++j)
		{
			pimp_pattern_entry *pe = &pd[i * mod->channel_count + j];
			print_pattern_entry(pe);
		}
		iprintf("\n");
	}
}

#endif

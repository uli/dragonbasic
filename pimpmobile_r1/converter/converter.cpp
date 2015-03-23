/* converter.cpp -- Pimpmobile converter toplevel code
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */
 
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <string>

#include "converter.h"

void print_usage()
{
	printf("usage: converter filenames\n");
	exit(1);
}

float normal_noise()
{
	float r = 0.0;
	for (unsigned j = 0; j < 12; ++j)
	{
		r += rand();
	}
	r /= RAND_MAX;
	r -= 6;
	return 0.f;
	return r;
}

/* converts a sample to u8-format. yes, this might be the clumsiest routine for this purpose ever written, who cares? */
void convert_sample(sample_header_t *samp)
{
	assert(samp != 0);
	
	// no conversion needed
	if (samp->format == SAMPLE_UNSIGNED_8BIT) return;
	
	/* in case there's an empty sample... (this should really remove the sample, but hey) */
	if (samp->waveform == 0 && samp->length == 0)
	{
		samp->format = SAMPLE_UNSIGNED_8BIT;
		return;
	}
	
	assert(samp->waveform != 0);
	// we need a new buffer
	u8 *new_data = (u8*)malloc(sizeof(u8) * samp->length);
	assert(new_data != 0);
	
	// go get some data, moran!
	for (unsigned i = 0; i < samp->length; ++i)
	{
		s32 new_sample = 0;
		
		// fetch data, and get it signed.
		switch (samp->format)
		{
			case SAMPLE_SIGNED_8BIT:    new_sample = (s32)((s8*)samp->waveform)[i];                break;
			case SAMPLE_UNSIGNED_16BIT: new_sample = ((s32)((u16*)samp->waveform)[i]) - (1 << 16); break;
			case SAMPLE_SIGNED_16BIT:   new_sample = ((s32)((s16*)samp->waveform)[i]);             break;
			default: assert(false); // should not happen. ever. u8-samples are dealt with earlier.
		}
		
		// downsample
		if (samp->format == SAMPLE_UNSIGNED_16BIT || samp->format == SAMPLE_SIGNED_16BIT)
		{
#if 0
			// dither, proven more or less to not be a good idea
			new_sample = int(float(new_sample) + normal_noise() * (2.0 / 3) * (1 << 7)) >> 8;
			if (new_sample >  127) new_sample =  127;
			if (new_sample < -128) new_sample = -128;
#else
			new_sample >>= 8;
#endif
		}
		
		// make unsigned
		new_sample += 128;
		
		// write back
		new_data[i] = (u8)new_sample;
	}
	
	// swap data
	free(samp->waveform);
	samp->waveform = new_data;
	samp->format = SAMPLE_UNSIGNED_8BIT;
}

/* converts all samples to u8-format. */
void convert_samples(module_t *mod)
{
	assert(mod != 0);
	for (unsigned i = 0; i < mod->instruments.size(); ++i)
	{
		instrument_t &instr = mod->instruments[i];
		
		for (unsigned s = 0; s < instr.samples.size(); ++s)
		{
			sample_header_t &samp = instr.samples[s];
			convert_sample(&samp);
#if 0
			if (i == 0)
			{
				printf("converting sample: '%s'\n", samp.name);
				FILE *fp = fopen("sample.raw", "wb");
				assert(fp != 0);
				fwrite(samp.waveform, sizeof(u8), samp.length, fp);
				fclose(fp);
			}
#endif
		}
	}
}

void print_pattern(module_t *mod, pattern_header_t &pat)
{
	for (unsigned i = 0; i < pat.num_rows; ++i)
	{
		for (unsigned j = 0; j < mod->channels.size(); ++j)
		{
			pattern_entry_t &pe = pat.pattern_data[i * mod->channels.size() + j];
			
			if (pe.note != 0)
			{
				const int o = (pe.note - 1) / 12;
				const int n = (pe.note - 1) % 12;
				/* C, C#, D, D#, E, F, F#, G, G, A, A#, B */
				printf("%c%c%X ",
					"CCDDEFFGGAAB"[n],
					"-#-#--#-#-#-"[n], o);
			}
			else printf("--- ");
			
			printf("%02X %02X %X%02X\t", pe.instrument, pe.volume_command, pe.effect_byte, pe.effect_parameter);
		}
		printf("\n");
	}
}

void print_patterns(module_t *mod)
{
	for (unsigned p = 0; p < mod->patterns.size(); ++p)
	{
		pattern_header_t &pat = mod->patterns[p];
//		assert(pat.pattern_data != NULL);
		print_pattern(mod, pat);
		printf("\n");
	}
}

int main(int argc, char *argv[])
{
	
	if (argc < 2) print_usage();
	
	for (int i = 1; i < argc; ++i)
	{
		std::string filename = argv[i];
		
		printf("loading module: %s\n", filename.c_str());
		FILE *fp = fopen(argv[i], "rb");
		if (!fp) print_usage();
		
		module_t *mod = load_module_xm(fp);
		if (!mod) mod = load_module_s3m(fp);
		if (!mod) mod = load_module_mod(fp);
		if (!mod)
		{
			printf("failed to load!\n");
			exit(1);
		}
		
		fclose(fp);
/*		printf("done loading\n"); */
		
/*		printf("converting samples\n"); */
		convert_samples(mod); // convert all samples to unsigned 8bit format

/*		printf("dumping samples\n"); */
		dump_samples(mod); // find any duplicate samples, and merge them
		
//		print_patterns(mod);
#if 0
		size_t period_pos = filename.rfind(".") + 1;
		filename.replace(period_pos, filename.size() - period_pos, "bin");
#endif
		filename.append(".bin");
		dump_module(mod, filename.c_str());
	}
	write_sample_dump("sample_bank.bin"); // dump all samples to file
	return 0;
}

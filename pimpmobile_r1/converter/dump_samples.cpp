/* dump_samples.cpp -- Sample data dumper
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <map>

#include "converter.h"
#include "../src/pimp_internal.h"

#include <vector>

using std::map;
using std::vector;
using std::make_pair;

struct dump_sample
{
	dump_sample(const sample_header_t &sh)
	{
		data.resize(sh.length);
		
		for (unsigned i = 0; i < sh.length; ++i)
		{
			data[i] = ((u8*)sh.waveform)[i];
		}
	}

	bool operator<(const dump_sample &s) const
	{
		return data < s.data;
	}
	
	vector<u8> data;
};

map<dump_sample, int> samples;
int size = 0;

/* sample dumper */
void dump_samples(module_t *mod)
{
	assert(mod != 0);
	for (unsigned i = 0; i < mod->instruments.size(); ++i)
	{
		instrument_t &instr = mod->instruments[i];
		
		for (unsigned s = 0; s < instr.samples.size(); ++s)
		{
			sample_header_t &samp = instr.samples[s];
			dump_sample dsamp = samp;
			
			if (samples.count(dsamp) == 0)
			{
				samples.insert(make_pair(dsamp, size));
				size += dsamp.data.size();
			}
			else
			{
				if (0) printf("matched sample: %d\n", samples[dsamp]);
			}
			
			// the pointer relative to the start of the sample bank
			samp.rel_ptr = samples[dsamp];
		}
	}
}

void write_sample_dump(const char *filename)
{
	std::vector<u8> data;
	data.resize(size);
	
	for (map<dump_sample, int>::iterator it = samples.begin(); it != samples.end(); ++it)
	{
		for (unsigned i = 0; i < it->first.data.size(); ++i)
		{
			data[it->second + i] = it->first.data[i];
		}
	}

	FILE *fp = fopen(filename, "wb");
	if (!fp)
	{
		printf("error: failed to open output-file\n");
		exit(1);
	}

	printf("sample bank data size: %i bytes\n", data.size());
	
	fwrite(&data[0], data.size(), 1, fp);
	fclose(fp);
}

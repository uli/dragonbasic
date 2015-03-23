#!/usr/bin/python
# -*- coding: latin-1 -*-
# lut_gen.cpp -- Look-up table generator for Pimpmobile
# Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
# For conditions of distribution and use, see copyright notice in LICENSE.TXT

AMIGA_DELTA_LUT_LOG2_SIZE = 7
AMIGA_DELTA_LUT_SIZE = 1 << AMIGA_DELTA_LUT_LOG2_SIZE
AMIGA_DELTA_LUT_FRAC_BITS = 15 - AMIGA_DELTA_LUT_LOG2_SIZE

def print_lut_to_file(file, lutname, lut):
	max_elem = reduce(max, lut)
	min_elem = reduce(min, lut)
	assert(min_elem >= 0)
	assert(max_elem < (1 << 16))
	file.write('const u16 %s[%d] =\n{\n\t' % (lutname, len(lut)))
	line_start = file.tell()
	for e in lut:
		file.write('%d, ' % e)
		if file.tell() > (line_start + 80):
			file.write('\n\t')
			line_start = file.tell()
	file.write('\n};\n\n')


AMIGA_C4_FREQ        = 8363.0
AMIGA_C4_PERIOD      = 1712
NOTES_PER_OCTAVE     = 12
FINETUNES_PER_NOTE   = 64
FINETUNES_PER_OCTAVE = NOTES_PER_OCTAVE * FINETUNES_PER_NOTE

def linear_delta_entry(note):
	temp = 2.0 ** (float(note) / FINETUNES_PER_OCTAVE)
	return int((temp * AMIGA_C4_FREQ) * 2 + 0.5)

def amiga_delta_entry(fine_note):
	temp = float(((fine_note + (AMIGA_DELTA_LUT_SIZE / 2)) * (1 << 15)) / AMIGA_DELTA_LUT_SIZE)
	return int(((AMIGA_C4_FREQ * AMIGA_C4_PERIOD) / temp) * (1 << 6) + 0.5)

def gen_linear_delta_lut():
	return [linear_delta_entry(i) for i in range(0, FINETUNES_PER_OCTAVE)]

def gen_amiga_delta_lut():
	return [amiga_delta_entry(i) for i in range(0, (AMIGA_DELTA_LUT_SIZE / 2) + 1)]

def dump_linear_lut(filename):
	linear_delta_lut = gen_linear_delta_lut()
	f = open(filename, 'w')
	print_lut_to_file(f, 'linear_delta_lut', linear_delta_lut)
	f.close()

def dump_amiga_lut(filename):
	amiga_delta_lut = gen_amiga_delta_lut()
	f = open(filename, 'w')
	f.write('#define AMIGA_DELTA_LUT_LOG2_SIZE %d\n' % (AMIGA_DELTA_LUT_LOG2_SIZE))
	print_lut_to_file(f, 'amiga_delta_lut', amiga_delta_lut)
	f.close()

dump_linear_lut('src/linear_delta_lut.h')
dump_amiga_lut('src/amiga_delta_lut.h')

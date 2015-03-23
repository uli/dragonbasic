# Makefile for pimpmobile module player file converter
# Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
# For conditions of distribution and use, see copyright notice in LICENSE.TXT

CC      = gcc
CXX     = g++
STRIP   = strip
LD      = g++
AS      = as

CPPFLAGS = 
CFLAGS   = 
CXXFLAGS = 
LDFLAGS  = 
ASFLAGS  = 

OBJS = \
	converter.o \
	converter_xm.o \
	converter_s3m.o \
	converter_mod.o \
	dump_module.o \
	dump_samples.o

TARGET = ../bin/converter

ifeq ($(DEBUG), 1)
	CPPFLAGS += -DDEBUG
	CXXFLAGS += -g3 -ggdb
	CFLAGS   += -g3 -ggdb
else
	CPPFLAGS += -DRELEASE -DNDEBUG
	CXXFLAGS += -O3 -fomit-frame-pointer
	CFLAGS   += -O3 -fomit-frame-pointer
endif
	
.PHONY: all clean

all: $(TARGET)

clean:
	$(RM) $(TARGET) $(OBJS) $(OBJS:.o=.d) *~

$(TARGET): $(OBJS)
	$(LD) $^ $(LDFLAGS) -o $@
	
converter2: dump_module2.o dump.o
	$(LD) $^ $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@ -MMD -MP -MF $(@:.o=.d)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@ -MMD -MP -MF $(@:.o=.d)

-include $(OBJS:.o=.d)

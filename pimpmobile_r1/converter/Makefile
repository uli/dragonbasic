# Makefile for pimpmobile module player file converter
# Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
# For conditions of distribution and use, see copyright notice in LICENSE.TXT

CC      = gcc
CXX     = g++
STRIP   = strip
LD      = g++
AS      = as

CPPFLAGS = 
CFLAGS   = -Wall
CXXFLAGS = -Wall
LDFLAGS  = 
ASFLAGS  = 

OBJS = \
	converter.o.$(PLATFORM) \
	converter_xm.o.$(PLATFORM) \
	converter_s3m.o.$(PLATFORM) \
	converter_mod.o.$(PLATFORM) \
	dump_module.o.$(PLATFORM) \
	dump_samples.o.$(PLATFORM)

TARGET = ../bin/converter

ifeq ($(DEBUG), 1)
	CPPFLAGS += -DDEBUG
	CXXFLAGS += -g3 -ggdb
	CFLAGS   += -g3 -ggdb
else
	CPPFLAGS += -DRELEASE -DNDEBUG
	CXXFLAGS += -O3 -g
	CFLAGS   += -O3 -g
endif
	
.PHONY: all clean

all: $(TARGET)

clean:
	$(RM) $(TARGET) converter converter.exe *.o.* $(OBJS:.o.$(PLATFORM)=.d) *~

$(TARGET): $(OBJS)
	$(LD) $^ $(LDFLAGS) -o $@
	
converter2: dump_module2.o.$(PLATFORM) dump.o.$(PLATFORM)
	$(LD) $^ $(LDFLAGS) -o $@

%.o.$(PLATFORM): %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@ -MMD -MP -MF $(@:.o.$(PLATFORM)=.d)

%.o.$(PLATFORM): %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@ -MMD -MP -MF $(@:.o.$(PLATFORM)=.d)

-include $(OBJS:.o.$(PLATFORM)=.d)

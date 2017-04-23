# Makefile for pimpmobile module player
# Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
# For conditions of distribution and use, see copyright notice in LICENSE.TXT

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
endif

DEVKITARM = $(DEVKITPRO)/devkitARM
LIBGBA    = $(DEVKITPRO)/libgba

PREFIX ?= $(DEVKITARM)/bin/arm-none-eabi-
CC      = $(PREFIX)gcc
CXX     = $(PREFIX)g++
OBJCOPY = $(PREFIX)objcopy
STRIP   = $(PREFIX)strip
LD      = $(PREFIX)g++
AS      = $(PREFIX)as
AR      = $(PREFIX)ar

CPPFLAGS = -I$(DEVKITARM)/include -I$(LIBGBA)/include
CFLAGS   = -mthumb-interwork -mlong-calls
CXXFLAGS = -mthumb-interwork -mlong-calls -fconserve-space -fno-rtti -fno-exceptions
LDFLAGS  = -mthumb-interwork -Wl,--gc-section -Wl,-Map,$(basename $@).map
ASFLAGS  = -mthumb-interwork

ARM   = -marm
THUMB = -mthumb

# TODO: profile what code to put where
OBJS = \
	src/pimp_gba.o         \
	src/pimp_render.o      \
	src/pimp_envelope.o    \
	src/pimp_debug.o       \
	src/pimp_mod_context.o \
	src/pimp_math.iwram.o  \
	src/pimp_mixer.iwram.o

ifeq ($(DEBUG), 1)
	CPPFLAGS += -DDEBUG
	CXXFLAGS += -g3 -ggdb
	CFLAGS   += -g3 -ggdb
	OBJS     += src/pimp_mixer_portable.o
else
	CPPFLAGS += -DRELEASE -DNDEBUG
	CXXFLAGS += -O3 -fomit-frame-pointer
	CFLAGS   += -O3 -fomit-frame-pointer
#	OBJS     += src/pimp_mixer_portable.iwram.o
	OBJS     += src/pimp_mixer_arm.o
	OBJS     += src/pimp_mixer_clip_arm.o
endif

	
.PHONY: all clean run debug

all: bin/example.bin bin/example.gba

bin/example.gba::
	make -C example

bin/example.bin::
	make -C example ../bin/example.bin

clean:
	$(RM) bin/* $(OBJS) $(OBJS:.o=.d) lib/libpimpmobile.a
	make -C converter clean
	make -C example clean

run:
	make -C example run

debug:
	make -C example debug

bin/converter:
	make -C converter

bin/lut_gen: lut_gen.cpp src/pimp_math.c src/pimp_config.h
	g++ $^ -o $@

lib/libpimpmobile.a: $(OBJS)

%.a:
	$(AR) $(ARFLAGS) $@ $?

%.iwram.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ARM) -c $< -o $@ -MMD -MP -MF $(@:.o=.d)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(THUMB) -c $< -o $@ -MMD -MP -MF $(@:.o=.d)

%.iwram.s: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -S -fverbose-asm $(ARM) -c $< -o $@

%.s: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -S -fverbose-asm $(THUMB) -c $< -o $@

%.iwram.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARM) -c $< -o $@ -MMD -MP -MF $(@:.o=.d)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(THUMB) -c $< -o $@ -MMD -MP -MF $(@:.o=.d)

-include $(OBJS:.o=.d)

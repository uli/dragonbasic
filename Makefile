VERSION = 2.0

DEVKITPRO ?= $(HOME)/devkitPro
ifeq ($(PLATFORM), win32)
ifeq ($(CROSS),)
DEVKITPRO := /c/devkitPro
endif
endif

DEVKITARM ?= $(DEVKITPRO)/devkitARM

PIMPMOBILE = ./pimpmobile_r1

# only used on Linux
APPDIR ?= $(shell pwd)

CXXFLAGS = -g -DDB_VERSION="\"$(VERSION)\"" -DAPPDIR="\"$(APPDIR)\"" -Wall
AFLAGS	 = -Iruntime

# options
#CXXFLAGS += -DNDEBUG
CXXFLAGS += -O2
CXXFLAGS += -DBUG_FOR_BUG

CXX = $(CROSS)g++

ARMCC = $(DEVKITARM)/bin/arm-none-eabi-gcc
ARMOBJCOPY = $(DEVKITARM)/bin/arm-none-eabi-objcopy
CRTDIR=$(DEVKITPRO)/devkitARM/lib/gcc/arm-none-eabi/$(shell $(ARMCC) -dumpversion)

LIBS = -lsndfile -lfreeimage
ifeq ($(PLATFORM), win32)
SUFF = .exe
else
SUFF =
endif

BOBJS = DBC.o.$(PLATFORM)
MOBJS = MF.o.$(PLATFORM) MF_mappy.o.$(PLATFORM)

all: win_cross linux runtime.gba runpimp.gba
	$(MAKE) -C examples

win_cross:
	$(MAKE) _all CROSS=i686-w64-mingw32- PLATFORM=win32
linux:
	if test `uname -s` = Linux ; then $(MAKE) _all PLATFORM=linux ; fi

_all:	dbc$(SUFF) mf$(SUFF) converter$(SUFF)

clean:
	rm -f dbc.exe dbc mf.exe mf *.o.* run*.gba run*.elf runtime_syms_*.h
	rm -f converter converter.exe
	rm -f dbapi/dbapi.chm docs/dbapi.chm
	$(MAKE) -C $(PIMPMOBILE) clean
	$(MAKE) -C examples clean

doc:	dbapi/dbapi.chm
	cp -p dbapi/dbapi.chm docs/

dbapi/dbapi.chm: dbapi/dbapi.hhp dbapi/dbapi.hhc dbapi/*.htm
	cd dbapi ; chmcmd dbapi.hhp
	rm -f dbapi/bla.something	# WTF?
	chmod 644 dbapi/dbapi.chm

mf$(SUFF): $(MOBJS)
	$(CXX) -o $@ $(MOBJS) $(LDFLAGS) $(LIBS)

dbc$(SUFF): $(BOBJS)
	$(CXX) -o $@ DBC.o.$(PLATFORM) $(LDFLAGS)

DBC.o.$(PLATFORM): DBC.h os.h
MF.o.$(PLATFORM): os.h runtime_syms_pimp.h runtime_syms_rt.h MF.h

$(BOBJS): %.o.$(PLATFORM): %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@
$(MOBJS): %.o.$(PLATFORM): %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

runtime.elf: runtime/runtime.s runtime/runtime_common.s
	$(ARMCC) $(AFLAGS) -Ttext=0x8000000 -nostdlib $< -o runtime.elf
runtime.gba: runtime.elf
	$(ARMOBJCOPY) -O binary $< $@

runtime_syms_pimp.h: runpimp.elf extract_syms.sh
	./extract_syms.sh $< RP >$@
runtime_syms_rt.h: runtime.elf extract_syms.sh
	./extract_syms.sh $< RT >$@

runpimp.elf: runtime/runpimp.c runtime/runtime_common.s runtime/gba_crt0.s runtime/gba_cart.ld $(PIMPMOBILE)/lib/libpimpmobile.a
	$(ARMCC) $(AFLAGS) -nostdlib -I$(DEVKITPRO)/libgba/include \
	-I$(PIMPMOBILE)/include -marm -T runtime/gba_cart.ld runtime/gba_crt0.s $(CRTDIR)/crti.o \
	$(CRTDIR)/crtbegin.o $< $(PIMPMOBILE)/lib/libpimpmobile.a -L \
	$(DEVKITPRO)/libgba/lib -lgcc -lsysbase -lc -lgba $(CRTDIR)/crtend.o \
	$(CRTDIR)/crtn.o -o $@
runpimp.gba: runpimp.elf
	$(ARMOBJCOPY) -O binary $< $@

converter$(SUFF):
	$(MAKE) -C $(PIMPMOBILE)/converter TARGET=$@ CXX=$(CXX) LD=$(CXX)
	cp -p $(PIMPMOBILE)/converter/converter$(SUFF) .
$(PIMPMOBILE)/lib/libpimpmobile.a:
	$(MAKE) -C $(PIMPMOBILE) lib/libpimpmobile.a

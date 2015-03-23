DEVKITPRO ?= $(HOME)/devkitPro
PIMPMOBILE = ./pimpmobile_r1

# only used on Linux
APPDIR ?= $(shell pwd)

CXXFLAGS = -g -DAPPDIR="\"$(APPDIR)\"" -Wall
AFLAGS	 = -Iruntime

# options
CXXFLAGS += -DNDEBUG
CXXFLAGS += -O2
CXXFLAGS += -DBUG_FOR_BUG

CXX = $(CROSS)g++

ARMCC = arm-none-eabi-gcc
CRTDIR=$(DEVKITPRO)/devkitARM/lib/gcc/arm-none-eabi/$(shell $(ARMCC) -dumpversion)

LIBS = -lsndfile
ifeq ($(PLATFORM), win32)
SUFF = .exe
LDFLAGS = -static
LIBS += -lFreeImage
else
SUFF =
LIBS += -lfreeimage
endif

BOBJS = DBC.o.$(PLATFORM)
MOBJS = MF.o.$(PLATFORM) MF_mappy.o.$(PLATFORM)

all: win linux runtime.gba runpimp.gba

win:
	$(MAKE) _all CROSS=i686-pc-mingw32- PLATFORM=win32
linux:
	$(MAKE) _all PLATFORM=linux

_all:	dbc$(SUFF) mf$(SUFF)

clean:
	rm -f dbc.exe dbc mf.exe mf *.o.* run*.gba run*.elf runtime_syms.h
	$(MAKE) -C $(PIMPMOBILE) clean

mf$(SUFF): $(MOBJS)
	$(CXX) -o $@ $(MOBJS) $(LDFLAGS) $(LIBS)

dbc$(SUFF): $(BOBJS)
	$(CXX) -o $@ DBC.o.$(PLATFORM) $(LDFLAGS)

DBC.o.$(PLATFORM): DBC.h os.h
MF.o.$(PLATFORM): os.h runtime_syms.h MF.h

$(BOBJS): %.o.$(PLATFORM): %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@
$(MOBJS): %.o.$(PLATFORM): %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

runtime.elf: runtime/runtime.s
	$(ARMCC) $(AFLAGS) -Ttext=0x8000000 -nostdlib $< -o runtime.elf
runtime.gba: runtime.elf
	objcopy -O binary $< $@

runtime_syms.h: runpimp.elf extract_syms.sh
	./extract_syms.sh $< >$@

runpimp.elf: runtime/runpimp.c runtime/gba_crt0.s runtime/gba_cart.ld $(PIMPMOBILE)/lib/libpimpmobile.a
	$(ARMCC) $(AFLAGS) -nostdlib -I$(DEVKITPRO)/libgba/include \
	-I$(PIMPMOBILE)/include -marm -T runtime/gba_cart.ld runtime/gba_crt0.s $(CRTDIR)/crti.o \
	$(CRTDIR)/crtbegin.o $< $(PIMPMOBILE)/lib/libpimpmobile.a -L \
	$(DEVKITPRO)/libgba/lib -lgcc -lsysbase -lc -lgba $(CRTDIR)/crtend.o \
	$(CRTDIR)/crtn.o -o $@
runpimp.gba: runpimp.elf
	objcopy -O binary $< $@

$(PIMPMOBILE)/lib/libpimpmobile.a:
	$(MAKE) -C $(PIMPMOBILE) lib/libpimpmobile.a

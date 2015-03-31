# Dragon BASIC

## Introduction

This is an open-source recreation of Dragon BASIC, a BASIC software
development kit for the Game Boy Advance written by Jeff Massung.

It consists of a BASIC compiler reconstructed from the last released binary,
and a completely rewritten compiler backend and libraries.  All the while it
remains (almost) fully compatible with legacy Dragon BASIC 1.4.x.

The system produces smaller and much faster code thanks to the following
improvements:

### MF

- uses literal pools instead of accessor functions
- generates Thumb instead of ARM code (all library functions have also been
  rewritten in Thumb)
- supports "naked" words that omit the prolog, reducing function call
  overhead
- optimizes many common TIN word sequences into more efficient machine code
- optimizes many operations on constants
- uses inlined comparisons and conditional branches
- runtime helper functions have been eliminated entirely

### DBC

- produces more easily optimized TIN code
- uses function arguments in place instead of copying them


Numerous bugs have been fixed in the runtime library and the BASIC compiler,
and a lot of new features have been added:

### Assembler

- support for Thumb instruction set
- pseudo-instruction MOVI simplifies constant loading
- support for forward references (no more need to use #offset)

### Asset handling

- support for many new image and sound formats using FreeImage and
  libsndfile
- support for MOD and XM music files using pimpmobile
- support for loading Mappy FMP files (supports 8x8 or 16x16 tiles and
  layers, but not animations)

### BASIC Language

- addition of local variables (which are, incidentally, much faster than
  globals, so use them!)
- byte and halfword-sized DATA and READ commands
- support for a default include path; no need to state the full path anymore
- allows leading underscores in identifiers

### Debugging

- "-sym" option creates a NO$GBA-compatible symbol file for
  debugging/profiling
- "-debug" option produces an assembly listing

### Documentation

- addition of many tutorials and sample programs from archeological digs at
  the Internet Archive and living fossil web sites.
- addition of Dragon BASIC language reference and timer API documentation to
  CHM file (from DDLullu's fix library)

### Library

- new 32 and 8-bit peeks and pokes
- uses BIOS call to wait for vertical blank (reduces power consumption, and
  works better overall)
- changed LOADPAL256, LOADSPRITE, LOADTILES, and UPDATESPRITES to use DMA
- uses byte-sized accesses where possible (there seems to have been some sort
  of superstition that it is not...)
- moved constants together with the corresponding functions to avoid having
  to include the entire library all the time
- new ASC() and CHR$() functions from DDLullu's fix library
- signed numbers support in STR$()

### Runtime Environment

- possibility to link C libraries into the TIN runtime (used to integrate
  pimpmobile)
- sets WAITCNT register to a sane value to improve memory access timing


Oh, and there is no size limit... :)

## Building

### Requirements

You need:

- Some sort of C++ compiler for your host system. (MinGW should work for
  Windows users.)
- The FreeImage library.
- The libsndfile library.
- devkitPro to build the runtime environment.
- Free Pascal to build the documentation CHM file. (Dragon BASIC
  traditionally uses documentation in CHM format, and Free Pascal contains
  the only open-source CHM compiler that I am aware of.)

### For POSIX platforms

Run `make linux`.

### For Win32 platforms

Run `make win`. (untested)

### For my own system

Run `make` to build a native Linux version and a cross-compiled Win32
version.

### Building the documentation

Run `make doc`.

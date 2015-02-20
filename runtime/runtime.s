@ runtime.s: Legacy DragonBASIC runtime identical to the last MF/TIN version
@ released by Jeff Massung.

.text

.global _start
_start:
	b _entry

.include "cart_hdr.s"

@ runtime entry point
.org 0xc0
_entry:
	b _tin_entry

.include "runtime_common.s"

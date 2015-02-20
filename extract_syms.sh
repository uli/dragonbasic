#!/bin/sh
objdump -x "$1" | \
egrep '^0[238]...... g.*(text|init|\*ABS\*)......... [^.]'| \
sed 's,\(........\) g.*............. \(.*\)$,#define RT_\2\t0x\1,'



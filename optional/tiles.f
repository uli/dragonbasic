{ -- MF/TIN tile library
  -- Copyright (c) 2003 by Jeff Massung }

\ return the VRAM address of tile data (inlined)
icode-thumb charblock ( n -- a )
	$60 ## v2 mov,
	20 ## v2 v2 lsl,
	14 ## tos tos lsl,
	v2 tos tos add,
end-code

\ return the VRAM address of map data (inlined)
icode-thumb screenblock ( n -- a )
	VRAM w movi
	11 ## tos tos lsl,
	w tos tos add,
end-code

\ copy ROM data to VRAM tile data
: loadtiles ( charblock offset from blocks -- )
	3 # n* a! push swap charblock swap 5 # n* + pop a@ dmacopy ;

\ copy a font in ROM data to a character block
: loadfont16 ( charblock a -- ) 32 # swap 95 # loadtiles ;
: loadfont256 ( charblock a -- ) 64 # swap 190 # loadtiles ;

\ return address of tile on screenblock
code-thumb tile ( screen x y -- a )
	v1 v2 pop
	
	\ get address of tile
	5 ## tos tos lsl,
	tos v1 w add,
	1 ## w w lsl,
	11 ## v2 v2 lsl,
	v2 w w add,
	$60 ## v1 mov,
	20 ## v1 v1 lsl,	\ VRAM
	v1 w tos add,
	
	\ done
	ret
end-code

\ returns the index of a tile (inlined)
icode-thumb gettile ( a -- u )
	tos 0@ tos ldrh,
	$fc00 v0 movi
	v0 tos bic,
end-code

\ sets the index of a tile
code-thumb settile ( a u -- )
	w pop
	
	\ get mask and set data
	w 0@ v0 ldrh,
	$f ## v1 mov,
	12 ## v1 v1 lsl,
	v1 v0 and,
	tos v0 v0 add,
	w 0@ v0 strh,
	
	\ done
	tos pop
	ret
end-code

\ sets the index of a tile without preserving palette number
icode-thumb settilefast ( a u -- )
	w pop
	w 0@ tos strh,
	tos pop
end-code

\ enable a background layer
code-thumb enabletiles ( bg screen char flags -- )
	v0 v1 v2 pop
	$40 ## w mov,
	20 ## w w lsl,	\ REGISTERS

	\ construct REG_BGxCNT
	2 ## v0 v0 lsl,
	v0 tos tos add,		\ charblock
	8 ## v1 v1 lsl,
	v1 tos tos add,		\ screenblock
	1 ## v2 a lsl,
	8 ## a add,
	w a +( tos strh,		\ write

	\ enable bg in REG_DISPCNT
	1 ## v1 mov,
	8 ## v2 add,
	v2 v1 lsl,
	w 0@ v2 ldrh,
	v2 v1 v2 add,
	w 0@ v2 strh,

	\ done
	tos pop
	ret
end-code

\ turn off a background layer
code-thumb disabletiles ( bg -- )
	REGISTERS v1 movi
	v1 0@ v2 ldrh,
	
	\ clear bit in REG_DISPCNT
	1 ## v0 mov,
	8 ## tos add,
	tos v0 lsl,
	v0 v2 bic,
	v1 0@ v2 strh,
	
	\ done
	tos pop
	ret
end-code

\ set the color of a tile
code-thumb colortile ( a pal -- )
	v0 pop
	v0 0@ v1 ldrh,
	
	\ mask and set new palette
	$f0 ## v2 mov,
	8 ## v2 v2 lsl,	\ $f000
	v2 v1 bic,
	12 ## tos tos lsl,
	tos v1 orr,
	
	\ done
	v0 0@ v1 strh,
	tos pop
	ret
end-code

\ flip a tile
code-thumb fliptile ( a x y -- )
	v1 v2 pop
	
	\ load and clear flip bits
	v2 0@ v0 ldrh,
	$c00 w movi
	w v0 bic,
	
	\ compare and set x flip bits
	0 ## v1 cmp,
	8 #offset eq? b,
	$400 w movi		\ 2 insns!
	w v0 orr,
	
	\ compare and set y flip bits
	0 ## tos cmp,
	6 #offset eq? b,
	1 ## w w lsl,	\ $800
	w v0 orr,
	
	\ write and finish
	v2 0@ v0 strh,
	tos pop
	ret
end-code

\ set the priority of a bg
code-thumb ordertiles ( bg priority -- )
	v1 pop
	
	\ setup
	$40 ## v2 mov,
	20 ## v2 v2 lsl,	\ REGISTERS
	1 ## v1 v1 lsl,
	8 ## v1 add,
	
	\ load, mask and set
	v2 v1 +( v0 ldrh,
	3 ## a mov,
	a v0 bic,
	tos v0 orr,
	v2 v1 +( v0 strh,
	
	\ done
	tos pop
	ret
end-code

\ erase a rectangle of tiles
code-thumb cleartiles ( a w h -- )
	v1 v2 pop
	1 ## v1 v1 lsl,
	
	\ clear loop
	l: __loop
	v1 w mov,
	
	\ clear row loop
	l: __row
	v2 0@ v0 ldrh,
	$f000 a movi
	a v0 and,
	v2 0@ v0 strh,
	2 ## v2 add,
	2 ## w sub,
	__row gt? b,
	
	\ finish row
	64 ## v2 add,
	v1 v2 v2 sub,
	1 ## tos sub,
	__loop gt? b,
	
	\ done
	tos pop
	ret
end-code

\ erase a rectangle of tiles without preserving palette
code-thumb iwram cleartilesfast ( a w h -- )
	v1 v2 pop
	1 ## v1 v1 lsl,
	0 ## v0 mov,

	\ clear loop
	l: __loop
	v1 w mov,

	\ clear row loop
	l: __row
	v2 0@ v0 strh,
	2 ## v2 add,
	2 ## w sub,
	__row gt? b,

	\ finish row
	64 ## v2 add,
	v1 v2 v2 sub,
	1 ## tos sub,
	__loop gt? b,

	\ done
	tos pop
	ret
end-code

\ scroll a background
code-thumb scroll ( bg dx dy -- )
	v1 v2 pop
	
	\ address of background globals
	$3000400 w LITERAL	\ IWRAM + BG_REGS
	5 ## v2 v2 lsl,
	v2 w w add,
	\ BG0_SX ## w add,	\ nop...
	
	\ load current scroll values, add and write
	w ia! v0 v2 ldm,
	v0 v1 v0 add,
	v2 tos v2 add,
	8 ## w sub,
	w ia! v0 v2 stm,
	
	\ done
	tos pop
	ret
end-code

\ set absolute scroll position of a background
code-thumb scrollpos ( bg x y -- )
	v1 v2 pop
	
	\ address of background globals
	$3000400 v0 LITERAL	\ IWRAM + BG_REGS
	5 ## v2 v2 lsl,
	v2 v0 v0 add,
	\ nop BG0_SX ## v0 v0 add,
	
	\ load current scroll values, add and write
	v0 0@ v1 str,
	v0 4 #( tos str,
	
	\ done
	tos pop
	ret
end-code

\ return x scroll of a background
\ XXX: candidates for inlining
code-thumb scrollx ( bg -- x )
	$3000400 v0 LITERAL	\ IWRAM + BG_REGS
	5 ## tos tos lsl,
	tos v0 v0 add,
	\ nop BG0_SX ## v0 v0 add,
	
	\ load
	v0 0@ tos ldr,
	ret
end-code

\ return y scroll of a background
code-thumb scrolly ( bg -- y )
	$3000400 v0 LITERAL	\ IWRAM + BG_REGS
	5 ## tos tos lsl,
	tos v0 v0 add,
	\ nop BG0_SX ## v0 v0 add,
	
	\ load
	v0 4 #( tos ldr,
	ret
end-code

\ map tiles from ROM to VRAM
code-thumb maptiles ( tile-addr a w h -- )
	v0 v1 v2 pop
	r6 r7 push
	
	\ loop until mapped
	l: __loop
	v0 w mov,
	
	\ loop through each row
	l: __row
	v2 0@ r6 ldrh,
	$f000 r7 movi
	r7 r6 and,
	v1 0@ a ldrh,
	2 ## v1 add,
	a r6 orr,
	v2 0@ r6 strh,
	2 ## v2 add,
	1 ## w sub,
	__row gt? b,
	
	\ finish each row
	64 ## v2 add,
	1 ## v0 r7 lsl,
	r7 v2 v2 sub,
	1 ## tos sub,
	__loop gt? b,
	
	\ done
	r6 r7 pop
	tos pop
	ret
end-code

\ map an image of tiles to a background
code-thumb mapimage ( a base w h -- )
	v1 v2 w pop
	r6 push
	$f0 ## r6 mov,
	8 ## r6 r6 lsl,	\ $f000
	
	\ loop until mapped
	l: __loop
	v1 v0 mov,
	
	\ loop through each row
	l: __row
	w 0@ a ldrh,
	r6 a and,
	v2 a orr,
	w 0@ a strh,
	2 ## w add,
	1 ## v2 add,
	1 ## v0 sub,	\ subs, actually
	__row gt? b,
	
	\ advance to the next row
	64 ## w add,
	v1 w w sub,
	v1 w w sub,
	1 ## tos sub,	\ subs, actually
	__loop gt? b,
	
	\ done
	r6 pop
	tos pop
	ret
end-code

\ updates the scrolling of a background
code-thumb /scroll ( bg -- )
	\ v1 = 0x4000000 + BG * 4
	v7 v1 mov,
	2 ## v1 v1 lsl,
	w v1 v1 add,
	$10 ## v1 add,

	\ set offsets REG_BGxOFS
	tos ia! a v2 ldm,
	v1 0@ a strh,
	v1 2 #( v2 strh,
	
	\ done
	tos pop
	ret
end-code

\ updates background registers (scroll and rotation)
code-thumb updatetiles ( bg -- )
	tos v7 mov, \ store bg
	
	\ offset to virtual BG registers
	$3000400 v2 LITERAL	\ IWRAM + BG_REGS
	
	\ get REG_DISPCNT for current mode
	5 ## tos tos lsl,
	v2 tos tos add,
	$40 ## w mov,
	20 ## w w lsl,		\ REGISTERS
	w 0@ v2 ldrh,
	7 ## v1 mov,
	v1 v2 and,
	
	\ check graphics mode - mode 0 all bg's are text
	0 ## v2 cmp,
	/scroll eq? b,

	\ bg's 0 & 1 are always text
	v7 v1 mov,
	1 ## v1 cmp,
	/scroll le? b,
	
	\ backgrounds 2 and 3 are rotated backgrounds
	4 ## v1 v1 lsl,
	w v1 v1 add,		\ +0x20 = bg 2, +0x30 = bg 3
	8 ## tos add,
	
	\ copy dx, dy, pa, pb, pc & pd to registers
	tos ia! v2 w a ldm,
	v1 ia! v2 w a stm,
	tos ia! v2 w a ldm,
	v1 ia! v2 w a stm,
	
	\ done
	tos pop
	ret
end-code

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
	3 # n* a! push swap charblock swap 5 # n* + pop a dmacopy ;

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
icode gettile ( a -- u )
	tos 0@ tos ldrh,
	$fc00 ## tos tos bic,
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
code disabletiles ( bg -- )
	REGISTERS ## v1 mov,
	v1 0@ v2 ldrh,
	
	\ clear bit in REG_DISPCNT
	1 ## v3 mov,
	8 ## tos tos add,
	v3 tos lsl v2 v2 bic,
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
code fliptile ( a x y -- )
	sp ia! v1 v2 ldm,
	
	\ load and clear flip bits
	v2 0@ v0 ldrh,
	$c00 ## v0 v0 bic,
	
	\ compare and set x flip bits
	0 ## v1 cmp,
	$400 ## v0 v0 ne? orr,
	
	\ compare and set y flip bits
	0 ## tos cmp,
	$800 ## v0 v0 ne? orr,
	
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
code cleartiles ( a w h -- )
	sp ia! v1 v2 ldm,
	v1 1 #lsl v1 mov,
	
	\ clear loop
	l: __loop
	v1 v4 mov,
	
	\ clear row loop
	l: __row
	v2 0@ v3 ldrh,
	$f000 ## v3 v3 and,
	v2 2 (# v3 strh,
	2 ## v4 v4 s! sub,
	__row gt? b,
	
	\ finish row
	64 ## v2 v2 add,
	v1 v2 v2 sub,
	1 ## tos tos s! sub,
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
code scrollpos ( bg x y -- )
	sp ia! v1 v2 ldm,
	
	\ address of background globals
	IWRAM ## v5  mov,
	BG_REGS ## v5 v5 add,
	v2 5 #lsl v5 v5 add,
	BG0_SX ## v5 v5 add,
	
	\ load current scroll values, add and write
	v5 4 (# v1 str,
	v5 4 (# tos str,
	
	\ done
	tos pop
	ret
end-code

\ return x scroll of a background
code scrollx ( bg -- x )
	IWRAM ## v5  mov,
	BG_REGS ## v5 v5 add,
	tos 5 #lsl v5 v5 add,
	BG0_SX ## v5 v5 add,
	
	\ load
	v5 0@ tos ldr,
	ret
end-code

\ return y scroll of a background
code scrolly ( bg -- y )
	IWRAM ## v5  mov,
	BG_REGS ## v5 v5 add,
	tos 5 #lsl v5 v5 add,
	BG0_SX ## v5 v5 add,
	
	\ load
	v5 4 #( tos ldr,
	ret
end-code

\ map tiles from ROM to VRAM
code maptiles ( tile-addr a w h -- )
	sp ia! v1 v2 v3 ldm,
	
	\ loop until mapped
	l: __loop
	v1 v4 mov,
	
	\ loop through each row
	l: __row
	v3 0@ v6 ldrh,
	$f000 ## v6 v6 and,
	v2 2 (# v5 ldrh,
	v6 v5 v6 orr,
	v3 2 (# v6 strh,
	1 ## v4 v4 s! sub,
	__row gt? b,
	
	\ finish each row
	64 ## v3 v3 add,
	v1 1 #lsl v3 v3 sub,
	1 ## tos tos s! sub,
	__loop gt? b,
	
	\ done
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

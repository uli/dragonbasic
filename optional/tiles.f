{ -- MF/TIN tile library
  -- Copyright (c) 2003 by Jeff Massung }

\ return the VRAM address of tile data (inlined)
icode charblock ( n -- a )
	tos 14 #lsl tos mov,
	VRAM ## tos tos add,
end-code

\ return the VRAM address of map data (inlined)
icode screenblock ( n -- a )
	tos 11 #lsl tos mov,
	VRAM ## tos tos add,
end-code

\ copy ROM data to VRAM tile data
: loadtiles ( charblock offset from blocks -- )
	5 # n* a! push swap charblock swap 5 # n* + pop a copy ;

\ copy a font in ROM data to a character block
: loadfont16 ( charblock a -- ) 32 # swap 95 # loadtiles ;
: loadfont256 ( charblock a -- ) 64 # swap 190 # loadtiles ;

\ return address of tile on screenblock
code tile ( screen x y -- a )
	sp ia! v2 v3 ldm,
	
	\ get address of tile
	tos 5 #lsl v2 w add,
	w 1 #lsl w mov,
	v3 11 #lsl w w add,
	VRAM ## w tos add,
	
	\ done
	ret
end-code

\ returns the index of a tile (inlined)
icode gettile ( a -- u )
	tos 0@ tos ldrh,
	$fc00 ## tos tos bic,
end-code

\ sets the index of a tile
code settile ( a u -- )
	w pop
	
	\ get mask and set data
	w 0@ v0 ldrh,
	$f000 ## v0 v0 and,
	tos v0 v0 add,
	w 0@ v0 strh,
	
	\ done
	tos pop
	ret
end-code

\ enable a background layer
code enabletiles ( bg screen char flags -- )
	sp ia! v0 v1 v2 ldm,
	REGISTERS ## v4 mov,
	
	\ construct REG_BGxCNT
	v0 2 #lsl tos tos add,	\ charblock
	v1 8 #lsl tos tos add,	\ screenblock
	v2 1 #lsl v3 mov,
	8 ## v3 v3 add,
	v4 v3 +( tos strh,		\ write
	
	\ enable bg in REG_DISPCNT
	1 ## v1 mov,
	8 ## v2 v2 add,
	v1 v2 lsl v1 mov,
	v4 0@ v2 ldrh,
	v2 v1 v2 add,
	v4 0@ v2 strh,
	
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
code colortile ( a pal -- )
	v0 pop
	v0 0@ v1 ldrh,
	
	\ mask and set new palette
	$f000 ## v1 v1 bic,
	tos 12 #lsl v1 v1 orr,
	
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
code ordertiles ( bg priority -- )
	v1 pop
	
	\ setup
	REGISTERS ## v2 mov,
	v1 1 #lsl v1 mov,
	8 ## v1 v1 add,
	
	\ load, mask and set
	v2 v1 +( v3 ldrh,
	3 ## v3 v3 bic,
	tos v3 v3 orr,
	v2 v1 +( v3 strh,
	
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
code scroll ( bg dx dy -- )
	sp ia! v1 v2 ldm,
	
	\ address of background globals
	IWRAM ## v5  mov,
	BG_REGS ## v5 v5 add,
	v2 5 #lsl v5 v5 add,
	BG0_SX ## v5 v5 add,
	
	\ load current scroll values, add and write
	v5 ia v3 v4 ldm,
	v3 v1 v3 add,
	v4 tos v4 add,
	v5 ia v3 v4 stm,
	
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
code mapimage ( a base w h -- )
	sp ia! v1 v2 v3 ldm,
	
	\ loop until mapped
	l: __loop
	v1 v4 mov,
	
	\ loop through each row
	l: __row
	v3 0@ v6 ldrh,
	$f000 ## v6 v6 and,
	v6 v2 v6 orr,
	v3 2 (# v6 strh,
	1 ## v2 v2 add,
	1 ## v4 v4 s! sub,
	__row gt? b,
	
	\ advance to the next row
	64 ## v3 v3 add,
	v1 1 #lsl v3 v3 sub,
	1 ## tos tos s! sub,
	__loop gt? b,
	
	\ done
	tos pop
	ret
end-code

\ updates the scrolling of a background
code /scroll ( bg -- )
	\ v7 = 0x4000000 + BG * 4
	v7 2 #lsl v3 v7 add,
	$10 ## v7 v7 add,

	\ set offsets REG_BGxOFS
	tos ia v1 v2 ldm,
	v7 0@ v1 strh,
	v7 2 #( v2 strh,
	
	\ done
	tos pop
	ret
end-code

\ updates background registers (scroll and rotation)
code updatetiles ( bg -- )
	tos v7 mov, \ store bg
	
	\ offset to virtual BG registers
	IWRAM ## v2 mov,
	BG_REGS ## v2 v2 add,
	
	\ get REG_DISPCNT for current mode
	tos 5 #lsl v2 tos add,
	REGISTERS ## v3 mov,
	v3 0@ v2 ldrh,
	7 ## v2 v2 and,
	
	\ check graphics mode - mode 0 all bg's are text
	0 ## v2 cmp,
	/scroll eq? b,

	\ bg's 0 & 1 are always text
	1 ## v7 cmp,
	/scroll le? b,
	
	\ backgrounds 2 and 3 are rotated backgrounds
	v7 4 #lsl v3 v7 add, \ +0x20 = bg 2, +0x30 = bg 3
	8 ## tos tos add,
	
	\ copy dx, dy, pa, pb, pc & pd to registers
	tos ia v1 v2 v3 v4 v5 v6 ldm,
	v7 ia v1 v2 v3 v4 v5 v6 stm,
	
	\ done
	tos pop
	ret
end-code
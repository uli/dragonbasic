{ -- Palette Library for MF/TIN
  -- Copyright (c) 2003 by Jeff Massung }

\ copy 32 bytes of data from ROM to palette RAM
: loadpal16 ( palette index a -- )
	a! 5 # n* + a 32 # copy ;

\ copy 512 bytes of data from ROM to palette RAM
: loadpal256 ( palette a -- )
	512 # copy ;

\ prototype color = rgb(r,g,b)
: rgb ( r g b -- color )
	10 # n* swap 5 # n* + + ;

\ components of a color
: rgbr ( color -- r ) $1f # and ;
: rgbg ( color -- g ) 5 # n/ $1f # and ;
: rgbb ( color -- g ) 10 # n/ $1f # and ;

\ palette constant data
label __colors

$0019001f , \ each palette entry is increased by 6
$000c0013 , \ starting from the darkest color and
$00000006 , \ increasing in intensity to white

code makepalette ( pal -- )
	__colors r1 literal
	10 ## r2 mov,

	\ blue loop
	l: __blue
	r1 r2 +( r9 ldrh,
	10 ## r3 mov,
	
	\ green loop
	l: __green
	r1 r3 +( r10 ldrh,
	10 ## r8 mov,
	
	\ red loop
	l: __red
	r1 r8 +( r11 ldrh,
	
	\ create 15-bit color
	r10 5 #lsl r11 r11 orr,
	r9 10 #lsl r11 r11 orr,
	r0 2 (# r11 strh,
	
	\ finish loops
	2 ## r8 r8 s! sub,
	__red pl? b,
	2 ## r3 r3 s! sub,
	__green pl? b,
	2 ## r2 r2 s! sub,
	__blue pl? b,
	
	\ done
	tos pop
	ret
end-code

code getpalentry ( pal index entry -- color )
	sp ia! v1 v2 ldm,
	v1 5 #lsl v2 v2 add,
	v2 tos 1 #lsl +( tos ldrh,
	ret
end-code

code setpalentry ( pal index entry color -- )
	sp ia! v1 v2 v3 ldm,
	v2 5 #lsl v2 mov,
	v1 1 #lsl v2 v1 add,
	v3 v1 +( tos strh,
	tos pop
	ret
end-code

code rotatepal16 ( pal index -- )
	sp ia! r1 r4 ldm,
	r0 5 #lsl r1 r0 add,
	2 ## r0 r0 add, \ skip entry 0
	0 ## r1 mov,
	r0 r1 +( r2 ldrh,
	
	\ loop until all 15 colors are rotated
	l: __loop
	2 ## r1 r1 add,
	r0 r1 +( r3 ldrh,
	r0 r1 +( r2 strh,
	r3 r2 mov,
	$1e ## r1 cmp,
	__loop lt? b,
	
	\ done
	r0 0@ r2 strh,
	r4 tos mov,
	ret
end-code

code rotatepal256 ( pal -- )
	r0 5 #lsl r0 mov,
	2 ## r0 r0 add, \ skip index 0
	0 ## r1 mov,
	$200 ## r4 mov,
	2 ## r4 r4 sub,
	r0 r1 +( r2 ldrh,
	
	\ loop until all 255 are rotated
	l: __loop
	2 ## r1 r1 add,
	r0 r1 +( r3 ldrh,
	r0 r1 +( r2 strh,
	r3 r2 mov,
	r4 r1 cmp,
	__loop lt? b,
	
	\ done
	r0 0@ r2 strh,
	tos pop
	ret
end-code
{ -- Palette Library for MF/TIN
  -- Copyright (c) 2003 by Jeff Massung }

\ copy 32 bytes of data from ROM to palette RAM
: loadpal16 ( palette index a -- )
	a! 5 # n* + a 32 # copy ;

\ copy 512 bytes of data from ROM to palette RAM
: loadpal256 ( palette a -- )
	128 # dmacopy ;

\ prototype color = rgb(r,g,b)
:n rgb ( r g b -- color )
	10 # n* swap 5 # n* + + ;

\ components of a color
:n rgbr ( color -- r ) $1f # and ;
:n rgbg ( color -- g ) 5 # n/ $1f # and ;
:n rgbb ( color -- g ) 10 # n/ $1f # and ;

\ palette constant data
label __colors

$0019001f , \ each palette entry is increased by 6
$000c0013 , \ starting from the darkest color and
$00000006 , \ increasing in intensity to white

code-thumb makepalette ( pal -- )
	r6 r7 push
	__colors r1 literal
	10 ## r2 mov,

	\ blue loop
	l: __blue
	r1 r2 +( r4 ldrh,
	10 ## r3 mov,
	
	\ green loop
	l: __green
	r1 r3 +( r5 ldrh,
	10 ## r6 mov,
	
	\ red loop
	l: __red
	r1 r6 +( r7 ldrh,
	
	\ create 15-bit color
	5 ## r5 r5 lsl,
	r5 r7 orr,
	10 ## r4 r4 lsl,
	r4 r7 orr,
	r0 0@ r7 strh,
	2 ## r0 add,
	
	\ finish loops
	2 ## r6 sub,
	__red pl? b,
	2 ## r3 sub,
	__green pl? b,
	2 ## r2 sub,
	__blue pl? b,
	
	\ done
	r6 r7 pop
	tos pop
	ret
end-code

code-thumb getpalentry ( pal index entry -- color )
	v1 v2 pop
	5 ## v1 v1 lsl,
	v1 v2 v2 add,
	2 ## tos tos lsl,
	tos v2 +( tos ldrh,
	ret
end-code

code-thumb setpalentry ( pal index entry color -- )
	v0 v1 v2 pop
	5 ## v1 v1 lsl,
	1 ## v0 v0 lsl,
	v1 v0 v0 add,
	v2 v0 +( tos strh,
	tos pop
	ret
end-code

code-thumb rotatepal16 ( pal index -- )
	r1 r4 pop
	5 ## r0 r0 lsl,
	r1 r0 r0 add,
	2 ## r0 add, \ skip entry 0
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

code-thumb rotatepal256 ( pal -- )
	5 ## r0 r0 lsl,
	2 ## r0 add, \ skip index 0
	0 ## r1 mov,
	$200 r4 movi
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

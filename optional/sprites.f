{ -- MF/TIN SPRITE functions
  -- Original code by Jeff Massung, 2003 }

{ -- OAM data is duplicated at 0x3000000 - 0x3000400
  -- and every updatesprites call waits for a vertical
  -- blank and copies the data to 0x7000000 - 0x7000400
  -- all function calls that return the address of a
  -- sprite return the duplicated address }

\ return the address of a sprite
:i sprite ( n -- a ) 3 # n* $3000000 # + ;

\ copy sprite data from ROM to VRAM
: loadsprite ( n a blocks -- )
	3 # n* a! swap 5 # n* $6000000 # + $10000 # + swap a@ dmacopy ;

\ returns the current frame character of a sprite
:n spriteframe ( n -- char )
	sprite 4 # + peek $fc00 # com and ;

\ create a new sprite
code-thumb makesprite ( sprite n -- )
	v0 v1 pop
	$30 ## v2 mov,
	20 ## v2 v2 lsl,	\ IWRAM
	3 ## v0 v0 lsl,
	v2 v0 v0 add,
	
	\ clear sprite attribute 0 and set 256 color bit
	$20 ## v2 mov,
	8 ## v2 v2 lsl,		\ $2000
	v0 0@ v2 strh,
	2 ## v0 add,
	
	\ clear attribute 1
	0 ## v2 mov,
	v0 0@ v2 strh,
	2 ## v0 add,
	
	\ set name bits of attribute 2
	v0 0@ tos strh,
	v1 tos mov,
	ret
end-code

\ set the shape and size of a sprite
code-thumb sizesprite ( sprite shape size -- )
	v1 v2 pop
	$30 ## v0 mov,
	20 ## v0 v0 lsl,	\ IWRAM
	
	\ get sprite address
	3 ## v2 v2 lsl,
	v0 v2 v2 add,
	
	\ load shape bit and clear
	v2 0@ v0 ldrh,
	$c2 ## w mov,
	8 ## w w lsl,
	w v0 bic,
	v1 v0 orr,
	v2 0@ v0 strh,
	
	\ load size and bit clear
	v2 2 #( v0 ldrh,
	$c0 ## w mov,
	8 ## w w lsl,
	w v0 bic,
	tos v0 orr,
	v2 2 #( v0 strh,
	
	\ done
	tos pop
	ret
end-code

\ set a sprite to 16-color and select palette
code-thumb colorsprite ( sprite index -- )
	w pop
	
	\ sprite address
	IWRAM v2 LITERAL
	3 ## w v1 lsl,
	v1 v2 w add,
	
	\ set palette entry
	w 4 #( v2 ldrh, 
	$f000 v1 LITERAL
	v1 v2 bic,
	12 ## tos v1 lsl,
	v1 v2 orr,
	w 4 #( v2 strh,
	
	\ clear 256-color bit
	w 0@ v2 ldrh,
	$2000 v1 LITERAL
	v1 v2 bic,
	w 0@ v2 strh,

	\ done
	tos pop
	ret
end-code

\ flips a sprite or mirrors it
code-thumb flipsprite ( sprite x y -- )
	v1 v2 pop
	$30 ## v0 mov,
	20 ## v0 v0 lsl,	\ IWRAM
	
	3 ## v2 v2 lsl,
	v0 v2 v2 add,
	
	\ clear flip bits
	v2 2 #( v0 ldrh,
	$30 ## a mov,
	8 ## a a lsl,	\ $3000
	a v0 bic,
	
	\ set x flip bit
	0 ## v1 cmp,
	8 #offset eq? b,
	$10 ## a mov,
	8 ## a a lsl,	\ $1000
	a v0 orr,
	
	\ set y flip bit
	0 ## tos cmp,
	8 #offset eq? b,
	$20 ## a mov,
	8 ## a a lsl,	\ $2000
	a v0 orr,
	
	\ done
	v2 2 #( v0 strh,
	tos pop
	ret
end-code

\ return the x coordinate of a sprite
code-thumb spritex ( sprite -- x )
	$30 ## w mov,
	20 ## w w lsl,	\ IWRAM
	3 ## tos tos lsl,
	w tos tos add,
	tos 2 #( tos ldrh,
	
	\ x >= 240 then negate
	$fe ## v0 mov,
	8 ## v0 v0 lsl,	\ $fe00
	v0 tos bic,
	$f0 ## tos cmp,
	8 #offset lt? b,
	$2 ## v0 mov,
	8 ## v0 v0 lsl,
	v0 tos tos sub,
	
	\ done
	ret
end-code

\ return the y coordinate of a sprite
code-thumb spritey ( sprite -- y )
	$30 ## w mov,
	20 ## w w lsl,	\ IWRAM
	3 ## tos tos lsl,
	w tos tos add,
	tos 0@ tos ldrh,
	
	\ y >= 160 then negate
	$ff ## v0 mov,
	v0 tos and,
	$a0 ## tos cmp,
	6 #offset lt? b,
	$80 ## tos sub,
	$80 ## tos sub,
	
	\ done
	ret
end-code

\ move the sprite offscreen so it is hidden
code-thumb hidesprite ( sprite -- )
	$30 ## w mov,
	20 ## w w lsl,	\ IWRAM
	3 ## tos tos lsl,
	w tos tos add,
	
	\ y = 160
	tos 0@ w ldrh,
	$ff ## v0 mov,
	v0 w bic,
	$a0 ## v0 mov,
	v0 w orr,
	tos 0@ w strh,
	
	\ x = 240
	tos 2 #( w ldrh,
	$fe ## v0 mov,
	8 ## v0 v0 lsl,	\ $fe00
	v0 w and,
	$f0 ## v0 mov,
	v0 w orr,
	tos 2 #( w strh,
	
	\ done
	tos pop
	ret
end-code

\ returns -1 if sprite is offscreen or 0 if onscreen
code-thumb spritehidden ( sprite -- flag )
	$30 ## w mov,
	20 ## w w lsl,	\ IWRAM
	3 ## tos tos lsl,
	w tos tos add,
	
	\ check y >= 160
	tos 0@ w ldrh,
	$ff ## r1 mov,
	8 ## r1 r1 lsl,	\ $ff00
	r1 w bic,
	$a0 ## w cmp,
	8 #offset lt? b,
	0 ## tos mov,
	tos tos mvn,	\ tos = -1
	ret
	
	\ check x >= 240
	tos 2 #( w ldrh,
	$fe ## r1 mov,
	8 ## r1 r1 lsl,
	r1 w bic,
	$f0 ## w cmp,
	8 #offset lt? b,
	0 ## tos mov,
	tos tos mvn,	\ tos = -1
	ret
	
	\ done
	0 ## tos mov, \ tos = 0
	ret
end-code

\ set the absolute position of a sprite
code-thumb positionsprite ( sprite x y -- )
	v1 v2 pop

	IWRAM w LITERAL
	3 ## v2 v2 lsl,
	v2 w v2 add,
	
	\ fix position
	$100 w LITERAL
	0 ## tos cmp,
	4 #offset ge? b,
	w tos tos add,
	0 ## v1 cmp,
	6 #offset ge? b,
	w v1 v1 add,
	w v1 v1 add,
	
	\ y position
	v2 0@ w ldrh,
	$ff00 v0 LITERAL
	v0 w and,
	tos w orr,
	v2 0@ w strh,
	
	\ x position
	v2 2 #( w ldrh,
	$fe00 v0 LITERAL
	v0 w and,
	v1 w orr,
	v2 2 #( w strh,
	
	\ done
	tos pop
	ret
end-code

\ relative movement of a sprite
code-thumb movesprite ( sprite dx dy -- )
	v1 v2 pop
	u push

	\ get address
	$30 ## w mov,
	20 ## w w lsl,	\ IWRAM
	3 ## v2 v2 lsl,
	w v2 v2 add,
	
	\ setup registers for speed
	$ff ## a mov,
	a v0 mov,
	1 ## v0 add,
	
	\ load y and mask out
	v2 0@ u ldrh,
	u w mov,
	a w bic,
	a u and,
	
	\ add dy to y position
	tos u u add,	\ adds, actually
	4 #offset ge? b,
	v0 u u add,
	
	\ write new y position
	a u and,
	w u orr,
	v2 0@ u strh,
	
	\ x position is 9-bit
	v0 a a add,
	
	\ load x and mask out
	v2 2 #( u ldrh,
	u w mov,
	a w bic,
	a u and,
	
	\ add dx to x position
	v1 u u add,	\ adds, actually
	6 #offset ge? b,
	1 ## v0 v0 lsl,
	v0 u u add,
	
	\ write new x position
	a u and,
	w u orr,
	v2 2 #( u strh,
	
	\ done
	u pop
	tos pop
	ret
end-code

\ set the draw priority of a sprite
code-thumb ordersprite ( sprite priority -- )
	w pop
	
	\ address of sprite
	$30 ## v2 mov,
	20 ## v2 v2 lsl,	\ IWRAM
	3 ## w w lsl,
	v2 w w add,
	
	\ load, clear and set priority bit
	w 4 #( v2 ldrh,
	$c ## v1 mov,
	8 ## v1 v1 lsl,	\ $c00
	v1 v2 bic,
	10 ## tos tos lsl,
	tos v2 orr,
	w 4 #( v2 strh,

	\ done
	tos pop
	ret
end-code

\ update the frame of a sprite
code-thumb animsprite ( sprite start end blocks -- )
	v0 v1 v2 pop
	r6 push

	\ sprite address
	$30 ## r6 mov,
	20 ## r6 r6 lsl,	\ IWRAM
	3 ## v2 v2 lsl,
	v2 r6 r6 add,

	\ load current frame
	r6 4 #( w ldrh,
	$fc ## v2 mov,
	8 ## v2 v2 lsl,		\ $fc00
	w a mov,
	v2 a bic,
	v2 w and,

	\ if a < start || a > end then a = start - blocks
	v1 a cmp,
	6 #offset ge? b,
	tos v1 a sub,
	6 #offset b,
	v0 a cmp,
	4 #offset le? b,
	tos v1 a sub,

	\ add blocks to a and reset if > end
	tos a a add,
	v0 a cmp,
	4 #offset le? b,
	v1 a mov,

	\ write back
	a w orr,
	r6 4 #( w strh,
	
	\ done
	r6 pop
	tos pop
	ret
end-code

\ sprite size constants (in ROM)
label __sizes

$00080008 , $00100010 , $00200020 , $00300030 , \ sqaure
$00100008 , $00200008 , $00200010 , $00300020 , \ wide
$00080010 , $00080020 , $00100020 , $00200030 , \ tall

\ check to see if two sprites overlap at all
code-thumb bumpsprites ( sprite1 sprite2 -- flag )
	a pop
	r6 r7 push
	tos r7 mov,
	
	\ get address of size table
	$30 ## v0 mov,
	20 ## v0 v0 lsl,	\ IWRAM
	__sizes tos LITERAL
	tos v5 mov,
	
	\ get addresses of sprites
	3 ## a a lsl,
	v0 a a add,
	3 ## r7 r7 lsl,
	v0 r7 r7 add,

	\ get the size of sprite 1 (V6->[V0,V1])
	a 0@ r6 ldrh,
	a 2 #( v0 ldrh,
	$c0 ## tos mov,
	8 ## tos tos lsl,	\ $c000
	tos r6 and,
	12 ## r6 r6 lsr,
	14 ## v0 v0 lsr,
	v0 r6 orr,
	2 ## r6 r6 lsl,
	v5 tos mov,
	tos r6 +( v2 ldr,
	$ff ## tos mov,
	v2 v1 mov,
	tos v1 and,
	16 ## v2 v2 lsr,
	v2 v6 mov,

	\ get the position of sprite 1 (V6->[V2,V6])
	a 2 #( v2 ldrh,
	a 0@ a ldrh,
	$ff ## tos mov,
	tos a and,
	$fe ## tos mov,
	8 ## tos tos lsl,	\ $fe00
	tos v2 bic,

	\ make sure that negative values will collide
	240 ## v2 cmp,
	8 #offset lt? b,
	$2 ## tos mov,
	8 ## tos tos lsl,	\ $200
	tos v2 v2 sub,

	160 ## a cmp,
	8 #offset lt? b,
	$1 ## tos mov,
	8 ## tos tos lsl,	\ $100
	tos a a sub,

	\ get the size of sprite 2 (V7->[V5,V3])
	r7 0@ r6 ldrh,
	r7 2 #( v0 ldrh,
	$c0 ## tos mov,
	8 ## tos tos lsl,	\ $c000
	tos r6 and,
	12 ## r6 r6 lsr,
	14 ## v0 v0 lsr,
	v0 r6 orr,
	v5 tos mov,
	2 ## r6 r6 lsl,
	tos r6 +( v0 ldr,
	$ff ## tos mov,
	v0 tos and,
	tos v3 mov,
	16 ## v0 v0 lsr,

	\ get the position of sprite 2 (V7->[V4,V7])
	r7 2 #( r6 ldrh,
	r7 0@ r7 ldrh,
	$ff ## tos mov,
	tos r7 and,
	$fe ## tos mov,
	8 ## tos tos lsl,	\ $fe00
	tos r6 bic,

	\ make sure that negative values will collide
	240 ## r6 cmp,
	8 #offset lt? b,
	$2 ## tos mov,
	8 ## tos tos lsl,	\ $200
	tos r6 r6 sub,

	160 ## r7 cmp,
	8 #offset lt? b,
	$1 ## tos mov,
	8 ## tos tos lsl,	\ $100
	tos r7 r7 sub,

	\ set collision to false
	0 ## tos mov,

	\ test x direction (V4 > V2+V0 = fail)
	v6 w mov,
	v2 w w add,
	w r6 cmp,
	6 #offset le? b,
	l: __ret
	r6 r7 pop
	ret \ fail

	\ test x direction (V4+V5 < V2 = fail)
	v0 r6 w add,
	w v2 cmp,
	__ret gt? b, \ fail

	\ test y direction (V7 > V6+V1 = fail)
	v1 a w add,
	w r7 cmp,
	__ret gt? b, \ fail

	\ test y direction (V7+V3 < V6 = fail)
	v3 w mov,
	r7 w w add,
	w a cmp,

	\ success
	6 #offset gt? b,
	0 ## tos mov,
	tos tos mvn,
	r6 r7 pop
	ret
end-code

\ copy all updated sprite data from IWRAM to OAM
code-thumb updatesprites ( -- )
	$30 ## v1 mov,
	20 ## v1 v1 lsl,	\ IWRAM
	$70 ## v2 mov,
	20 ## v2 v2 lsl,	\ OAM
	$40 ## v0 mov,
	20 ## v0 v0 lsl,	\ REGISTERS
	$d0 ## v0 add,		\ REGISTERS + $d0

	\ setup dma3 transfer addresses
	v0 $4 #( v1 str,
	v0 $8 #( v2 str,

	\ set control to 32-bit, increment both, 256 words
	$84000100 v1 LITERAL
	v0 $c #( v1 str,

	\ done
	ret
end-code

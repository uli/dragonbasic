{ -- MF/TIN CORE (always include) functions
  -- Copyright (c) 2003 by Jeff Massung }

\ inlined peek and poke values into GBA memory
icode-thumb peek ( a -- h ) tos 0@ tos ldrh, end-code
icode-thumb peekb ( a -- h ) tos 0@ tos ldrb, end-code
icode-thumb peekw ( a -- h ) tos 0@ tos ldr, end-code
icode-thumb poke ( a h -- ) w pop w 0@ tos strh, tos pop end-code
icode-thumb pokeb ( a h -- ) w pop w 0@ tos strb, tos pop end-code
icode-thumb pokew ( a h -- ) w pop w 0@ tos str, tos pop end-code

\ divide and modula
:n / ( n1 n2 -- n3 ) swap a! 7 swi ;
:n mod ( n1 n2 -- n3 ) swap a! 7 swi drop a ;

\ conditionals
:n = ( n1 n2 -- flag ) - 0= ;
:n <> ( n1 n2 -- flag ) - 0= com ;
:n < ( n1 n2 -- flag ) - 0< ;
:n > ( n1 n2 -- flag ) swap - 0< ;
:n <= ( n1 n2 -- flag ) swap - 0< com ;
:n >= ( n1 n2 -- flag ) - 0< com ;

\ fixed point math operations
:n f* ( n1 n2 -- n3 ) * 8 # a/ ;
:n f/ ( n1 n2 -- n3 ) a! 8 # n* 6 swi ;

\ send a string to the VBA console
:n log ( a -- ) 1+ $ff swi drop ;

\ the restore data pointer
variable .idata

\ transfer from data pointer to local address register
:n >a ( -- ) .idata @ a! ;

:n a> ( -- ) a .idata ! ;

\ allocate bytes of data on the return stack
code-thumb r-alloc ( u -- a )
	rsp w mov,
	
	\ allocate space
	tos rsp rsp sub,
	rsp tos mov,
	4 ## rsp sub,
	rsp 0@ w str,	\ save old rsp
	
	\ save new returning adress
	4 ## rsp sub,
	rsp 0@ u str,
	pc u mov,
	3 ## u add,	\ 2 to skip this insn and 1 for Thumb
	ret
	
	\ this is called on next return
	rsp ia! u ldm,
	rsp 0@ rsp ldr,
	u bx,
end-code

\ copy bytes from one address to another
code-thumb copy ( to from u -- )
	v0 v1 pop
	tos tos tst,
	
	\ loop
	l: __copy
	
	\ if <= 0 then return
	6 #offset gt? b,
	tos pop
	ret
	
	\ transfer
	v0 0@ w ldrh,
	2 ## v0 add,
	v1 0@ w strh,
	2 ## v1 add,
	
	\ decrement and loop
	2 ## tos sub,	\ subs, actually
	__copy b,
end-code

\ erase bytes at an address
code-thumb erase ( a u -- )
	v0 pop
	w w eor,
	tos tos tst,
	
	\ loop
	l: __erase
	
	\ if <= 0 then return
	8 #offset gt? b,
	tos pop
	ret
	
	\ erase
	v0 0@ w strh,
	2 ## v0 add,
	
	\ decrement and loop
	2 ## tos sub,	\ subs, actually
	__erase b,
end-code

\ set the graphics mode
code-thumb graphics ( mode sprites -- )
	v1 pop
	
	\ ready r2
	0 ## v2 mov,
	0 ## tos cmp,
	
	\ set the sprite bit
	4 #offset eq? b,
	$1040 tos literal
	
	\ bitmapped modes enable bg 2
	3 ## v1 cmp,
	8 #offset lt? b,
	$4 ## v2 mov,
	8 ## v2 v2 lsl,
	
	\ write mode to REG_DISPCNT
	v1 v2 orr,
	tos v2 orr,
	$40 ## tos mov,
	20 ## tos tos lsl,	\ REGISTERS
	tos 0@ v2 strh,

	\ store VRAM address
	$60 ## tos mov,
	20 ## tos tos lsl,	\ VRAM
	tos push
	
	\ erase VRAM
	$17fff tos literal
	erase b,
end-code

\ wait for the next vertical blank
code-thumb vblank ( -- )
	$4000000 v0 LITERAL
	
	\ wait for not vertical blank
	\ makes sure we wait for one frame if less than one scanline has
	\ passed since the previous call
	l: __wait
	v0 6 #( v1 ldrh,
	160 ## v1 cmp,
	__wait eq? b,

	\ wait for vertical blank
	l: __wait
	v0 6 #( v1 ldrh,
	160 ## v1 cmp,
	__wait ne? b,

	\ done
	ret
end-code

\ return the current scanline
code-thumb scanline ( -- n )
	tos push
	$40 ## tos mov,
	20 ## tos tos lsl,	\ REGISTERS
	tos 6 #( tos ldrh,
	ret
end-code
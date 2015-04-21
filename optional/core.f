{ -- MF/TIN CORE (always include) functions
  -- Copyright (c) 2003 by Jeff Massung }

\ inlined peek and poke values into GBA memory
icode-thumb peek ( a -- h ) tos 0@ tos ldrh, end-code
icode-thumb peekb ( a -- h ) tos 0@ tos ldrb, end-code
icode-thumb peekw ( a -- h ) tos 0@ tos ldr, end-code
icode-thumb poke ( a h -- ) w pop w 0@ tos strh, tos pop end-code
icode-thumb pokeb ( a h -- ) w pop w 0@ tos strb, tos pop end-code
icode-thumb pokew ( a h -- ) w pop w 0@ tos str, tos pop end-code

\ divide and modulo
:i / ( n1 n2 -- n3 ) swap a! 7 swi ;
:i mod ( n1 n2 -- n3 ) swap a! 7 swi drop a@ ;

code-thumb iwram 10/ ( n -- q )
	v0 v5 mov,
	15 ## tos v0 lsr,
	__recip eq? b,
	10 ## a mov,
	6 swi,
	v5 v0 mov,
	ret
	l: __recip
	3277 v0 LITERAL
	tos v0 mul,
	15 ## v0 v0 lsr,
	10 ## a mov,
	v0 a mul,
	a tos a sub,
	v0 tos mov,
	v5 v0 mov,
	ret
end-code

\ conditionals
:n = ( n1 n2 -- flag ) - 0= ;
:n <> ( n1 n2 -- flag ) - 0= com ;
:i < ( n1 n2 -- flag ) - 0< ;
:i > ( n1 n2 -- flag ) swap - 0< ;
:i <= ( n1 n2 -- flag ) swap - 0< com ;
:i >= ( n1 n2 -- flag ) - 0< com ;

\ fixed point math operations
:i f* ( n1 n2 -- n3 ) * 8 # a/ ;
:n f/ ( n1 n2 -- n3 ) a! 8 # n* 6 swi ;

\ the restore data pointer
variable .idata

\ transfer from data pointer to local address register
:i >a ( -- ) .idata @ a! ;

:i a> ( -- ) a@ .idata ! ;

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
	__end eq? b,
	
	\ loop
	l: __copy
	
	\ transfer
	v0 0@ w ldrh,
	2 ## v0 add,
	v1 0@ w strh,
	2 ## v1 add,
	
	\ decrement and loop
	2 ## tos sub,	\ subs, actually
	\ if > 0 then continue
	__copy gt? b,

l: __end
	tos pop
	ret
end-code

\ erase bytes at an address
code-thumb erase ( a u -- )
	v0 pop
	w w eor,
	tos tos tst,
	
	\ if <= 0 then return
	__exit le? b,

	\ loop
	l: __erase

	\ erase
	v0 0@ w str,
	4 ## v0 add,
	
	\ decrement and loop
	4 ## tos sub,	\ subs, actually
	__erase gt? b,

	l: __exit
	tos pop
	ret
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
\ XXX: This assumes that the user vblank handler does not touch DISPCNT or
\ IE...
code-thumb vblank ( -- )
	tos push

	\ enable vblank interrupt in DISPCNT
	$4000000 v0 movi
	v0 4 #( v1 ldrh,
	v1 push		\ push current setting for later
	8 ## v2 mov,
	v2 v1 orr,
	v0 4 #( v1 strh,

	\ enable vblank interrupt in IE
	$4000200 v0 literal
	v0 0@ v1 ldrh,
	v0 v1 push		\ push IE setting and address for later
	1 ## v2 mov,
	v2 v1 orr,
	v0 0@ v1 strh,	\ enable vblank interrupt

	5 swi,		\ VBlankIntrWait

	v0 v1 v2 pop		\ pop IE address, IE setting, DISPCNT setting
	v0 0@ v1 strh,		\ restore IE value
	$4000000 v0 movi
	v0 4 #( v2 strh,	\ restore DISPCNT value

	tos pop
	ret
end-code

\ return the current scanline
icode-thumb scanline ( -- n )
	tos push
	$40 ## tos mov,
	20 ## tos tos lsl,	\ REGISTERS
	tos 6 #( tos ldrh,
end-code

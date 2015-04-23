{ -- MF/TIN CORE (always include) functions
  -- Copyright (c) 2003 by Jeff Massung }

\ inlined peek and poke values into GBA memory
icode-thumb peek 0 ( a -- h ) tos 0@ tos ldrh, end-code
icode-thumb peekb 0 ( a -- h ) tos 0@ tos ldrb, end-code
icode-thumb peekw 0 ( a -- h ) tos 0@ tos ldr, end-code

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

\ return the current scanline
icode-thumb scanline 4 ( -- n )
	tos push
	$40 ## tos mov,
	20 ## tos tos lsl,	\ REGISTERS
	tos 6 #( tos ldrh,
end-code

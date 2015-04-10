{ -- MF/TIN string library
  -- Copyright (c) 2003 by Jeff Massung }

\ return the length of a string (inlined)
icode-thumb len ( a -- u )
	tos 0@ tos ldrb,
end-code

\ return the address and length of a string (inlined)
icode-thumb count ( a -- a+1 u )
	tos w mov,
	w 0@ tos ldrb,
	1 ## w w add,
	w push
end-code

\ resize the length of a string
code-thumb resize ( a u -- )
	w pop
	w 0@ tos strb,
	tos pop
	ret
end-code

\ map a string to a screenblock
code-thumb print ( tile a -- )
	v1 pop
	tos w mov,
	
	\ get length and quit if zero
	w 0@ a ldrb,
	1 ## w w add,
	0 ## a cmp,
	6 #offset ne? b,
	l: __return
	tos pop
	ret
	
	l: __print
	
	w 0@ tos ldrb,
	1 ## w w add,

	\ load tile and mask
	v1 0@ v2 ldrh,
	$fc ## v0 mov,
	8 ## v0 v0 lsl,
	v0 v2 and,
	
	\ add byte and write
	tos v2 v2 add,
	v1 0@ v2 strh,
	2 ## v1 add,
	
	\ decrement
	1 ## a sub,	\ subs, actually
	__print gt? b,
	
	\ done
	tos pop
	ret
end-code

\ write the length of a string and return
code-thumb #str ( x -- )
	\ note: w contains adress of string and v0 is length
	w 0@ v0 strb,
	tos pop
	ret
end-code

\ pop characters off the stack into RAM
code-thumb pop-string ( x -- )
	\ note: w contains address of string and v0 is length
	\ tos contains the first byte to pop onto string
	
	\ load tos into first byte
	w 1 #( tos strb,
	
	\ begin at index 1
	2 ## v2 mov,
	
	\ loop through all other characters
	l: __pop
	v0 v2 cmp,
	#str gt? b,

	a pop
	w v2 +( a strb,
	
	\ loop until all characters popped
	1 ## v2 add,
	__pop b,
end-code

\ convert a signed integer to a base 10 string
code-thumb /str ( a u -- )
	w pop

	1 ## v0 mov,		\ length
	
	0 ## v1 mov,		\ reset negative flag
	v1 v3 mov,

	\ force absolute value
	0 ## tos cmp,
	__pos pl? b,
	tos tos neg,
	tos v3 mov,		\ set negative flag
	l: __pos
	
	\ test for 0
	0 ## tos cmp,
	__loop ne? b,
	$30 ## tos mov,
	pop-string b,
	
	l: __loop
	10 ## tos cmp,

	__ge10 ge? b,
	$30 ## tos add,

	v3 v1 mov,
	0 ## v1 cmp,
	pop-string eq? b,	\ positive number

	tos push
	1 ## v0 add,
	45 ## tos mov,		\ negative number, add '-'
	pop-string b,

	l: __ge10
	1 ## v0 add,
	10 ## a mov,
	6 swi, 			\ divide by 10
	$30 ## a add,		\ store remainder
	a push 			\ as a decimal character
	__loop b,
end-code

\ convert an unsigned integer to a base 16 string
code-thumb /hex ( a u -- )
	w pop
	1 ## v0 mov, \ length
	
	\ test for 0
	0 ## tos cmp,
	__last_digit eq? b,
	
	l: __loop
	16 ## tos cmp,
	__last_digit lo? b,
	
	1 ## v0 add,		\ 1 more digit
	$f ## v1 mov,
	tos a mov,
	v1 a and,		\ divide by 16
	4 ## tos tos lsr,	\ store remainder
	
	\ 0-9 or A-F?
	$a ## a cmp,
	__digit_dec lt? b,
	
	\ A-F
	$a ## a sub,
	$41 ## a add,
	__push_char b,
	
	\ 0-9
l: __digit_dec
	$30 ## a add,

l: __push_char
	a push			\ as a hex character
	__loop b,
	
l: __last_digit
	\ 0-9 or A-F?
	$a ## tos cmp,
	__last_digit_dec lt? b,
	
	\ A-F
	$a ## tos sub,
	$41 ## tos add,
	pop-string b,
	
	\ 0-9
l: __last_digit_dec
	$30 ## tos add,
	
	\ finish
	pop-string b,
end-code

variable string_ring_ptr

\ initialize string ring pointer
code-thumb .init_string_ring
	string_ring v1 LITERAL		\ ring start -> v1
	string_ring_ptr v0 LITERAL
	v0 0@ v1 str,
	ret
end-code

\ allocate space in the string ring buffer
code-thumb s-alloc ( u -- a )
	tos w mov,			\ size -> w

	string_ring v1 LITERAL		\ ring start -> v1
	string_ring_ptr v0 LITERAL
	v0 0@ tos ldr,			\ current pos -> v0

	tos w w add,			\ new ring ptr -> w

	\ check if we have overstepped the bounds of the ring buffer
	string_ring_size v2 movi
	v1 v2 v2 add,			\ end of ring buffer -> v2
	v2 w cmp,			\ still within ring buffer?
	__ok lt? b,
	tos w w sub,			\ get back the size
	v1 w w add,			\ &ring[size] -> w
	v1 tos mov,			\ &ring[0] -> tos

	l: __ok
	v0 0@ w str,			\ write back new ring pos
	ret
end-code

\ convert a number to a string and return address
: str$ ( n -- a ) a! 12 # s-alloc dup a@ /str ;
: hex$ ( n -- a ) a! 12 # s-alloc dup a@ /hex ;

\ convert between numbers and characters
: chr$ ( n -- a ) 4 # s-alloc swap 8 # n* 1 # + OVER ! ;
: asc ( a -- n ) 1 # + peekb ;

\ transfer bytes from one address to address in A
code-thumb -> ( from count -- a ) ( A: to -- to+count )
	w pop
	
	\ save count and destination address
	tos v0 mov,	
	a tos mov,
	
	l: __copy
	\ decrement count
	1 ## v0 sub,		\ subs, actually
	12 #offset mi? b,
	
	\ load byte to transfer
	w 0@ v2 ldrb,
	1 ## w w add,
	a 0@ v2 strb,
	1 ## a a add,
	__copy b,

	ret
end-code

\ copy from one string into another
: move ( to from -- ) dup len 1+ copy ;

\ append one string onto another
: append$ ( to from -- a )
	256 # s-alloc 1+ a! swap count
		-> dup 1 # - push swap count -> drop 
	a@ swap - r@ swap resize pop ;

\ grab the beginning of a string
: left$ ( from count -- a )
	256 # s-alloc a! swap over 1+
		-> dup a! swap resize a@ ;

\ grab the end of a string
: right$ ( from count -- a )
	256 # s-alloc 1+ a! push
		dup len r@ - 1+ +
	r@ -> 1 # - dup pop resize ;

\ grab the middle of a string
: mid$ ( from start count -- a )
	256 # s-alloc 1+ a! push + 1+
		r@ -> 1 # - dup pop resize ;

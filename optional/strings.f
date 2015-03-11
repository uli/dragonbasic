{ -- MF/TIN string library
  -- Copyright (c) 2003 by Jeff Massung }

\ return the length of a string (inlined)
icode-thumb len ( a -- u )
	tos 0@ tos ldrb,
end-code

\ return the address and length of a string (inlined)
icode count ( a -- a+1 u )
	tos w mov,
	w 1 (# tos ldrh,
	$ff ## tos tos and,
	w push
end-code

\ resize the length of a string
code resize ( a u -- )
	w pop
	w 0@ v0 ldrh,
	
	\ mask
	$ff00 ## v0 v0 and,
	$ff ## tos tos and,
	
	\ set
	tos v0 tos add,
	w 0@ tos strh,
	
	\ done
	tos pop
	ret
end-code

\ map a string to a screenblock
code-thumb print ( tile a -- )
	v1 pop
	tos w mov,
	
	\ get length and quit if zero
	w 0@ v0 ldrh,
	2 ## w w add,
	$ff ## a mov,
	v0 a and,	\ ands, actually
	6 #offset ne? b,
	l: __return
	tos pop
	ret
	
	\ loop, writing 2 bytes at a time
	l: __print
	
	\ get upper byte
	8 ## v0 tos lsr,
	
	\ load tile and mask
	v1 0@ v2 ldrh,
	$fc ## v0 mov,
	8 ## v0 v0 lsl,
	v0 v2 and,
	
	\ add upper byte and write
	tos v2 v2 add,
	v1 0@ v2 strh,
	2 ## v1 add,
	
	\ decrement
	1 ## a sub,	\ subs, actually
	__return eq? b,
	
	\ lower byte
	w 0@ v0 ldrh,
	2 ## w add,
	
	\ load tile and mask
	v1 0@ v2 ldrh,
	$fc ## tos mov,
	8 ## tos tos lsl,
	tos v2 and,
	
	\ add upper byte and write
	$ff ## tos mov,
	v0 tos and,
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
code #str ( x -- )
	\ note: w contains adress of string and v7 is length
	w 0@ v0 ldrh,
	$ff00 ## v0 v0 and,
	v0 v7 v0 orr,
	w 0@ v0 strh,
	
	\ done
	tos pop
	ret
end-code

\ pop characters off the stack into RAM
code pop-string ( x -- )
	\ note: w contains address of string and v7 is length
	\ tos contains the first byte to pop onto string
	
	\ load tos into first byte
	w 0@ v0 ldrh,
	$ff ## v0 v0 and,
	tos 8 #lsl v0 v0 add,
	w 0@ v0 strh,
	
	\ begin at index 1
	2 ## v6 mov,
	
	\ loop through all other characters
	l: __pop
	v7 v6 cmp,
	#str gt? b,
	1 ## v6 tst,
	1 ## v6 v5 bic,
	a pop
	
	\ odd index = upper byte
	w v5 +( v0 ldrh,
	$00ff ## v0 v0 ne? and,
	a 8 #lsl v0 v0 ne? add,
	$ff00 ## v0 v0 eq? and,
	a v0 v0 eq? add,
	w v5 +( v0 strh,
	
	\ loop until all characters popped
	1 ## v6 v6 add,
	__pop b,
end-code

\ convert an unsigned integer to a base 10 string
code /str ( a u -- )
	w pop
	1 ## v7 mov, \ length
	
	\ force absolute value
	0 ## tos cmp,
	0 ## tos tos mi? rsb,
	
	\ test for 0
	0 ## tos cmp,
	$30 ## tos eq? mov,
	\ tos eq? push
	pop-string eq? b,
	
	l: __loop
	10 ## tos cmp,
	$30 ## tos tos lt? add,
	pop-string lt? b,
	1 ## v7 v7 add,
	10 ## a mov,
	6 swi, 			\ divide by 10
	$30 ## a a add, \ store remainder
	a push 			\ as a decimal character
	__loop b,
end-code

\ convert an unsigned integer to a base 16 string
code /hex ( a u -- )
	w pop
	1 ## v7 mov, \ length
	
	\ test for 0
	0 ## tos cmp,
	48 #offset eq? b,
	
	l: __loop
	16 ## tos cmp,
	40 #offset lt? b,
	
	1 ## v7 v7 add,		\ 1 more digit
	$f ## tos a and,	\ divide by 16
	tos 4 #lsr tos mov,	\ store remainder
	
	\ 0-9 or A-F?
	$a ## a cmp,
	
	\ A-F
	$a ## a a ge? sub,
	$41 ## a a ge? add,
	
	\ 0-9
	$30 ## a a lt? add,	
	a push 				\ as a hex character
	__loop b,
	
	\ 0-9 or A-F?
	$a ## tos cmp,
	
	\ A-F
	$a ## tos tos ge? sub,
	$41 ## tos tos ge? add,
	
	\ 0-9
	$30 ## tos tos lt? add,
	
	\ finish
	pop-string b,
end-code

\ convert a number to a string and return address
: str$ ( n -- a ) a! 256 # r-alloc dup a /str ;
: hex$ ( n -- a ) a! 256 # r-alloc dup a /hex ;

\ transfer bytes from one address to address in A
code -> ( from count -- a ) ( A: to -- to+count )
	w pop
	
	\ save count and destination address
	tos v0 mov,	
	a tos mov,
	
	l: __copy
	
	\ decrement count
	1 ## v0 v0 s! sub,
	mi? ret
	
	\ load byte to transfer
	w 1 (# v3 ldrh,
	$ff ## v3 v3 and,
	1 ## a tst,
	
	\ just write the low byte
	a 1 (# v3 eq? strh,
	v3 v4 eq? mov, \ save
	__copy eq? b,
	
	\ mask and write the upper byte
	v3 8 #lsl v4 v3 add,
	1 ## a v1 bic,
	v1 0@ v3 strh,
	1 ## a a add,
	__copy b,
end-code

\ copy from one string into another
: move ( to from -- ) dup len 1+ copy ;

\ append one string onto another
: append$ ( to from -- a )
	256 # r-alloc 1+ a! swap count
		-> dup 1 # - push swap count -> drop 
	a swap - r@ swap resize pop ;

\ grab the beginning of a string
: left$ ( from count -- a )
	256 # r-alloc a! swap over 1+
		-> dup a! swap resize a ;

\ grab the end of a string
: right$ ( from count -- a )
	256 # r-alloc 1+ a! push 
		dup len r@ - 1+ +
	r@ -> 1 # - dup pop resize ;

\ grab the middle of a string
: mid$ ( from start count -- a )
	256 # r-alloc 1+ a! push + 1+
		r@ -> 1 # - dup pop resize ;
{ -- MF/TIN input library
  -- Copyright (c) 2003 by Jeff Massung }

\ wait for a button state to change
code-thumb input ( n -- )
	lr push
	$4000130 v1 LITERAL	\ REGISTERS + $130
	v1 0@ v2 ldrh,
	tos v2 and,
	
	\ wait until the mask changes
	l: __wait
	v1 v2 push
	vblank	bl,
	v1 v2 pop

	v1 0@ v0 ldrh,
	tos v0 and,
	v2 v0 eor,	\ eors, actually
	__wait eq? b,
	
	\ done
	v0 pop
	tos pop
	v0 bx,
end-code

\ return the status of a button (0=released)
icode-thumb key ( n1 -- n2 )
	REGISTERS v1 movi
	$f8 ## v1 add,		\ REGISTERS + $f8
	v1 $38 #( v2 ldrh,	\ REGISTERS + $130
	tos v2 and,
	v2 tos eor,
end-code

\ return the status of all buttons (0=released)
code-thumb keys ( -- n )
	tos push
	$4000130 v1 LITERAL
	v1 0@ tos ldrh,
	tos tos mvn,
	$3ff v1 LITERAL
	v1 tos and,
	ret
end-code

\ wait for button in mask to be pressed and released
code-thumb waitkey ( n1 -- n2 )
	lr push
	$4000130 v1 LITERAL
	
	\ wait for any button in the mask
	l: __press
	v1 push
	vblank	bl,
	v1 pop

	v1 0@ v2 ldrh,
	tos v2 and,
	tos v2 eor,	\ eors, actually
	__press eq? b,
	
	\ wait for the same button to be released
	l: __depress
	v1 v2 push
	vblank	bl,
	v1 v2 pop

	v1 0@ v0 ldrh,
	v2 v0 and,
	v2 v0 cmp,
	__depress ne? b,
	
	\ done
	v2 tos mov,
	v0 pop
	v0 bx,
end-code

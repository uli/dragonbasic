{ -- MF/TIN input library
  -- Copyright (c) 2003 by Jeff Massung }

\ wait for a button state to change
code-thumb input ( n -- )
	$4000130 v1 LITERAL	\ REGISTERS + $130
	v1 0@ v2 ldrh,
	tos v2 and,
	
	\ wait until the mask changes
	l: __wait
	v1 0@ v0 ldrh,
	tos v0 and,
	v2 v0 eor,	\ eors, actually
	__wait eq? b,
	
	\ done
	tos pop
	ret
end-code

\ return the status of a button (0=released)
code-thumb key ( n1 -- n2 )
	$4000130 v1 LITERAL	\ REGISTERS + $130
	v1 0@ v2 ldrh,
	tos v2 and,
	v2 tos eor,
	ret
end-code

\ return the status of all buttons (0=released)
code keys ( -- n )
	tos push
	REGISTERS ## v1 mov,
	$130 ## v1 v1 add,
	v1 0@ tos ldrh,
	tos tos mvn,
	ret
end-code

\ wait for button in mask to be pressed and released
code waitkey ( n1 -- n2 )
	REGISTERS ## v1 mov,
	$130 ## v1 v1 add,
	
	\ wait for any button in the mask
	l: __press
	v1 0@ v2 ldrh,
	v2 tos v2 and,
	v2 tos v2 s! eor,
	__press eq? b,
	
	\ wait for the same button to be released
	l: __depress
	v1 0@ v3 ldrh,
	v3 v2 v3 and,
	v2 v3 cmp,
	__depress ne? b,
	
	\ done
	v2 tos mov,
	ret
end-code
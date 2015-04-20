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

\ NB: The code below depends on the order of these variables!
variable (last_key_state)
variable (down_keys)
variable (up_keys)

code-thumb checkkeys
	$4000130 v1 LITERAL
	v1 0@ v0 ldrh,
	v0 v0 mvn,
	$3ff v1 LITERAL
	v1 v0 and,

	(last_key_state) v1 LITERAL
	v1 0@ v2 ldr,		\ previous key state -> v2
	v1 0@ v0 str,		\ current key state -> (last_key_state)

	v0 v2 eor,		\ keys changed -> v2
	v2 v0 and,		\ keys pressed -> v0
	v1 4 #( v0 str,		\ keys pressed -> (down_keys)

	\ Anything that has changed, but is not a key down event is
	\ a key up event.
	v0 v0 mvn,		\ keys not pressed -> v0
	v0 v2 and,		\ keys released -> v2
	v1 8 #( v2 str,		\ keys released -> (up_keys)
	lr bx,
end-code

code-thumb keydown ( n1 -- n2 )
	(down_keys) v1 LITERAL
	v1 0@ v1 ldr,
	v1 tos and,
	lr bx,
end-code

code-thumb keyup ( n1 -- n2 )
	(up_keys) v1 LITERAL
	v1 0@ v1 ldr,
	v1 tos and,
	lr bx,
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

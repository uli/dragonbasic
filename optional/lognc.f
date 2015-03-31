{ -- NO$GBA logging functions
  --
  -- Copyright (C) 2015 Ulrich Hecht
  --
  -- This software is provided 'as-is', without any express or implied
  -- warranty.  In no event will the authors be held liable for any damages
  -- arising from the use of this software.
  --
  -- Permission is granted to anyone to use this software for any purpose,
  -- including commercial applications, and to alter it and redistribute it
  -- freely, subject to the following restrictions:
  --
  -- 1. The origin of this software must not be misrepresented; you must not
  --    claim that you wrote the original software. If you use this software
  --    in a product, an acknowledgment in the product documentation would be
  --    appreciated but is not required.
  -- 2. Altered source versions must be plainly marked as such, and must not be
  --    misrepresented as being the original software.
  -- 3. This notice may not be removed or altered from any source distribution.
  --
  -- Ulrich Hecht
  -- ulrich.hecht@gmail.com }

\ send a string to the NO$GBA console
code log ( a -- )
	\ NO$GBA only allows inline debug strings, so
	\ we construct a suitable code snippet at runtime
	\ in EWRAM.
	\ XXX: This address should not be fixed!
	$203c000 a LITERAL

	$e1a0c00c v0 LITERAL	\ mov r12, r12
	a 0@ v0 str,
	$ea00003e v0 LITERAL	\ b   pc+256
	a 4 #( v0 str,
	$6464 v0 LITERAL	\ magic number
	a 8 #( v0 str,

	tos 1 (# v0 ldrb,	\ string length
	12 ## a v2 add,		\ start of string in EWRAM

	\ copy string to the inside of our code fragment
	l: loop
		tos 1 (# v1 ldrb,
		v2 1 (# v1 strb,
		1 ## v0 v0 s! sub,
		loop ne? b,

	\ zero-terminate
	0 ## v0 mov,
	v2 1 (# v0 strb,

	260 ## a v2 add,
	$e12fff1e v0 LITERAL	\ ret
	v2 0@ v0 str,

	\ call our code fragment
	lr push
	pc lr mov,
	a bx,
	lr pop

	tos pop
	ret
end-code

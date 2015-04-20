{ -- MF/TIN SRAM functions
  -- Original code by Jeff Massung, 2003 }

\ shift masking
:i /8 ( n1 -- n1 n2 ) dup 8 # n/ ;
:n *8 ( n1 n2 -- n3 ) swap 8 # n* + ;

\ save data to static ram
:n savebyte ( b -- ) >a c!a a> ;
:n saveword ( n -- ) >a /8 c!a c!a a> ;
:n savelong ( x -- ) >a /8 /8 /8 c!a c!a c!a c!a a> ;

\ load data from static ram
:n loadbyte ( -- b ) >a c@a a> ;
: loadword ( -- n ) >a c@a c@a *8 a> ;
: loadlong ( -- x ) >a c@a c@a *8 c@a *8 c@a *8 a> ;

\ save a string
code-thumb /savetext ( a -- )
	$100 w movi
	
	l: __loop
	
	\ copy a byte
	tos 0@ v0 ldrb,
	1 ## tos add,
	a 0@ v0 strb,
	1 ## a add,
	
	\ loop until 256 bytes written
	1 ## w sub,
	__loop gt? b,
	
	\ done
	tos pop
	ret
end-code

\ load a string
code-thumb /loadtext ( a -- a )
	$100 w movi
	tos v2 mov,
	
	l: __loop
	
	\ load two bytes
	a 0@ v1 ldrb,
	1 ## a add,
	
	\ write bytes
	v2 0@ v1 strb,
	1 ## v2 add,
	
	\ loop until length is zero
	1 ## w sub,
	__loop gt? b,
	
	\ done
	ret
end-code

\ save a string to static ram
: savetext ( a -- ) >a /savetext a> ;
: loadtext$ ( -- a ) >a 256 # r-alloc /loadtext a> ;

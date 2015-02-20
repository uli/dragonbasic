{ -- MF/TIN SRAM functions
  -- Original code by Jeff Massung, 2003 }

\ shift masking
: /8 ( n1 -- n1 n2 ) dup 8 # n/ ;
: *8 ( n1 n2 -- n3 ) swap 8 # n* + ;

\ save data to static ram
: savebyte ( b -- ) >a c!a a> ;
: saveword ( n -- ) >a /8 c!a c!a a> ;
: savelong ( x -- ) >a /8 /8 /8 c!a c!a c!a c!a a> ;

\ load data from static ram
: loadbyte ( -- b ) >a c@a a> ;
: loadword ( -- n ) >a c@a c@a *8 a> ;
: loadlong ( -- x ) >a c@a c@a *8 c@a *8 c@a *8 a> ;

\ save a string
code /savetext ( a -- )
	$100 ## w s! mov,
	
	l: __loop
	
	\ copy a byte
	tos 1 (# v0 ldrh,
	a 1 (# v0 strb,
	
	\ loop until 256 bytes written
	1 ## w w s! sub,
	__loop gt? b,
	
	\ done
	tos pop
	ret
end-code

\ load a string
code /loadtext ( a -- a )
	$100 ## w s! mov,
	tos v7 mov,
	
	l: __loop
	
	\ load two bytes
	a 1 (# v1 ldrb,
	a 1 (# v0 ldrb,
	
	\ write bytes
	v0 8 #lsl v1 v1 add,
	v7 2 (# v1 strh,
	
	\ loop until length is zero
	2 ## w w s! sub,
	__loop gt? b,
	
	\ done
	ret
end-code

\ save a string to static ram
: savetext ( a -- ) >a /savetext a> ;
: loadtext$ ( -- a ) >a 256 # r-alloc /loadtext a> ;
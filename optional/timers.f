{ -- MF/TIN TIMER functions
  -- Original code by Jeff Massung, 2003 }

\ create a new timer
code-thumb maketimer ( frequency -- )
	$ffff v0 literal
	
	\ divide for real frequency
	tos a mov,
	v0 tos mov,
	6 swi,
	
	\ subtract and clear remainder
	tos v0 tos sub,
	a a eor,
	
	\ offset to timer registers
	$4000100 v0 LITERAL
	
	\ set frequency and clear
	v0 $8 #( tos strh,
	v0 $a #( a strh,
	v0 $c #( a strh,
	v0 $e #( a strh,
	
	\ done
	tos pop
	ret
end-code

\ start the user timer
code-thumb starttimer ( -- )
	\ offset to timer registers and interrupts
	$4000100 v2 LITERAL	\ REGISTERS + $100
	IWRAM_GLOBALS v0 LITERAL
	
	\ ENABLE + FREQ_64 + IRQ
	$c2 ## v0 mov,

	v2 $a #( v0 strh,
	
	\ set timer 3 to overflow (ENABLE + COUNT_UP)
	$84 ## v1 mov,
	v2 $e #( v1 strh,

	\ enable timer 2 in IE
	$f0 ## v2 add,
	$20 ## v1 mov,
	v2 $10 #( v0 ldrh,
	v1 v0 orr,
	v2 $10 #( v0 strh,

	ret
end-code

\ stop the user timer
code-thumb stoptimer ( -- )
	\ offset to timer registers
	$4000100 v2 LITERAL	\ REGISTERS + $100
	
	\ clear and write
	v1 v1 eor,
	v2 $c #( v1 strh,
	v2 $a #( v1 strh,
	v2 $e #( v1 strh,
	
	\ disable timer 2 in IE
	$f0 ## v2 add,
	$20 ## v1 mov,
	v2 $10 #( v0 ldrh,
	v1 v0 bic,
	v2 $10 #( v0 strh,

	\ done
	ret
end-code

\ count the timer fires
code-thumb clocktimer ( -- u )
	tos push
	
	\ offset to timer registers
	$4000100 tos LITERAL	\ REGISTERS + $100
	
	\ load number of overflows into timer 3
	tos $c #( tos ldrh,
	ret
end-code

\ wait for so many fires of the timer
code-thumb waittimer ( u -- )
	\ offset to timer registers
	$4000100 v2 LITERAL	\ REGISTERS + $100
	
	\ loop
	l: __wait
	2 swi,			\ BIOS Halt
	v2 $c #( v1 ldrh,
	tos v1 cmp,
	__wait lt? b,
	
	\ done
	tos pop
	ret
end-code

\ reset the number of fires
code-thumb resettimer ( -- )
	\ offset to timer registers
	$4000100 v2 LITERAL	\ REGISTERS + $100
	
	\ turn off timers before resetting
	0 ## v1 mov,
	v2 $e #( v1 strh,
	v2 $c #( v1 strh,
	
	\ turn back on
	$84 ## v1 mov,
	v2 $e #( v1 strh,
	ret
end-code

{ -- MF/TIN TIMER functions
  -- Original code by Jeff Massung, 2003 }

\ create a new timer
code maketimer ( frequency -- )
	$ffff v0 literal
	
	\ divide for real frequency
	tos a mov,
	v0 tos mov,
	6 swi,
	
	\ subtract and clear remainder
	tos v0 tos sub,
	a a a eor,
	
	\ offset to timer registers
	REGISTERS ## v0 mov,
	$100 ## v0 v0 add,
	
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
	
	\ test to see if an interrupt is running
	v0 INT_T2 #( v1 ldr,
	$82 ## v0 mov, 
	
	\ ENABLE + FREQ_64 + IRQ (if running)
	0 ## v1 cmp,
	4 #offset eq? b,
	$40 ## v0 add,

	v2 $a #( v0 strh,
	
	\ set timer 3 to overflow (ENABLE + COUNT_UP)
	$84 ## v1 mov,
	v2 $e #( v1 strh,
	ret
end-code

\ stop the user timer
code stoptimer ( -- )
	\ offset to timer registers
	REGISTERS ## v2 mov,
	$100 ## v2 v2 add,
	
	\ clear and write
	v1 v1 v1 eor,
	v2 $c #( v1 strh,
	v2 $a #( v1 strh,
	v2 $e #( v1 strh,
	
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
code waittimer ( u -- )
	\ offset to timer registers
	REGISTERS ## v2 mov,
	$100 ## v2 v2 add,
	
	\ loop
	l: __wait
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

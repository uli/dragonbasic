{ -- MF/TIN INTERRUPT functions
  -- Original code by Jeff Massung, 2003 }

\ turn interrupts on
: enableinterrupts ( -- )
	$4000208 # ( REG_IF) 0 # com poke ;

\ turn interrupts off
: disableinterrupts ( -- )
	$4000208 # ( REG_IF) 0 # poke ;

\ setup an interrupt event handler (keypad)
code onkey ( addr n1 n2 -- )
	sp ia! r1 r2 r3 ldm,
	$4000000 ## r9 mov,
	$200 ## r9 r9 add,
	
	\ write mask to REG_KEYCNT
	0 ## r0 cmp,
	$4000 ## r1 r1 add,
	$8000 ## r1 r1 ne? add,
	r9 -206 #( r1 strh,
	
	\ write address of interrupt handler
	IWRAM ## r8 mov,
	GLOBALS ## r8 r8 add,
	r8 INT_P1 #( r2 str,
	0 ## r2 cmp,
	
	\ set key interrupt flag
	r9 0@ r2 ldrh,
	
	\ if the address is zero then turn off the interrupt
	$1000 ## r2 r2 eq? bic,
	$1000 ## r2 r2 ne? orr,
	r9 0@ r2 strh,
	
	\ done
	r3 r0 mov,
	ret
end-code

\ setup an interrupt event handler (user timer)
code ontimer ( a -- )
	\ offset for timer registers
	REGISTERS ## v0 mov,
	$100 ## v0 v0 add,
	
	\ test for no interrupt
	0 ## tos cmp,
	
	\ clear IRQ bit or set it for timer 2
	v0 $a #( v1 ldrh,
	$40 ## v1 v1 eq? bic,
	$40 ## v1 v1 ne? orr,
	v0 $a #( v1 strh,
	
	\ offset to interrupt registers
	$100 ## v0 v0 add,
	
	\ write address of interrupt handler
	IWRAM ## v2 mov,
	GLOBALS ## v2 v2 add,
	v2 INT_T2 #( tos str,
	
	\ set timer interrupt flag (or clear)
	v0 0@ tos ldrh,
	$20 ## tos tos eq? bic,
	$20 ## tos tos ne? orr,
	v0 0@ tos strh,
	
	\ done
	tos pop
	ret
end-code

\ setup an interrupt event handler (horizontal blank)
code onhblank ( a -- )
	REGISTERS ## v3 mov,
	0 ## tos cmp,
	
	\ enable hblank bit in REG_DISPSTAT
	v3 4 #( v1 ldrh,
	$10 ## v1 v1 eq? bic,
	$10 ## v1 v1 ne? orr,
	v3 4 #( v1 strh,
	
	\ offset to interrupt registers
	$200 ## v3 v3 add,
	
	\ write address of interrupt handler
	IWRAM ## v2 mov,
	GLOBALS ## v2 v2 add,
	v2 INT_HB #( tos str,
	
	\ set hblank interrupt flag
	v3 0@ tos ldrh,
	$2 ## tos tos eq? bic,
	$2 ## tos tos ne? orr,
	v3 0@ tos strh,
	
	\ done
	tos pop
	ret
end-code

\ setup an interrupt event handler (vertical blank)
code onvblank ( a -- )
	REGISTERS ## v3 mov,
	0 ## tos cmp,
	
	\ enable vblank bit in REG_DISPSTAT
	v3 4 #( v1 ldrh,
	$8 ## v1 v1 eq? bic,
	$8 ## v1 v1 ne? orr,
	v3 4 #( v1 strh,
	
	\ offset to interrupt registers
	$200 ## v3 v3 add,
	
	\ write address of interrupt handler
	IWRAM ## v2 mov,
	GLOBALS ## v2 v2 add,
	v2 INT_VB #( tos str,
	
	\ set vblank interrupt flag
	v3 0@ tos ldrh,
	$1 ## tos tos eq? bic,
	$1 ## tos tos ne? orr,
	v3 0@ tos strh,
	
	\ done
	tos pop
	ret
end-code

\ setup an interrupt event handler (vertical count)
code onvcount ( a count -- )
	w pop
	
	\ address of registers
	REGISTERS ## v3 mov,
	0 ## w cmp,
	
	\ enable vcountbit in REG_DISPSTAT
	v3 4 #( v1 ldrh,
	$20 ## v1 v1 eq? bic,
	$20 ## v1 v1 ne? orr,
	tos 8 #lsl v1 v1 add, \ count
	v3 4 #( v1 strh,
	
	\ offset to interrupt registers
	$200 ## v3 v3 add,
	
	\ write address of interrupt handler
	IWRAM ## v2 mov,
	GLOBALS ## v2 v2 add,
	v2 INT_VC #( w str,
	
	\ set vblank interrupt flag
	v3 0@ tos ldrh,
	$4 ## tos tos eq? bic,
	$4 ## tos tos ne? orr,
	v3 0@ tos strh,
	
	\ done
	tos pop
	ret
end-code

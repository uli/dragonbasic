{ -- MF/TIN extended CORE functions
  -- Original code by Jeff Massung, 2003 }

\ word aligned address
icode-thumb aligned ( a1 -- a2 )
	3 ## tos add,
	3 ## v0 mov,
	v0 tos bic,
end-code

\ dynamically allocate memory on return stack
: allocate ( b -- a ) aligned r-alloc ;

\ copy data using DMA 3
code-thumb dmacopy ( addr1 addr2 u -- )
	v1 v2 pop
	$40000d0 v0 LITERAL	\ REGISTERS + $d0
	
	\ setup dma transfer addresses
	v0 $4 #( v1 str,	\ REGISTERS + $d4
	v0 $8 #( v2 str,	\ REGISTERS + $d8
	
	\ set control to 32-bit, increment both
	$84 ## v1 mov,
	24 ## v1 v1 lsl,	\ $84000000
	v1 tos orr,
	v0 $c #( tos str,	\ REGISTERS + $dc
	
	\ done
	tos pop
	ret
end-code

\ wait for the dma to finish
code dmawait ( -- )
	REGISTERS ## w mov,
	
	\ wait for start bit to clear
	l: __wait
	w $dc #( v0 ldr,
	$80000000 ## v0 tst,
	__wait ne? b,
	
	\ done
	ret
end-code

\ return the number of blocks from a image size
code-thumb blocks ( w h d -- u )
	v0 v1 pop
	
	\ multiply WxH div 64
	v1 v0 mul,
	6 ## v0 v0 lsr,
	
	\ multiply by bit depth
	3 ## tos tos lsr,
	tos v0 lsl,
	v0 tos mov,
	
	\ done
	ret
end-code

\ return the number of bytes used by blocks (inlined)
icode tileoffset ( n -- u )
	tos 5 #lsl tos mov,
end-code

\ inlined absolute value
icode abs ( n1 -- n2 )
	0 ## tos cmp,
	0 ## tos tos mi? rsb,
end-code

\ inlined square root
icode sqrt ( n1 -- n2 )
	sp db! u stm,
	8 swi,
end-code

\ convert an integer to a fixed-point number (inline)
icode-thumb fix# ( n -- f )
	8 ## tos tos lsl,
end-code

\ convert a fixed-point value to an integer (inline)
icode-thumb int ( f -- n )
	8 ## tos tos asr,
end-code

\ round a fixed-point value to an integer
:n round ( f -- n ) $80 # + int ;

\ fixed-point square root
code fsqrt# ( f1 -- f2 )
	8 swi,
	tos 4 #lsl tos mov,
	ret
end-code
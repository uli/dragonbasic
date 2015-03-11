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
code dmacopy ( addr1 addr2 u -- )
	sp ia! v1 v2 ldm,
	$4000000 ## v0 mov,
	
	\ setup dma transfer addresses
	v0 $d4 #( v1 str,
	v0 $d8 #( v2 str,
	
	\ set control to 32-bit, increment both
	$84000000 ## tos tos orr,
	v0 $dc #( tos str,
	
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
icode fix# ( n -- f )
	tos 8 #lsl tos mov,
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
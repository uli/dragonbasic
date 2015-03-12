{ -- MF/TIN EFFECTS functions
  -- Original code by Jeff Massung, 2003 }

\ set or clear the mosaic bit for a background
code-thumb mosaictiles ( bg flag -- )
	w pop
	
	\ move to REG_BGxCNT
	$40 ## v2 mov,
	20 ## v2 v2 lsl,	\ REGISTERS
	1 ## tos tos lsl,
	8 ## tos add,
	
	\ set or reset bit in REG_BGxCNT
	v2 tos +( v1 ldrh,
	$40 ## v0 mov,
	v0 v1 bic,
	0 ## w cmp,
	4 #offset eq? b,
	v0 v1 orr,
	v2 tos +( v1 strh,
	
	\ done
	tos pop
	ret
end-code

\ set or reset the sprite mosaic bit of a sprite
code spritemosaic ( sprite flag -- )
	w pop
	0 ## tos cmp,
	
	\ address of sprite
	IWRAM ## v2 mov,
	w 3 #lsl v2 w add,
	
	\ load and clear or set
	w 0@ v0 ldrh,
	$1000 ## v0 v0 eq? bic,
	$1000 ## v0 v0 ne? orr,
	w 0@ v0 strh,
	
	\ done
	tos pop
	ret
end-code

\ set parameters of mosaic effect
code mosaic ( bgx bgy objx objy -- )
	sp ia! v1 v2 v3 ldm,

	\ sprite
	tos 12 #lsl tos mov,
	v1 8 #lsl tos tos orr,
	
	\ bg
	v2 4 #lsl tos tos orr,
	v3 tos tos orr,
	
	\ set
	REGISTERS ## v1 mov,
	v1 $4c #( tos str,

	\ done
	tos pop
	ret
end-code

\ fade the screen black
code fadeout ( -- )
	REGISTERS ## w mov,
	
	\ REG_BLDCNT = $EF
	$ef ## v0 mov,
	w $50 #( v0 strh,
	
	\ Loop v1 = 0-15
	0 ## v1 mov,
	
	l: __loop
	
	\ REG_COLEY = v1
	w $54 #( v1 strh,
	1 ## v1 v1 add,
	
	\ Wait for a vertical blank (160)
	l: __wait
	w 6 #( v2 ldrh,
	160 ## v2 cmp,
	__wait ne? b,
	
	\ Wait for a vertical blank (159)
	l: __wait
	w 6 #( v2 ldrh,
	159 ## v2 cmp,
	__wait ne? b,
	
	\ Loop 16 times
	$10 ## v1 cmp,
	__loop le? b,
	
	ret
end-code

\ fade the screen in
code fadein ( -- )
	REGISTERS ## w mov,
	
	\ REG_BLDCNT = $EF
	$ef ## v0 mov,
	w $50 #( v0 strh,
	
	\ Loop v1 = 0-15
	0 ## v1 mov,
	
	l: __loop
	
	\ REG_COLEY = 16-v1
	$10 ## v1 v3 rsb,
	w $54 #( v3 strh,
	1 ## v1 v1 add,
	
	\ Wait for a vertical blank (160)
	l: __wait
	w 6 #( v2 ldrh,
	160 ## v2 cmp,
	__wait ne? b,
	
	\ Wait for a vertical blank (159)
	l: __wait
	w 6 #( v2 ldrh,
	159 ## v2 cmp,
	__wait ne? b,
	
	\ Loop 16 times
	$10 ## v1 cmp,
	__loop le? b,
	
	ret
end-code
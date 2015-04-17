{ -- MF/TIN SOUND functions
  -- Original code by Jeff Massung, 2003 }

\ turn all sounds on
code-thumb turnsoundon ( -- )
	$40 ## v2 mov,
	20 ## v2 v2 lsl,
	$80 ## v2 add,
	
	\ $80 -> REG_SOUNDCNT_X
	$80 ## v1 mov,
	v2 $4 #( v1 strh,
	
	\ $FF77 -> REG_SOUNDCNT_L
	$ff ## v1 mov,
	8 ## v1 v1 lsl,
	$77 ## v0 mov,
	v0 v1 orr,
	v2 0@ v1 strh,
	
	\ 2 -> REG_SOUNDCNT_H
	2 ## v1 mov,
	v2 $2 #( v1 strh,
	
	\ done
	ret
end-code

\ turn all sounds off
code-thumb turnsoundoff ( -- )
	$40 ## v2 mov,
	20 ## v2 v2 lsl,
	$80 ## v2 add,
	
	\ 0 -> REG_SOUNDCNT_X
	0 ## v1 mov,
	v2 $4 #( v1 strh,
	
	\ done
	ret
end-code

\ stop all background WAV sound from playing
code-thumb stopsound ( -- )
	$40000d0 v0 LITERAL
	
	\ stop DMA 2 and timer 1
	0 ## w mov,
	v0 0@ w str,
	v0 $34 #( w str,		\ $4000104
	
	\ turn off DMA 2 interrupt in IE
	$f4 ## v0 add,			\ $40001c4
	v0 $3c #( w ldrh,		\ $4000200
	$400 v2 movi
	v2 w bic,
	v0 $3c #( w strh,		\ $4000200

	\ done
	ret
end-code

\ stop all background WAV music from playing
code-thumb stopmusic ( -- )
	$40000c4 v0 LITERAL
	v0 v2 mov,
	$fe ## v2 add,			\ $40001c2 (interrupts - $3e)
	
	\ turn off DMA 1 interrupt
	v2 $3e #( w ldrh,		\ $4000200
	$200 v1 movi
	v1 w bic,
	v2 $3e #( w strh,		\ $4000200
	
	\ stop DMA 1 and reset timer 0
	0 ## w mov,
	v0 0@ w str,			\ $40000c4
	v0 $3c #( w str,		\ $4000100 (timers)
	
	\ done
	ret
end-code

\ play a single note out channel 1 or 2
code-thumb playnote ( channel length freq duty -- )
	\ check for sweep control
	\ if = 0 then set bit 3
	\ hardware need that sometime
	$4000060 r4 LITERAL
	r4 0@ r3 ldrh,
	0 ## r3 cmp,
	6 #offset ne? b,
	8 ## r3 mov,
	r4 0@ r3 strh,

	\ load arguments
	r4 r5 pop

	\ set values to write to registers (R2=L, R3=H)
	$f0 ## r3 mov,
	8 ## r3 r3 lsl,	\ $f000

	0 ## r5 cmp,
	14 #offset le? b,
	$7 ## r2 mov,
	8 ## r2 r2 lsl,	\ $700
	r2 r3 r3 add,

	\ set the length of the tone
	r5 r5 neg,
	64 ## r5 add,
	r5 r3 r3 add,

	\ set the wave duty cycle
	r0 r3 orr,

	\ reset and timed bits
	$8 ## r2 mov,
	12 ## r2 r2 lsl,	\ $8000
	r4 r2 orr,
	0 ## r5 cmp,
	8 #offset le? b,
	$4 ## r0 mov,
	12 ## r0 r0 lsl,	\ $4000
	r0 r2 orr,

	\ compute offsets to sound registers
	$62 ## r1 mov,	\ r1 = $62 (channel 1)
	r5 pop
	1 ## r5 sub,	\ subs, actually
	4 #offset eq? b,
	$68 ## r1 mov,	\ r1 = $68 (channel 2)

	\ set default values for notes
	$40 ## r4 mov,
	20 ## r4 r4 lsl,	\ REGISTERS
	r4 r1 +( r3 strh,
	2 ## r1 r1 add,
	0 ## r5 cmp,
	4 #offset eq? b,
	2 ## r1 add,
	r4 r1 +( r2 strh,

	\ done
	tos pop
	ret
end-code

\ use channel 4 to produce white noise
code-thumb playdrum ( length freq step -- )
	\ setup
	$40 ## v2 mov,
	20 ## v2 v2 lsl,	\ REGISTERS
	$78 ## v2 add,
	$f7 ## v1 mov,
	8 ## v1 v1 lsl,		\ $f700

	\ load arguments
	a w pop
	w v0 mov,
	0 ## w cmp,
	8 #offset le? b,

	\ set envelope step and length (REG_SOUND4CNT_L)
	64 ## w add,
	w w neg,
	w v1 v1 add,
	v2 0@ v1 strh,

	\ set loop mode (REG_SOUND4CNT_H)
	$80 ## v1 mov,
	8 ## v1 v1 lsl,		\ $8000, reset
	0 ## v0 cmp,
	8 #offset le? b,
	$40 ## v0 mov,
	8 ## v0 v0 lsl,		\ $4000, timed
	v0 v1 orr,
	4 ## tos tos lsl,
	tos v1 orr,		\ step
	a v1 orr,		\ frequency
	v2 4 #( v1 strh,

	\ done
	tos pop
	ret
end-code

\ interrupt handler for playsound
code iwram /playsound ( -- )
	IWRAM r4 movi

	\ load sound samples and process
	r4 $60c #( r2 ldr,
	16 ## r2 r2 s! sub,
	__write_back pl? b,

	REGISTERS r1 movi
	0 ## r0 mov,
	
	\ stop DMA 2 transfer and timer 1 if done
	r1 $d0 #( r0 str,	\ DMA2CNT
	r1 $104 #( r0 str,	\ TM1CNT
	r1 $a4 #( r0 str,	\ FIFO_B

	\ write samples back
l: __write_back
	r4 $60c #( r2 str,
	
	\ done
	ret
end-code

\ play a wav sample 1 time on DirectSound channel 1
code-thumb playsound ( a -- )
	r6 push
	$4000080 r3 LITERAL	\ REGISTERS + $80
	$4000200 r4 LITERAL	\ REGISTERS + $200
	
	\ enable interrupts
	0 ## r5 mov,
	1 ## r5 r5 sub,
	r4 8 #( r5 strh,

	\ turn off DMA 2
	0 ## r1 mov,
	r3 $50 #( r1 str,	\ REGISTERS + $d0
	
	IWRAM_GLOBALS r5 LITERAL

	\ reg_sndcnt_h |= 0xF008 (fifo b)
	r3 $2 #( r1 ldrh,
	$f0 ## r6 mov,
	8 ## r6 r6 lsl,	\ $f000
	r6 r1 orr,
	8 ## r6 mov,
	r6 r1 orr,
	r3 $2 #( r1 strh,

	\ skip and load address
	r5 r6 mov,
	8 ## r6 add,
	r0 ia! r1 r2 ldm,
	r6 ia! r0 r2 stm,

	\ write DMA 2 source and destination
	r3 $48 #( r0 str,
	r3 r0 mov,
	$24 ## r0 add,
	r3 $4c #( r0 str,
	
	\ enable and start DMA 2 on fifo empty with interrupt enabled
	$f600 r0 movi
	r3 r6 mov,
	$52 ## r6 add,
	r6 0@ r0 strh,

	\ set interrupt vector
	/playsound r0 literal
	r5 INT_D2 #( r0 str,

	\ enable DMA 2 interrupt in IE
	r4 0@ r0 ldrh,
	$400 r6 movi
	r6 r0 orr,
	r4 0@ r0 strh,

	\ enable timer 1
	$80 ## r3 add,
	$80 ## r6 mov,
	16 ## r6 r6 lsl,	\ $800000
	r6 r1 orr,
	r3 4 #( r1 str,

	\ done
	r6 pop
	tos pop
	ret
end-code

\ interrupt handler for playmusic
code iwram /playmusic ( -- )
	IWRAM r5 movi

	\ load sound samples left
	r5 $604 #( r2 ldr,
	16 ## r2 r2 s! sub,
	__write_back pl? b,

	REGISTERS r4 movi
	0 ## r3 mov,

	\ stop DMA transfer to reset source address
	r4 $c4 #( r1 ldr,
	r4 $c4 #( r3 str,
	r4 $a0 #( r3 str,		\ clear fifo a

	\ loop music by resetting address and samples
	r5 $600 #( r0 ldr,
	r4 $bc #( r0 str,		\ DMA1SAD

	\ restart the DMA transfer
	r0 -4 #( r2 ldr,
	r4 $c4 #( r1 str,

	\ write address and samples back
l: __write_back
	r5 $604 #( r2 str,
	
	\ done
	ret
end-code

\ start playing looping wav sounds
code-thumb playmusic ( a -- )
	REGISTERS r3 LITERAL
	$4000200 r4 LITERAL
	
	\ enable interrupts
	0 ## r5 mov,
	1 ## r5 sub,
	r4 8 #( r5 strh,	\ $4000208

	\ stop DMA transfer of any currently playing music
	0 ## r1 mov,
	$c4 ## r3 add,		\ $40000c4
	r3 0@ r1 str,

	\ 0x3006F00 -> R5
	IWRAM r5 LITERAL
	GLOBALS r3 LITERAL
	r3 r5 r5 add,

	\ reg_sndcnt_h |= 0xB04 (fifo a)
	$4000082 r3 LITERAL
	r3 0@ r1 ldrh,
	$b00 r2 LITERAL
	r2 r1 orr,
	4 ## r2 mov,
	r2 r1 orr,
	r3 0@ r1 strh,
	
	\ skip and load address
	r0 ia! r1 r2 ldm,
	r5 0@ r0 str,
	r5 4 #( r2 str,

	\ write DMA 1 source and destination
	$40000a0 r3 LITERAL
	r3 $1c #( r0 str,	\ $40000bc
	r3 r0 mov,		\ $40000a0
	$20 ## r3 add,		\ $40000c0
	r3 0@ r0 str,

	\ enable and start DMA 1 on fifo emty
	$f600 r0 movi
	$6 ## r3 add,		\ $40000c6
	r3 0@ r0 strh,

	\ set interrupt vector
	/playmusic r0 literal
	r5 INT_D1 #( r0 str,

	\ enable DMA 1 interrupt in IE
	r4 0@ r0 ldrh,
	$200 r3 movi
	r3 r0 orr,
	r4 0@ r0 strh,

	\ enable timer 0
	$4000100 r3 LITERAL
	$80 ## r0 mov,
	16 ## r0 r0 lsl,	\ $800000
	r1 r0 orr,
	r3 0@ r0 str,

	\ done
	tos pop
	ret
end-code

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
	
	\ turn off timer 1 overflow interrupt
	v0 $36 #( w ldrh,		\ $4000106
	$c0 ## v2 mov,
	v2 w bic,
	v0 $36 #( w strh,		\ $4000106
	
	\ stop dma 2 and timer 1
	0 ## w mov,
	v0 0@ w str,
	v0 $34 #( w str,		\ $4000104
	
	\ done
	ret
end-code

\ stop all background WAV music from playing
code-thumb stopmusic ( -- )
	$40000c4 v0 LITERAL
	v0 v2 mov,
	$fe ## v2 add,			\ $40001c2 (interrupts - $3e)
	
	\ turn off timer 0 overflow interrupt
	v2 $3e #( w ldrh,		\ $4000200
	$8 ## v1 mov,
	v1 w bic,
	v2 $3e #( w strh,		\ $4000200
	
	\ stop dma 1 and reset timer 0
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

\ address of playsound handler
label (playsound-handler)

\ interrupt handler for playsound
code-thumb /playsound ( -- )
	tothumb

	\ 0x40000a0 -> R1, 0x4000100 -> R3
	$40 ## r1 mov,
	20 ## r1 r1 lsl,
	$a0 ## r1 add,		\ $40000a0
	r1 r3 mov,
	$60 ## r3 add,		\ $4000100

	\ address of sound data -> R4
	$3000608 r4 LITERAL

	\ load sound samples and process
	r4 0@ r0 ldr,
	r4 4 #( r2 ldr,
	1 ## r2 r2 sub,		\ subs, actually
	__write_back pl? b,
	0 ## r0 mov,
	
	\ stop dma 2 transfer and timer 1 if done
	r1 $30 #( r0 str,	\ $40000d0
	r3 $4 #( r0 str,	\ $4000104
	r1 $4 #( r0 str,	\ $40000a4

	\ write address and samples back
l: __write_back
	r4 ia! r0 r2 stm,
	
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

	\ turn off timer 1 overflow interrupt flag
	0 ## r1 mov,
	r3 $50 #( r1 str,
	
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

	\ write dma 2 source and destination
	r3 $48 #( r0 str,
	r3 r0 mov,
	$24 ## r0 add,
	r3 $4c #( r0 str,
	
	\ enable and start dma 2 on fifo empty
	$b6 ## r0 mov,
	8 ## r0 r0 lsl,	\ $b600
	r3 r6 mov,
	$52 ## r6 add,
	r6 0@ r0 strh,

	\ set interrupt vector
	(playsound-handler) r0 literal
	r5 INT_T1 #( r0 str,

	\ enable timer 1 interrupt
	r4 0@ r0 ldrh,
	$10 ## r6 mov,
	r6 r0 orr,
	r4 0@ r0 strh,

	\ set timer 1 to interrupt and enable
	$80 ## r3 add,
	$c0 ## r6 mov,
	16 ## r6 r6 lsl,	\ $c00000
	r6 r1 orr,
	r3 4 #( r1 str,

	\ done
	r6 pop
	tos pop
	ret
end-code

\ interrupt handler for playmusic
code-thumb iwram /playmusic ( -- )
	tothumb			\ interrupt handlers are always called as ARM
	$40 ## r4 mov,
	20 ## r4 r4 lsl,
	$80 ## r4 add,		\ $4000080

	\ address of sound data -> R5
	$3000600 r5 LITERAL

	\ load sound samples left
	r5 4 #( r2 ldr,
	1 ## r2 sub,
	20 #offset pl? b,	\ XXX: Adjust offset when changing anything!
	0 ## r3 mov,

	\ stop dma tranfer to reset source address
	r4 $44 #( r1 ldr,
	r4 $44 #( r3 str,
	r4 $20 #( r3 str, \ clear fifo a

	\ loop music by resetting address and samples
	r5 0@ r0 ldr,
	r4 $3c #( r0 str,

	\ restart the dma transfer
	4 ## r0 sub,
	r0 0@ r2 ldr,
	r4 $44 #( r1 str,

	\ bpl jumps here
	\ write address and samples back
	r5 4 #( r2 str,
	
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

	\ stop dma transfer of any currently playing music
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

	\ write dma 1 source and destination
	$40000a0 r3 LITERAL
	r3 $1c #( r0 str,	\ $40000bc
	r3 r0 mov,		\ $40000a0
	$20 ## r3 add,		\ $40000c0
	r3 0@ r0 str,

	\ enable and start dma 1 on fifo emty
	$b6 ## r0 mov,
	8 ## r0 r0 lsl,		\ $b600
	$6 ## r3 add,		\ $40000c6
	r3 0@ r0 strh,

	\ set interrupt vector
	/playmusic r0 literal
	r5 INT_T0 #( r0 str,

	\ enable timer 0 interrupt
	r4 0@ r0 ldrh,
	$8 ## r3 mov,
	r3 r0 orr,
	r4 0@ r0 strh,

	\ set timer 0 to interrupt and enable
	$4000100 r3 LITERAL
	$c0 ## r0 mov,
	16 ## r0 r0 lsl,	\ $c00000
	r1 r0 orr,
	r3 0@ r0 str,

	\ done
	tos pop
	ret
end-code

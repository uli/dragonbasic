{ -- MF/TIN SOUND functions
  -- Original code by Jeff Massung, 2003 }

\ turn all sounds on
code turnsoundon ( -- )
	REGISTERS ## v2 mov,
	
	\ $80 -> REG_SOUNDCNT_X
	$80 ## v1 mov,
	v2 $84 #( v1 strh,
	
	\ $FF77 -> REG_SOUNDCNT_L
	$ff00 ## v1 mov,
	$77 ## v1 v1 orr,
	v2 $80 #( v1 strh,
	
	\ 2 -> REG_SOUNDCNT_H
	2 ## v1 mov,
	v2 $82 #( v1 strh,
	
	\ done
	ret
end-code

\ turn all sounds off
code turnsoundoff ( -- )
	REGISTERS ## v2 mov,
	
	\ 0 -> REG_SOUNDCNT_X
	0 ## v1 mov,
	v2 $84 #( v1 strh,
	
	\ done
	ret
end-code

\ stop all background WAV sound from playing
code stopsound ( -- )
	REGISTERS ## v0 mov,
	$100 ## v0 v1 add,		\ timers
	
	\ turn off timer 1 overflow interrupt
	v1 6 #( w ldrh,
	$c0 ## w w bic,
	v1 6 #( w strh,
	
	\ stop dma 2 and timer 1
	0 ## w mov,
	v0 $d0 #( w str,
	v1 4 #( w str,
	
	\ done
	ret
end-code

\ stop all background WAV music from playing
code stopmusic ( -- )
	REGISTERS ## v0 mov,
	$100 ## v0 v1 add,		\ timers
	$200 ## v0 v2 add,		\ interrupts
	
	\ turn off timer 0 overflow interrupt
	v2 0@ w ldrh,
	$c0 ## w w bic,
	v2 0@ w strh,
	
	\ stop dma 1 and timer 0
	0 ## w mov,
	v0 $c4 #( w str,
	v1 0@ w str,
	
	\ done
	ret
end-code

\ play a single note out channel 1 or 2
code playnote ( channel length freq duty -- )
	\ load arguments
	REGISTERS ## r12 mov,
	sp ia! r8 r9 r10 ldm,
	0 ## r9 cmp,

	\ set values to write to registers (R2=L, R3=H)
	$f000 ## r3 mov,
	$700 ## r3 r3 gt? add,

	\ set the length of the tone
	64 ## r9 r9 gt? rsb,
	r9 r3 r3 gt? add,

	\ set the wave duty cycle
	r0 r3 r3 orr,

	\ reset and timed bits
	$8000 ## r8 r2 orr,
	$4000 ## r2 r2 gt? orr,

	\ compute offsets to sound registers
	1 ## r10 r10 s! sub,
	$62 ## r1 eq? mov,	\ r1 = $62 (channel 1)
	$68 ## r1 ne? mov,	\ r1 = $68 (channel 2)

	\ set default values for notes
	r12 r1 +( r3 strh,
	2 ## r1 r1 eq? add,
	4 ## r1 r1 ne? add,
	r12 r1 +( r2 strh,

	\ done
	tos pop
	ret
end-code

\ use channel 4 to produce white noise
code playdrum ( length freq step -- )
	\ setup
	REGISTERS ## v2 mov,
	$f700 ## v1 mov,

	\ load arguments
	sp ia! v3 v4 ldm,
	0 ## v4 cmp,

	\ set envelope step and length (REG_SOUND4CNT_L)
	64 ## v4 v4 gt? rsb,
	v4 v1 v1 gt? add,
	v2 $78 #( v1 strh,

	\ set loop mode (REG_SOUND4CNT_H)
	$8000 ## v1 mov,		\ reset
	$4000 ## v1 v1 gt? orr,	\ timed
	tos 4 #lsl v1 v1 orr,	\ step
	v3 v1 v1 orr,			\ frequency
	v2 $7c #( v1 strh,

	\ done
	tos pop
	ret
end-code

\ address of playsound handler
label (playsound-handler)

\ interrupt handler for playsound
code /playsound ( -- )
	sp db! r8 r9 stm,

	\ 0x4000000 -> R8, 0x4000100 -> R9	
	REGISTERS ## r8 mov,
	$100 ## r8 r9 add,

	\ address of sound data -> R11
	IWRAM ## r11 mov,
	GLOBALS ## r11 r11 add,
	8 ## r11 r11 add,

	\ load sound samples and process
	r11 ia r0 r2 ldm,
	1 ## r2 r2 s! sub,
	0 ## r0 mi? mov,
	
	\ stop dma 2 transfer and timer 1 if done
	r8 $d0 #( r0 mi? str,
	r9 4 #( r0 mi? str,
	r8 $a4 #( r0 mi? str,
	
	\ write address and samples back
	r11 ia r0 r2 stm,
	sp ia! r8 r9 ldm,
	
	\ done
	ret
end-code

\ play a wav sample 1 time on DirectSound channel 1
code playsound ( a -- )
	\ 0x4000000 -> R8, 0x4000200 -> R9
	REGISTERS ## r8 mov,
	$200 ## r8 r9 add,
	
	\ enable interrupts
	0 ## r10 mvn,
	r9 8 #( r10 strh,

	\ turn off timer 1 overflow interrupt flag
	0 ## r1 mov,
	r8 $d0 #( r1 str,
	
	\ 0x3000000 -> R10, 0x3006F00 -> R11
	IWRAM ## r10 mov,
	GLOBALS ## r10 r11 add,
	r11 r10 mov,

	\ reg_sndcnt_h |= 0xF008 (fifo b)
	r8 $82 #( r1 ldrh,
	$f000 ## r1 r1 orr,
	$8 ## r1 r1 orr,
	r8 $82 #( r1 strh,

	\ skip and load address
	8 ## r11 r11 add,
	r0 ia! r1 r2 ldm,
	r11 ia r0 r2 stm,

	\ write dma 2 source and destination
	r8 $c8 #( r0 str,
	$a4 ## r8 r0 orr,
	r8 $cc #( r0 str,
	
	\ enable and start dma 2 on fifo empty
	$b600 ## r0 mov,
	r8 $d2 #( r0 strh,

	\ set interrupt vector
	(playsound-handler) r0 literal
	r10 INT_T1 #( r0 str,

	\ enable timer 1 interrupt
	r9 0@ r0 ldrh,
	$10 ## r0 r0 orr,
	r9 0@ r0 strh,

	\ set timer 1 to interrupt and enable
	$100 ## r8 r8 orr,
	$c00000 ## r1 r0 orr,
	r8 4 #( r0 str,

	\ done
	tos pop
	ret
end-code

\ address of playmusic handler
label (playmusic-handler)

\ interrupt handler for playsound
code /playmusic ( -- )
	\ save registers
	sp db! r8 r9 stm,
	REGISTERS ## r8 mov,

	\ address of sound data -> R9
	IWRAM ## r9 mov,
	GLOBALS ## r9 r9 add,

	\ load sound samples left
	r9 4 #( r2 ldr,
	1 ## r2 r2 s! sub,
	0 ## r3 mi? mov,

	\ stop dma tranfer to reset source address
	r8 $c4 #( r1 mi? ldr,
	r8 $c4 #( r3 mi? str,
	r8 $a0 #( r3 mi? str, \ clear fifo a

	\ loop music by resetting address and samples
	r9 0@ r0 mi? ldr,
	r8 $bc #( r0 mi? str,

	\ restart the dma transfer
	r0 -4 #( r2 mi? ldr,
	r8 $c4 #( r1 mi? str,

	\ write address and samples back
	r9 4 #( r2 str,
	sp ia! r8 r9 ldm,
	
	\ done
	ret
end-code

\ start playing looping wav sounds
code playmusic ( a -- )
	\ 0x4000000 -> R8, 0x4000200 -> R9
	REGISTERS ## r8 mov,
	$200 ## r8 r9 add,
	
	\ enable interrupts
	0 ## r10 mvn,
	r9 8 #( r10 strh,

	\ stop dma transfer of any currently playing music
	0 ## r1 mov,
	r8 $c4 #( r1 str,

	\ 0x3000000 -> R10, 0x3006F00 -> R11
	IWRAM ## r10 mov,
	GLOBALS ## r10 r11 add,

	\ reg_sndcnt_h |= 0xB04 (fifo a)
	r8 $82 #( r1 ldrh,
	$b00 ## r1 r1 orr,
	$4 ## r1 r1 orr,
	r8 $82 #( r1 strh,
	
	\ skip and load address
	r0 ia! r1 r2 ldm,
	r11 ia r0 r2 stm,

	\ write dma 1 source and destination
	r8 $bc #( r0 str,
	$a0 ## r8 r0 orr,
	r8 $c0 #( r0 str,

	\ enable and start dma 1 on fifo emty
	$b600 ## r0 mov,
	r8 $c6 #( r0 strh,

	\ set interrupt vector
	(playmusic-handler) r0 literal
	r11 INT_T0 #( r0 str,

	\ enable timer 0 interrupt
	r9 0@ r0 ldrh,
	$8 ## r0 r0 orr,
	r9 0@ r0 strh,

	\ set timer 0 to interrupt and enable
	$100 ## r8 r8 orr,
	$c00000 ## r1 r0 orr,
	r8 0@ r0 str,

	\ done
	tos pop
	ret
end-code
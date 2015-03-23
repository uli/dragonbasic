/* pimp_mixer_arm.S -- ARM optimized mixer code
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */
 
.text
.section .iwram
.arm
.align 2

@ no mixing:                 361612
@ without bresenham:
@   inline-asm version:     1211037
@   current:                1150788
@ with bresenham:
@   inline-asm version:     1163062
@   current:                1105083


@irq-safe:   1121327
@irq-unsafe: 1105083

// #define IRQ_SAFE
#define USE_BRESENHAM_MIXER
// #define NO_MIXING
// #define NO_CLIPPING

.stack_store:
.word 0

#ifndef IRQ_SAFE
.ime_store:
.word 0
#endif

#ifdef USE_BRESENHAM_MIXER
.sample_data_store:
.word 0
#endif

.mixer_jumptable:
.word .mix0
.word .mix1
.word .mix2
.word .mix3
.word .mix4
.word .mix5
.word .mix6
.word .mix7

.global __pimp_mixer_mix_samples
.type __pimp_mixer_mix_samples, %function
__pimp_mixer_mix_samples:
	stmfd sp!, {r4-r12, lr} @ store all registers but parameters and stack
	
#ifdef NO_MIXING
	ldmfd sp!, {r4-r12, lr} @ restore rest of registers
	bx lr                   @ return to caller
#endif
	
#ifndef IRQ_SAFE
	str sp, .stack_store    @ store stack pointer so we can use that register in our mixer (note, interrupts must be disabled for this to be safe)
#endif

#define TARGET              r0
#define COUNTER             r1
#define SAMPLE_DATA         r2
#define VOLUME              r3
#define SAMPLE_CURSOR       lr
#define SAMPLE_CURSOR_DELTA r12
	
	@ load rest of parameters
	ldr SAMPLE_CURSOR, [sp, #40]
	ldr SAMPLE_CURSOR_DELTA, [sp, #44]
	
	@ find how many samples to fixup
	and r4, COUNTER, #7
	
	@ fixup, jump to the correct position
	adr	r5, .mixer_jumptable
	ldr	pc, [r5, r4, lsl #2]
	
	@ fixup code unrolled 7 times
.mix7:
	ldr   r6, [TARGET]
	ldrb  r5, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r6, r5, VOLUME, r6
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	str   r6, [TARGET], #4

.mix6:
	ldr   r6, [TARGET]
	ldrb  r5, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r6, r5, VOLUME, r6
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	str   r6, [TARGET], #4

.mix5:
	ldr   r6, [TARGET]
	ldrb  r5, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r6, r5, VOLUME, r6
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	str   r6, [TARGET], #4

.mix4:
	ldr   r6, [TARGET]
	ldrb  r5, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r6, r5, VOLUME, r6
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	str   r6, [TARGET], #4

.mix3:
	ldr   r6, [TARGET]
	ldrb  r5, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r6, r5, VOLUME, r6
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	str   r6, [TARGET], #4

.mix2:
	ldr   r6, [TARGET]
	ldrb  r5, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r6, r5, VOLUME, r6
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	str   r6, [TARGET], #4

.mix1:
	ldr   r6, [TARGET]
	ldrb  r5, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r6, r5, VOLUME, r6
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	str   r6, [TARGET], #4

.mix0:
	
	movs COUNTER, COUNTER, asr #3 @ divide counter by 8
	beq .ret                      @ if no more samples, return

#ifndef IRQ_SAFE
	ldr r4, =0x4000208 @ load address of REG_IME
	ldr r5, [r4]       @ load value of REG_IME
	str r5, .ime_store @ stash for later
	eor r6, r6
	str r6, [r4]       @ disable interrupt	
#endif

#ifdef USE_BRESENHAM_MIXER
	// if ((sample_cursor_delta & ~((1UL << 12) - 1)) == 0)
	@ check if bresenham mixer can be used or not 
	ldr r4, =0xfffff000         @ ~((1UL << 12) - 1))
	tst SAMPLE_CURSOR_DELTA, r4 @ any bits set?
	beq .bresenham_mixer        @ no? lets go!
#endif

#ifdef IRQ_SAFE
#define TEMP                r11
#define UNROLL_RANGE        r4-r10
#else
#define TEMP                sp
#define UNROLL_RANGE        r4-r11
#endif

.simple_loop:
	ldmia TARGET, {UNROLL_RANGE}
	
	ldrb  TEMP, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r4, TEMP, VOLUME, r4
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA

	ldrb  TEMP, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r5, TEMP, VOLUME, r5
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA

	ldrb  TEMP, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r6, TEMP, VOLUME, r6
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA

	ldrb  TEMP, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r7, TEMP, VOLUME, r7
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	
	ldrb  TEMP, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r8, TEMP, VOLUME, r8
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA

	ldrb  TEMP, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r9, TEMP, VOLUME, r9
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA

	ldrb  TEMP, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r10, TEMP, VOLUME, r10
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA

#ifndef IRQ_SAFE
	ldrb  TEMP, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r11, TEMP, VOLUME, r11
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA

	stmia TARGET!, {UNROLL_RANGE}
#else
	stmia TARGET!, {UNROLL_RANGE}
	
	@ mix a single sample
	ldr   r10, [TARGET]
	ldrb  TEMP, [SAMPLE_DATA, SAMPLE_CURSOR, lsr #12]
	mla   r10, TEMP, VOLUME, r10
	add   SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	str   r10, [TARGET], #4	
#endif
	
	subs COUNTER, COUNTER, #1
	bne .simple_loop


#ifndef IRQ_SAFE
	ldr r4, =0x4000208 @ load address of REG_IME
	ldr r5, .ime_store @ stash for later
	str r5, [r4]       @ write value to REG_IME
#endif

.ret:
	@ clean return
	mov r0, SAMPLE_CURSOR

	ldr sp, .stack_store    @ restore stack pointer
	ldmfd sp!, {r4-r12, lr} @ restore rest of registers
	bx lr                   @ return to caller

#ifdef USE_BRESENHAM_MIXER

.bresenham_mixer:
	str SAMPLE_DATA, .sample_data_store                  @ stash away SAMPLE_DATA for later use
	add SAMPLE_DATA, SAMPLE_DATA, SAMPLE_CURSOR, lsr #12 @ modify pointer so it points to the fist sample in frame
	
	mov SAMPLE_CURSOR, SAMPLE_CURSOR, asl #20
	mov SAMPLE_CURSOR_DELTA, SAMPLE_CURSOR_DELTA, asl #20

	ldrb  TEMP, [SAMPLE_DATA], #1
	mul   TEMP, VOLUME, TEMP
.bresenham_loop:
	ldmia TARGET, {UNROLL_RANGE}

	add   r4, TEMP, r4
	adds  SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	ldrcsb TEMP, [SAMPLE_DATA], #1
	mulcs  TEMP, VOLUME, TEMP

	add   r5, TEMP, r5
	adds  SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	ldrcsb TEMP, [SAMPLE_DATA], #1
	mulcs  TEMP, VOLUME, TEMP

	add   r6, TEMP, r6
	adds  SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	ldrcsb TEMP, [SAMPLE_DATA], #1
	mulcs  TEMP, VOLUME, TEMP

	add   r7, TEMP, r7
	adds  SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	ldrcsb TEMP, [SAMPLE_DATA], #1
	mulcs  TEMP, VOLUME, TEMP

	add   r8, TEMP, r8
	adds  SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	ldrcsb TEMP, [SAMPLE_DATA], #1
	mulcs  TEMP, VOLUME, TEMP

	add   r9, TEMP, r9
	adds  SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	ldrcsb TEMP, [SAMPLE_DATA], #1
	mulcs  TEMP, VOLUME, TEMP

	add   r10, TEMP, r10
	adds  SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	ldrcsb TEMP, [SAMPLE_DATA], #1
	mulcs  TEMP, VOLUME, TEMP

#ifndef IRQ_SAFE

	add   r11, TEMP, r11
	adds  SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	ldrcsb TEMP, [SAMPLE_DATA], #1
	mulcs  TEMP, VOLUME, TEMP

	stmia TARGET!, {UNROLL_RANGE}
#else
	stmia TARGET!, {UNROLL_RANGE}


	ldr   r10, [TARGET]
	add   r10, TEMP, r10
	adds  SAMPLE_CURSOR, SAMPLE_CURSOR, SAMPLE_CURSOR_DELTA
	ldrcsb TEMP, [SAMPLE_DATA], #1
	mulcs  TEMP, VOLUME, TEMP
	str   r10, [TARGET], #4		
#endif

	subs  COUNTER, COUNTER, #1
	bne .bresenham_loop

#ifndef IRQ_SAFE
	ldr r4, =0x4000208 @ load address of REG_IME
	ldr r5, .ime_store @ stash for later
	str r5, [r4]       @ write value to REG_IME
#endif
	
	ldr sp, .stack_store       @ restore stack pointer
	ldr r0, .sample_data_store @ restore the old sample data
	
	@ calculate how the sample cursor changed
	sub r0, SAMPLE_DATA, r0
	sub r0, r0, #1
	mov r0, r0, lsl #12
	add r0, SAMPLE_CURSOR, asr #20
	
	@ return to caller
	ldmfd sp!, {r4-r12, lr} @ restore rest of registers
	bx lr

#endif


//  runtime_common.s: DragonBASIC runtime environment
//
//  Copyright (C) 2015 Ulrich Hecht
//  Source code reconstructed with permission from MF/TIN executable version
//  1.4.3, Copyright (C) 2003-2004 Jeff Massung
//
//  This software is provided 'as-is', without any express or implied
//  warranty.  In no event will the authors be held liable for any damages
//  arising from the use of this software.
//
//  Permission is granted to anyone to use this software for any purpose,
//  including commercial applications, and to alter it and redistribute it
//  freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software
//     in a product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//  2. Altered source versions must be plainly marked as such, and must not be
//     misrepresented as being the original software.
//  3. This notice may not be removed or altered from any source distribution.
//
//  Ulrich Hecht
//  ulrich.hecht@gmail.com


.global _tin_entry
.global _tin_entry_p
.global _tin_iwram_table_p
.global _thumbthunk
.org 0xe0
_tin_entry:
	sub	r7, sp, #0x200

	// Set user interrupt vectors to NOP so there won't be any bad
	// surprises when enabling interrupts.
	adr	r5, _empty
	mov	r1, #0x3000000
	orr	r1, r1, #0x600
	str	r5, [r1, #0x10]
	str	r5, [r1, #0x14]
	str	r5, [r1, #0x18]
	str	r5, [r1, #0x1c]
	str	r5, [r1, #0x20]
	str	r5, [r1, #0x24]
	str	r5, [r1, #0x28]

	ldr	r5, _tin_entry_p
	ldr	r6, irq_handler_p
	mov	r1, #0x4000000
	str	r6, [r1, #-4]
	ldr	r6, waitcnt_val
	str	r6, [r1, #0x204]
	bx	r5
waitcnt_val:
	.word	0x4317
irq_handler_p:
	.word irq_handler
_tin_entry_p:
	# will be overwritten by the TIN compiler
	.word 0xffffffff
_tin_iwram_table_p:
	# will be overwritten by the TIN compiler if there is code that
	# must be copied to IWRAM
	.word 0

irq_handler:
	push	{r4-r11, lr}
	mov	r9, #0x4000000
	orr	r8, r9, #0x200
	ldrh	r10, [r8, #2]

	ldrh	r8, [r9, #-8]
	orr	r8, r8, r10
	strh	r8, [r9, #-8]

	mov	r8, #0x3000000
	orr	r8, r8, #0x600

	add	lr, pc, #4
	tst	r10, #0x0008
	ldrne	pc, [r8, #16]
	add	lr, pc, #4
	tst	r10, #0x0010
	ldrne	pc, [r8, #20]
	add	lr, pc, #4
	tst	r10, #0x1000
	ldrne	pc, [r8, #24]
	add	lr, pc, #4
	tst	r10, #0x0002
	ldrne	pc, [r8, #28]
	add	lr, pc, #4
	tst	r10, #0x0001
	ldrne	pc, [r8, #32]
	add	lr, pc, #4
	tst	r10, #0x0004
	ldrne	pc, [r8, #36]
	add	lr, pc, #4
	tst	r10, #0x0020
	ldrne	pc, [r8, #40]

	orr	r8, r9, #0x200
	strh	r10, [r8, #2]
	pop	{r4-r11, lr}
_empty:
	bx	lr
.thumb
_thumbthunk:
	bx	r1
.arm

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
.org 0xe0
_tin_entry:
	sub	r7, sp, #0x200
	ldr	r5, tin_entry_p
	ldr	r6, irq_handler_p
	mov	r1, #0x4000000
	str	r6, [r1, #-4]
	bx	r5
irq_handler_p:
	.word irq_handler
tin_entry_p:
	# will be overwritten by the TIN compiler
	.word 0xffffffff

irq_handler:
	push	{r4-r11, lr}
	mov	r9, #0x4000000
	orr	r9, r9, #0x200
	ldrh	r10, [r9, #2]
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

	strh	r10, [r9, #2]
	pop	{r4-r11, lr}
	bx	lr

.global _tin_wlit
_tin_wlit:
	ldrt	r5, [lr], #4
	bx	lr


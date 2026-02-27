/*
 * Copyright (C) 2026, The AROS Development Team
 * All rights reserved.
 *
 * Desc: Intuition Engine M68K bootstrap.
 *       Replaces the Amiga-specific rom_init.S — no custom chipset probing,
 *       no Gayle detection, no overlay disable. The IE provides a clean
 *       68020+FPU environment with known memory layout.
 *
 * Licensed under the AROS PUBLIC LICENSE (APL) Version 1.1
 */

#include <aros/m68k/asm.h>

	.text
	.chip 68020
	.align 4

	.globl __MemoryTest
	.globl rom_init
rom_init:
	/* IE does not have an AROSBootstrap path — always normal init. */

	/* ---- Set up initial supervisor stack ---- */
	lea.l	_ss_end,%sp

	/* Put the memory bank array at the bottom of the stack area */
	lea.l	_ss,%a5

	/* ---- CPU/FPU data ---- */
	/* IE always provides a full 68020 + 68881/68882 FPU.
	 * No runtime detection needed — hardcode AttnFlags.
	 *
	 * AttnFlags layout (low word of cpupcr[0]):
	 *   bit 0 = AFB_68010
	 *   bit 1 = AFB_68020
	 *   bit 2 = AFB_68030
	 *   bit 3 = AFB_68040
	 *   bit 13 = AFB_ADDR32
	 *
	 * FPU flags (high word of cpupcr[0]):
	 *   bits 16-23 = FPU ID (0x1F or less = 68881, >0x1F = 68882)
	 *
	 * We set: AFB_68010 | AFB_68020 | AFB_ADDR32 = 0x2003
	 * FPU: 68882 ID = 0x20 in high word → 0x00200000
	 * Combined: 0x00202003
	 */
	move.l	%a5,%a4		/* CPU/FPU data array pointer */
	lea	16(%a5),%a5	/* Reserve 16 bytes for CPU data + extra membank */
	move.l	#0x00202003,(%a4)	/* cpupcr[0]: AttnFlags + FPU ID */
	clr.l	4(%a4)			/* cpupcr[1]: PCR = 0 (no PCR on 68020) */
	clr.l	8(%a4)			/* reserved */
	clr.l	12(%a4)			/* reserved */

	/* ---- Memory bank list ---- */
	/* IE memory layout (skip I/O holes):
	 * Bank 1: 0x001000 - 0x09FFFF (636KB, vectors at 0x000-0xFFF)
	 * Bank 2: 0x100000 - 0x5FFFFF (5MB, VRAM region used as RAM)
	 * Sentinel: 0, 0
	 *
	 * The bank list is {start, size} pairs terminated by {0, 0}.
	 * Fastest memory first (Bank 2 = "fast", Bank 1 = "chip").
	 */

	/* Bank 2: 5MB fast memory */
	move.l	#0x00100000,(%a5)+	/* start */
	move.l	#0x00500000,(%a5)+	/* size = 5MB */

	/* Bank 1: 636KB chip memory (first 4KB reserved for vectors) */
	move.l	#0x00001000,(%a5)+	/* start */
	move.l	#0x0009F000,(%a5)+	/* size = 636KB */

	/* End-of-list sentinel */
	clr.l	(%a5)+
	clr.l	(%a5)

	/* ---- Jump to C bootstrap ---- */
	lea.l	_ss,%a5
	move.l	%a5,%sp@-	/* Push CPU/FPU data array */
	lea	16(%a5),%a5
	move.l	%a5,%sp@-	/* Push memory bank array */
	jsr	exec_boot

	/* Should not return. If it does, halt with diagnostic. */
ie_halt:
	/* Write 'H' to terminal for diagnostics */
	move.l	#0x48,0xF0700
	nop
	bra.s	ie_halt

/* ---- Early exception handler ---- */
#define STACK_OFFSET (6*4)
	.globl Early_Exception
Early_Exception:
	or.w	#0x0700,%sr
	/* Write exception PC to terminal for debugging */
	move.l	STACK_OFFSET+2(%sp),%d0
	move.l	#0x45,0xF0700		/* 'E' */
	move.l	#0x78,0xF0700		/* 'x' */
	move.l	#0x0A,0xF0700		/* newline */
	jmp	Early_TrapHandler

	.section .rodata

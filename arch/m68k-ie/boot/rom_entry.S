/*
 * Copyright (C) 2026, The AROS Development Team
 * All rights reserved.
 *
 * Desc: Intuition Engine ROM entry point.
 *       Standard M68K reset vectors: initial SSP at offset 0, initial PC at offset 4.
 *       The IE CPU fetches these on reset to begin execution.
 *
 * Licensed under the AROS PUBLIC LICENSE (APL) Version 1.1
 */

Version = 46
Revision= 1

	.text

	.globl software_reset
	.globl _start
	.align 4

rom_start:
	/* M68K reset vector format:
	 * Offset 0: Initial SSP (stack pointer)
	 * Offset 4: Initial PC (program counter)
	 * The IE loader patches these during LoadROM, but we provide
	 * sensible defaults here for standalone ROM images.
	 */
	.long	0x00020000		/* Initial SSP */
_start:
software_reset:
	jmp	rom_init

	/* ROM identification header */
	.word	0x0000
	.word	0xffff
	.word	Version
	.word	Revision
	.word	Version
	.word	Revision
	.word	0xffff
	.word	0xffff
	.asciz	"AROS ROM for Intuition Engine"
	.asciz	"Copyright \251 1995-2026 "
	.asciz	"The AROS Development Team, "
	.asciz	"All Rights Reserved."
	.asciz	ADATE

	/* Magic reset vector at offset 0xd0 (compatibility) */
	.align	4
	.org	0xd0
	.global rom_magic_reset
rom_magic_reset:
	reset
	jmp	rom_init

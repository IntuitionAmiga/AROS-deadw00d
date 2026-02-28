/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: SuperstackSwap() - set up a new supervisor stack
    Lang: english

    This routine is expected to be called from Exec/Supervisor().
    IE always uses 68020+, so we always have the stack frame ID.
*/

	#include "aros/m68k/asm.h"

/* On entry, the stack looks like:
 *   UWORD frame ID  (68010+)
 *   ULONG PC
 *   UWORD STATUS
 * A6 = SysBase
 * A5 = void SuperstackSwap(void)
 *
 * D0/D1 and A0/A1 can be used as scratch
 */
	.text
	.balign 4
	.globl SuperstackSwap
SuperstackSwap:
	move.l	%a6@(SysStkUpper),%a0
	moveq.l	#-8,%d0		/* 68020 always has frame ID: SR + PC + FMT = 8 bytes */
	lea.l	%a0@(%d0),%a1
1:
	move.w	%sp@+,%a0@(%d0)
	addq.l	#2,%d0
	bne	1b
	move.l	%a1,%sp
	rte

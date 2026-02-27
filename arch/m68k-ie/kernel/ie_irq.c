/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Intuition Engine IRQ handling.
          Unlike the Amiga (which multiplexes many sources through Paula's
          INTENA/INTREQ), the IE uses M68K autovector interrupts with each
          level mapping to exactly one source:

            Level 2 (vector 26) = Input devices (keyboard/mouse)
            Level 3 (vector 27) = Audio DMA (Paula-compatible)
            Level 4 (vector 28) = VBL (display vertical blank, 60Hz)
            Level 5 (vector 29) = System timer (5ms tick, 200Hz)

          No shared interrupt controller, no INTENA/INTREQ register.
          Each handler simply calls core_Cause() with the appropriate
          Exec IntVects slot.
    Lang: english
 */

#include <aros/kernel.h>
#include <aros/asmcall.h>

#include <exec/resident.h>
#include <exec/execbase.h>

#include <hardware/intbits.h>

#include "kernel_base.h"
#include "kernel_intr.h"
#include "kernel_syscall.h"
#include "kernel_scheduler.h"

#include "m68k_exception.h"
#include "ie_irq.h"

#include "exec_intern.h"

/*
 * DECLARE_TrapCode generates an assembly wrapper + C handler pair.
 * The wrapper saves registers, calls the C handler, and either
 * calls Exec_6_ExitIntr (if handler returns TRUE → reschedule)
 * or does a plain RTE (if FALSE → no reschedule, e.g. NMI).
 *
 * Same macro as m68k-amiga; the M68K exception model is identical.
 */
#define DECLARE_TrapCode(handler) \
    BOOL handler(VOID); \
    VOID handler##_TrapCode(ULONG Id); \
    asm ( \
            "   .global " #handler "_TrapCode\n" \
            "   .func " #handler "_TrapCode\n" \
            #handler "_TrapCode:\n" \
            "   addq.l  #4,%sp\n" \
            "   movem.l %d0/%d1/%a0/%a1/%a5/%a6,%sp@-\n" \
            "   jsr     " #handler "\n" \
            "   tst.w   %d0\n" \
            "   beq     0f\n" \
            "   jmp     Exec_6_ExitIntr\n" \
            "0:\n" \
            "   movem.l %sp@+,%d0/%d1/%a0/%a1/%a5/%a6\n" \
            "   rte\n" \
            "   .endfunc\n" \
        ); \

DECLARE_TrapCode(IE_Level_2);
DECLARE_TrapCode(IE_Level_3);
DECLARE_TrapCode(IE_Level_4);
DECLARE_TrapCode(IE_Level_5);
DECLARE_TrapCode(IE_Level_7);

/*
 * Level 2: Input devices (keyboard, mouse).
 * Maps to ExecBase IntVects[INTB_PORTS] — same as Amiga Level 2.
 */
BOOL IE_Level_2(VOID)
{
    const UWORD mask = (1 << INTB_PORTS);

    core_Cause(INTB_PORTS, mask);

    return TRUE;
}

/*
 * Level 3: Audio DMA (Paula-compatible).
 * Reads IE_AUD_STATUS to determine which channels completed,
 * clears the handled bits, and calls core_Cause for each.
 * Maps to ExecBase IntVects[INTB_AUD0..INTB_AUD3].
 */
BOOL IE_Level_3(VOID)
{
    volatile unsigned long *status_reg = (volatile unsigned long *)0xF22A4;
    ULONG status = *status_reg;

    if (status & 0x0F) {
        /* Write-to-clear the handled bits */
        *status_reg = status & 0x0F;

        if (status & (1 << 0))
            core_Cause(INTB_AUD0, 1 << INTB_AUD0);
        if (status & (1 << 1))
            core_Cause(INTB_AUD1, 1 << INTB_AUD1);
        if (status & (1 << 2))
            core_Cause(INTB_AUD2, 1 << INTB_AUD2);
        if (status & (1 << 3))
            core_Cause(INTB_AUD3, 1 << INTB_AUD3);
    }

    return TRUE;
}

/*
 * Level 4: Vertical blank (60Hz).
 * Maps to ExecBase IntVects[INTB_VERTB] — same as Amiga VERTB.
 * timer.device registers its VBlank handler here.
 */
BOOL IE_Level_4(VOID)
{
    const UWORD mask = (1 << INTB_VERTB);

    core_Cause(INTB_VERTB, mask);

    return TRUE;
}

/*
 * Level 5: System timer (200Hz, 5ms tick).
 * Maps to ExecBase IntVects[INTB_EXTER] — timer.device registers
 * its microhz handler here (replaces CIA interrupt on Amiga).
 */
BOOL IE_Level_5(VOID)
{
    const UWORD mask = (1 << INTB_EXTER);

    core_Cause(INTB_EXTER, mask);

    return TRUE;
}

/*
 * Level 7: NMI — non-maskable, no reschedule.
 */
BOOL IE_Level_7(VOID)
{
    return FALSE;
}

/*
 * IE exception table — only the levels we use.
 * Other levels (1, 3, 6) are left unhandled.
 */
const struct M68KException IEExceptionTable[] = {
    { .Id = 26, .Handler = IE_Level_2_TrapCode },
    { .Id = 27, .Handler = IE_Level_3_TrapCode },
    { .Id = 28, .Handler = IE_Level_4_TrapCode },
    { .Id = 29, .Handler = IE_Level_5_TrapCode },
    { .Id = 31, .Handler = IE_Level_7_TrapCode },
    { .Id =  0, }
};

void IEIRQInit(struct ExecBase *SysBase)
{
    /* Install interrupt vector handlers */
    M68KExceptionInit(IEExceptionTable, SysBase);

    /* Enable Vertical Blank and SoftInt in exec.
     * SoftInt delivery on IE relies on Enable() checking SFF_SoftInt
     * (CUSTOM_CAUSE is a no-op since AROS_ARCH_amiga is not defined).
     */
    /* Nothing else to do — no hardware interrupt controller to program.
     * The IE loader asserts interrupts via cpu.AssertInterrupt() from
     * the Go host side; the CPU's autovector mechanism delivers them
     * to the installed vector handlers.
     */
}

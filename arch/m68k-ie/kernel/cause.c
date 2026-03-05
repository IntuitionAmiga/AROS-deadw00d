/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: KrnCause for IE — dispatch pending software interrupts.
          Unlike the Amiga (which triggers a Level 1 hardware interrupt
          via INTREQ), the IE has no hardware interrupt controller.
          Enable() calls KrnCause() when SFF_SoftInt is set; we dispatch
          the SoftIntDispatch handler chain directly via core_Cause().
*/

#include <aros/kernel.h>
#include <exec/execbase.h>
#include <hardware/intbits.h>

#include <kernel_base.h>
#include <kernel_intr.h>
#include <kernel_syscall.h>

#include <proto/kernel.h>

/* See rom/kernel/cause.c for documentation */

AROS_LH0I(void, KrnCause,
    struct KernelBase *, KernelBase, 3, Kernel)
{
    AROS_LIBFUNC_INIT

    /*
     * Dispatch pending software interrupts immediately.
     *
     * On Amiga hardware, CUSTOM_CAUSE(INTF_SOFTINT) writes to the Paula
     * INTREQ register, triggering a Level 1 interrupt whose handler calls
     * SoftIntDispatch. Since the IE has no Paula, we call core_Cause()
     * directly — this invokes ExecBase->IntVects[INTB_SOFTINT] which
     * is SoftIntDispatch.
     *
     * Reentrancy is safe: SoftIntDispatch calls KrnCli() before RemHead()
     * and KrnSti() before executing each handler. A hardware interrupt
     * during handler execution may call core_ExitIntr() which also checks
     * SFF_SoftInt, but SoftIntDispatch clears the flag before processing
     * and the break+restart loop handles new entries correctly.
     *
     * Called from Enable() when SFF_SoftInt is set and interrupts are
     * being re-enabled (IDNESTCOUNT < 0, not in supervisor mode).
     */
    core_Cause(INTB_SOFTINT, 1L << INTB_SOFTINT);

    AROS_LIBFUNC_EXIT
}

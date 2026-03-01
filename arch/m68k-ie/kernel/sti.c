/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: KrnSti — enable interrupts on IE (M68K SR-based).
          Uses TRAP #15 to modify the IPL bits in the status register
          from either user or supervisor mode.  The trap handler
          (IE_Trap_Sti, installed by start.c) clears the IPL bits in
          the saved SR on the exception frame before returning via RTE.
*/

#include <aros/kernel.h>
#include <aros/libcall.h>

#include <kernel_base.h>

#include <proto/kernel.h>

/* See rom/kernel/sti.c for documentation */

AROS_LH0I(void, KrnSti,
    struct KernelBase *, KernelBase, 10, Kernel)
{
    AROS_LIBFUNC_INIT

    /* TRAP #15 → vector 47 → IE_Trap_Sti handler clears IPL */
    asm volatile ("trap #15\n");

    AROS_LIBFUNC_EXIT
}

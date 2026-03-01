/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: KrnCli — disable interrupts on IE (M68K SR-based).
          Uses TRAP #14 to modify the IPL bits in the status register
          from either user or supervisor mode.  The trap handler
          (IE_Trap_Cli, installed by start.c) sets IPL=7 in the saved
          SR on the exception frame before returning via RTE.
*/

#include <aros/kernel.h>
#include <aros/libcall.h>

#include <kernel_base.h>

#include <proto/kernel.h>

/* See rom/kernel/cli.c for documentation */

AROS_LH0I(void, KrnCli,
    struct KernelBase *, KernelBase, 9, Kernel)
{
    AROS_LIBFUNC_INIT

    /* TRAP #14 → vector 46 → IE_Trap_Cli handler sets IPL=7 */
    asm volatile ("trap #14\n");

    AROS_LIBFUNC_EXIT
}

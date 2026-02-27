/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: KrnCli — disable interrupts on IE (M68K SR-based).
          The Amiga version writes to Paula INTENA; the IE has no
          interrupt controller, so we mask via the M68K Status Register.
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

    /* Set IPL to 7 (mask all interrupts) via OR into SR */
    asm volatile ("ori.w #0x0700,%sr\n");

    AROS_LIBFUNC_EXIT
}

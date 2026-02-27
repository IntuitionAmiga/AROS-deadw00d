/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: KrnSti — enable interrupts on IE (M68K SR-based).
          The Amiga version writes to Paula INTENA; the IE has no
          interrupt controller, so we unmask via the M68K Status Register.
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

    /* Clear IPL bits (allow all interrupts) via AND into SR */
    asm volatile ("andi.w #0xf8ff,%sr\n");

    AROS_LIBFUNC_EXIT
}

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: KrnCause stub for IE.
          SoftInt delivery on the IE uses Enable()'s SFF_SoftInt check,
          not a hardware interrupt trigger. This stub is not called.
*/

#include <aros/kernel.h>

#include <kernel_base.h>
#include <kernel_syscall.h>

#include <proto/kernel.h>

/* See rom/kernel/cause.c for documentation */

AROS_LH0I(void, KrnCause,
    struct KernelBase *, KernelBase, 3, Kernel)
{
    AROS_LIBFUNC_INIT

    /* Stub — exec/Cause() does not call KrnCause() on the IE platform.
     * SoftInt delivery relies on Enable() checking SFF_SoftInt.
     */

    AROS_LIBFUNC_EXIT
}

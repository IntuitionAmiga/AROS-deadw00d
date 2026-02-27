/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE kernel debug output via TERM_OUT MMIO register.
          The Amiga version writes to Paula SERDAT; the IE writes
          to the TERM_OUT register at 0xF0700 which the host displays
          on the terminal.
*/

#include <aros/kernel.h>

#include <kernel_base.h>
#include <kernel_debug.h>

#include "ie_hwreg.h"

/*
 * KernelBase is an optional parameter here. During
 * very early startup it can be NULL.
 */
int krnPutC(int chr, struct KernelBase *KernelBase)
{
    if (chr == '\n')
        krnPutC('\r', KernelBase);

    /* Output a char to the IE terminal MMIO register */
    ie_write32(IE_TERM_OUT, (unsigned long)chr);

    return 1;
}

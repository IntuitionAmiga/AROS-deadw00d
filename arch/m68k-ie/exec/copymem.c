/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE-accelerated CopyMem — offloads large copies to IE64 coprocessor
*/

#include <aros/libcall.h>
#include <proto/exec.h>

#include <libraries/iewarp.h>
#include <ie_hwreg.h>

AROS_LH3(void, CopyMem,
    AROS_LHA(CONST_APTR, source, A0),
    AROS_LHA(APTR,       dest,   A1),
    AROS_LHA(ULONG,      size,   D0),
    struct ExecBase *, SysBase, 104, Exec)
{
    AROS_LIBFUNC_INIT

    UBYTE *s = (UBYTE *)source;
    UBYTE *d = (UBYTE *)dest;

    /* Check if iewarp.library's IE64 worker is available via MMIO */
    if (size >= 1024)
    {
        /* Dispatch to IE64 coprocessor */
        ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
        ie_write32(IE_COPROC_OP, WARP_OP_MEMMOVE);
        ie_write32(IE_COPROC_REQ_PTR, (ULONG)source);
        ie_write32(IE_COPROC_REQ_LEN, size);
        ie_write32(IE_COPROC_RESP_PTR, (ULONG)dest);
        ie_write32(IE_COPROC_RESP_CAP, size);
        ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

        if (ie_read32(IE_COPROC_CMD_STATUS) == 0)
        {
            ULONG ticket = ie_read32(IE_COPROC_TICKET);
            /* Busy-wait for completion (no iewarp.library dependency) */
            ie_write32(IE_COPROC_TICKET, ticket);
            ie_write32(IE_COPROC_TIMEOUT, 5000);
            ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_WAIT_CMD);
            return;
        }
    }

    /* M68K fallback — overlap-safe copy */
    if (d < s || d >= s + size)
    {
        /* Non-overlapping or forward-safe: copy forward */
        if (size >= 4 && !((IPTR)s & 3) && !((IPTR)d & 3))
        {
            ULONG *ls = (ULONG *)s;
            ULONG *ld = (ULONG *)d;
            ULONG longs = size >> 2;
            ULONG i;

            for (i = 0; i < longs; i++)
                ld[i] = ls[i];

            s += longs << 2;
            d += longs << 2;
            size &= 3;
        }

        while (size--)
            *d++ = *s++;
    }
    else
    {
        /* Overlapping with dest > src: copy backward */
        s += size;
        d += size;

        while (size--)
            *--d = *--s;
    }

    AROS_LIBFUNC_EXIT
}

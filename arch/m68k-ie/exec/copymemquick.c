/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE-accelerated CopyMemQuick — ULONG-aligned fast copy via IE64
*/

#include <aros/libcall.h>
#include <proto/exec.h>

#include <libraries/iewarp.h>
#include <ie_hwreg.h>

AROS_LH3(void, CopyMemQuick,
    AROS_LHA(CONST_APTR, source, A0),
    AROS_LHA(APTR,       dest,   A1),
    AROS_LHA(ULONG,      size,   D0),
    struct ExecBase *, SysBase, 105, Exec)
{
    AROS_LIBFUNC_INIT

    /* Check if IE64 coprocessor can handle this */
    if (size >= 1024)
    {
        ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
        ie_write32(IE_COPROC_OP, WARP_OP_MEMCPY_QUICK);
        ie_write32(IE_COPROC_REQ_PTR, (ULONG)source);
        ie_write32(IE_COPROC_REQ_LEN, size);
        ie_write32(IE_COPROC_RESP_PTR, (ULONG)dest);
        ie_write32(IE_COPROC_RESP_CAP, size);
        ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

        if (ie_read32(IE_COPROC_CMD_STATUS) == 0)
        {
            ULONG ticket = ie_read32(IE_COPROC_TICKET);
            ie_write32(IE_COPROC_TICKET, ticket);
            ie_write32(IE_COPROC_TIMEOUT, 5000);
            ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_WAIT_CMD);
            return;
        }
    }

    /* M68K fallback — ULONG-aligned copy */
    {
        ULONG *ls = (ULONG *)source;
        ULONG *ld = (ULONG *)dest;
        ULONG longs = size >> 2;
        ULONG i;

        for (i = 0; i < longs; i++)
            ld[i] = ls[i];
    }

    AROS_LIBFUNC_EXIT
}

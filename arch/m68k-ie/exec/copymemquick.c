/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE-accelerated CopyMemQuick — ULONG-aligned fast copy via IE64
*/

#include <aros/libcall.h>
#include <proto/exec.h>

#include <libraries/iewarp.h>
#include <ie_hwreg.h>

static struct Library *IEWarpBase = NULL;
static BOOL iewarp_tried = FALSE;
#include <iewarp_consumer.h>

static inline BOOL iewarp_open_safe(void)
{
    if (IEWarpBase) return TRUE;
    if (iewarp_tried) return FALSE;
    iewarp_tried = TRUE;
    IEWarpBase = OpenLibrary("iewarp.library", 0);
    return IEWarpBase != NULL;
}

AROS_LH3(void, CopyMemQuick,
    AROS_LHA(CONST_APTR, source, A0),
    AROS_LHA(APTR,       dest,   A1),
    AROS_LHA(ULONG,      size,   D0),
    struct ExecBase *, SysBase, 105, Exec)
{
    AROS_LIBFUNC_INIT

    /* Dispatch to IE64 coprocessor via iewarp.library */
    if (size >= 1024 && iewarp_open_safe())
    {
        IEWarpSetCaller(IEWARP_CALLER_EXEC);
        {
            ULONG ticket = IEWarpMemCpyQuick((APTR)dest, (APTR)source, size);
            if (ticket)
            {
                IEWarpWait(ticket);
                return;
            }
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

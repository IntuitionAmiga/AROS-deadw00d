/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE-accelerated CopyMem — offloads large copies to IE64 coprocessor
*/

#include <aros/libcall.h>
#include <proto/exec.h>

#include <libraries/iewarp.h>
#include <ie_hwreg.h>

static struct Library *IEWarpBase = NULL;
static BOOL iewarp_tried = FALSE;
#include <iewarp_consumer.h>

/*
 * Re-entrant safe open: CopyMem is called by OpenLibrary itself,
 * so IEWARP_OPEN() would cause infinite recursion. This guard
 * prevents re-entry — the nested CopyMem from OpenLibrary just
 * uses the M68K fallback path.
 */
static inline BOOL iewarp_open_safe(void)
{
    if (IEWarpBase) return TRUE;
    if (iewarp_tried) return FALSE;
    iewarp_tried = TRUE;
    IEWarpBase = OpenLibrary("iewarp.library", 0);
    return IEWarpBase != NULL;
}

AROS_LH3(void, CopyMem,
    AROS_LHA(CONST_APTR, source, A0),
    AROS_LHA(APTR,       dest,   A1),
    AROS_LHA(ULONG,      size,   D0),
    struct ExecBase *, SysBase, 104, Exec)
{
    AROS_LIBFUNC_INIT

    UBYTE *s = (UBYTE *)source;
    UBYTE *d = (UBYTE *)dest;

    /* Dispatch to IE64 coprocessor via iewarp.library */
    if (size >= 1024 && iewarp_open_safe())
    {
        IEWarpSetCaller(IEWARP_CALLER_EXEC);
        {
            ULONG ticket = IEWarpMemMove((APTR)dest, (APTR)source, size);
            if (ticket)
            {
                IEWarpWait(ticket);
                return;
            }
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

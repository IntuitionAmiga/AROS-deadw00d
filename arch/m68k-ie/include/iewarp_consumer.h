/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: iewarp.library consumer helper for IE arch modules.
          Provides lazy library opening and LVO call stubs for
          functions not yet in the auto-generated defines/iewarp.h.

    Usage:
        static struct Library *IEWarpBase = NULL;
        #include <iewarp_consumer.h>

        void MyDispatch(...)
        {
            if (IEWARP_OPEN())
            {
                IEWarpSetCaller(IEWARP_CALLER_EXEC);
                ULONG t = IEWarpMemMove(dst, src, len);
                if (t) IEWarpWait(t);
            }
        }
*/

#ifndef IEWARP_CONSUMER_H
#define IEWARP_CONSUMER_H

#include <proto/exec.h>

/*
 * Set up the library base accessor before including proto/iewarp.h.
 * This makes all auto-generated inline stubs use the file-scope
 * IEWarpBase variable declared by the consumer.
 */
#ifndef __aros_getbase_IEWarpBase
#define __aros_getbase_IEWarpBase() (IEWarpBase)
#endif
#define __IEWARP_NOLIBBASE__

#include <proto/iewarp.h>

/* Lazy-open: evaluates to TRUE if iewarp.library is available */
#define IEWARP_OPEN() \
    (IEWarpBase || (IEWarpBase = OpenLibrary("iewarp.library", 0)))

/*
 * LVO call stubs for selected functions added after the last build
 * system regeneration of defines/iewarp.h.  Specifically: SetCaller
 * (slot 30), Scroll/GlyphRender/AreaFill (slots 39-41), and
 * WorkerStop/WorkerStart (slots 42-43).
 *
 * These will be superseded by the auto-generated stubs once the
 * AROS build is re-run with the updated iewarp.conf.
 */

/* IEWarpSetCaller — slot 30 (LVO -180) */
#ifndef IEWarpSetCaller
#define IEWarpSetCaller(callerID) \
    ((void)AROS_LC1(ULONG, IEWarpSetCaller, \
        AROS_LCA(ULONG, (callerID), D0), \
        struct Library *, IEWarpBase, 30, IEWarp))
#endif

/* IEWarpScroll — slot 39 (LVO -234) */
#ifndef IEWarpScroll
#define IEWarpScroll(fb, width, height, xMin, yMin, dx, dy, stride) \
    AROS_LC8(ULONG, IEWarpScroll, \
        AROS_LCA(APTR, (fb), A0), \
        AROS_LCA(UWORD, (width), D0), \
        AROS_LCA(UWORD, (height), D1), \
        AROS_LCA(UWORD, (xMin), D2), \
        AROS_LCA(UWORD, (yMin), D3), \
        AROS_LCA(WORD, (dx), D4), \
        AROS_LCA(WORD, (dy), D5), \
        AROS_LCA(UWORD, (stride), D6), \
        struct Library *, IEWarpBase, 39, IEWarp)
#endif

/* IEWarpGlyphRender — slot 40 (LVO -240) */
#ifndef IEWarpGlyphRender
#define IEWarpGlyphRender(descs, count, dst, dstStride, alphaData) \
    AROS_LC5(ULONG, IEWarpGlyphRender, \
        AROS_LCA(APTR, (descs), A0), \
        AROS_LCA(ULONG, (count), D0), \
        AROS_LCA(APTR, (dst), A1), \
        AROS_LCA(ULONG, (dstStride), D1), \
        AROS_LCA(APTR, (alphaData), A2), \
        struct Library *, IEWarpBase, 40, IEWarp)
#endif

/* IEWarpAreaFill — slot 41 (LVO -246) */
#ifndef IEWarpAreaFill
#define IEWarpAreaFill(vertices, count, dstBase, stride) \
    AROS_LC4(ULONG, IEWarpAreaFill, \
        AROS_LCA(APTR, (vertices), A0), \
        AROS_LCA(ULONG, (count), D0), \
        AROS_LCA(APTR, (dstBase), A1), \
        AROS_LCA(ULONG, (stride), D1), \
        struct Library *, IEWarpBase, 41, IEWarp)
#endif

/* IEWarpWorkerStop — slot 42 (LVO -252) */
#ifndef IEWarpWorkerStop
#define IEWarpWorkerStop() \
    ((void)AROS_LC0(ULONG, IEWarpWorkerStop, \
        struct Library *, IEWarpBase, 42, IEWarp))
#endif

/* IEWarpWorkerStart — slot 43 (LVO -258) */
#ifndef IEWarpWorkerStart
#define IEWarpWorkerStart() \
    ((void)AROS_LC0(ULONG, IEWarpWorkerStart, \
        struct Library *, IEWarpBase, 43, IEWarp))
#endif

#endif /* IEWARP_CONSUMER_H */

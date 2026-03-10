/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE-accelerated AreaEnd — polygon fill via IE64 coprocessor
*/

#include <aros/libcall.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <graphics/rastport.h>
#include <graphics/gfx.h>

#ifdef __mc68000__
#include <libraries/iewarp.h>
#include <ie_hwreg.h>

static struct Library *IEWarpBase = NULL;
#include <iewarp_consumer.h>
#endif

#define IE_AREA_THRESHOLD 4096  /* minimum bounding-box area for IE64 dispatch */

AROS_LH1(LONG, AreaEnd,
    AROS_LHA(struct RastPort *, rp, A1),
    struct GfxBase *, GfxBase,
    83, Graphics)
{
    AROS_LIBFUNC_INIT

    struct AreaInfo *ai = rp->AreaInfo;
    WORD *vtx;
    WORD count;
    WORD minX, maxX, minY, maxY;
    LONG area;
    WORD i;

    if (!ai || ai->Count < 3)
        return -1;

    vtx   = ai->VctrTbl;
    count = ai->Count;

    /* Compute bounding box */
    minX = maxX = vtx[0];
    minY = maxY = vtx[1];
    for (i = 1; i < count; i++)
    {
        WORD x = vtx[i * 2];
        WORD y = vtx[i * 2 + 1];
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }

    area = (LONG)(maxX - minX + 1) * (LONG)(maxY - minY + 1);

    /* Dispatch WARP_OP_AREA_FILL via iewarp.library */
#ifdef __mc68000__
    if (area >= IE_AREA_THRESHOLD && rp->BitMap && IEWARP_OPEN())
    {
        APTR base = rp->BitMap->Planes[0];

        if (base)
        {
            IEWarpSetCaller(IEWARP_CALLER_GRAPHICS);
            {
                ULONG ticket = IEWarpAreaFill(
                    (APTR)vtx, (ULONG)count,
                    base, (ULONG)rp->BitMap->BytesPerRow);
                if (ticket)
                {
                    IEWarpWait(ticket);
                    ai->Count   = 0;
                    ai->VctrPtr = ai->VctrTbl;
                    ai->FlagPtr = ai->FlagTbl;
                    return 0;
                }
            }
        }
    }
#endif

    /*
     * M68K fallback: scanline polygon fill using RectFill.
     */
    {
        WORD y;

        for (y = minY; y <= maxY; y++)
        {
            WORD intersections[64];
            WORD nints = 0;
            WORD j;

            for (j = 0; j < count && nints < 62; j++)
            {
                WORD j2 = (j + 1) % count;
                WORD y1 = vtx[j  * 2 + 1];
                WORD y2 = vtx[j2 * 2 + 1];
                WORD x1 = vtx[j  * 2];
                WORD x2 = vtx[j2 * 2];

                if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y))
                {
                    /* Edge crosses this scanline — compute X intersection */
                    WORD ix = x1 + (LONG)(y - y1) * (x2 - x1) / (y2 - y1);
                    intersections[nints++] = ix;
                }
            }

            /* Sort intersections (insertion sort — typically very few) */
            for (j = 1; j < nints; j++)
            {
                WORD key = intersections[j];
                WORD k = j - 1;
                while (k >= 0 && intersections[k] > key)
                {
                    intersections[k + 1] = intersections[k];
                    k--;
                }
                intersections[k + 1] = key;
            }

            /* Fill spans in pairs (even-odd rule) */
            for (j = 0; j + 1 < nints; j += 2)
            {
                if (intersections[j] <= intersections[j + 1])
                    RectFill(rp, intersections[j], y, intersections[j + 1], y);
            }
        }
    }

    /* Reset AreaInfo for next polygon */
    ai->Count   = 0;
    ai->VctrPtr = ai->VctrTbl;
    ai->FlagPtr = ai->FlagTbl;

    return 0;

    AROS_LIBFUNC_EXIT
}

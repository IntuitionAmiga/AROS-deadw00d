/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE-accelerated ScrollRasterBF — batched scroll+fill via IE64
*/

#include <aros/libcall.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <graphics/rastport.h>
#include <graphics/clip.h>
#include <graphics/gfx.h>

#ifdef __mc68000__
#include <libraries/iewarp.h>
#include <ie_hwreg.h>

static struct Library *IEWarpBase = NULL;
#include <iewarp_consumer.h>
#endif

#define IE_SCROLL_THRESHOLD 4096  /* minimum area (pixels) for IE64 dispatch */

AROS_LH7(void, ScrollRasterBF,
    AROS_LHA(struct RastPort *, rp,     A1),
    AROS_LHA(LONG,              dx,     D0),
    AROS_LHA(LONG,              dy,     D1),
    AROS_LHA(LONG,              xMin,   D2),
    AROS_LHA(LONG,              yMin,   D3),
    AROS_LHA(LONG,              xMax,   D4),
    AROS_LHA(LONG,              yMax,   D5),
    struct GfxBase *, GfxBase,
    182, Graphics)
{
    AROS_LIBFUNC_INIT

    LONG width  = xMax - xMin + 1;
    LONG height = yMax - yMin + 1;

    if (width <= 0 || height <= 0 || (dx == 0 && dy == 0))
        return;

    /* Dispatch batched blit+fill to IE64 via iewarp.library */
#ifdef __mc68000__
    if ((LONG)(width * height) >= IE_SCROLL_THRESHOLD &&
        rp->BitMap && IEWARP_OPEN())
    {
        ULONG stride = rp->BitMap->BytesPerRow;
        APTR  base   = rp->BitMap->Planes[0];

        if (base && stride)
        {
            IEWarpSetCaller(IEWARP_CALLER_GRAPHICS);
            {
                ULONG ticket = IEWarpScroll(base,
                    (UWORD)width, (UWORD)height,
                    (UWORD)xMin, (UWORD)yMin,
                    (WORD)dx, (WORD)dy, (UWORD)stride);
                if (ticket)
                {
                    IEWarpWait(ticket);
                    return;
                }
            }
        }
    }
#endif

    /* Fallback: delegate to default graphics.library ScrollRasterBF */
    ClipBlit(rp, xMin, yMin, rp, xMin + dx, yMin + dy,
             width - (dx < 0 ? -dx : dx),
             height - (dy < 0 ? -dy : dy),
             0xC0);

    /* Fill exposed region with background color */
    if (dx > 0)
        RectFill(rp, xMin, yMin, xMin + dx - 1, yMax);
    else if (dx < 0)
        RectFill(rp, xMax + dx + 1, yMin, xMax, yMax);

    if (dy > 0)
        RectFill(rp, xMin, yMin, xMax, yMin + dy - 1);
    else if (dy < 0)
        RectFill(rp, xMin, yMax + dy + 1, xMax, yMax);

    AROS_LIBFUNC_EXIT
}

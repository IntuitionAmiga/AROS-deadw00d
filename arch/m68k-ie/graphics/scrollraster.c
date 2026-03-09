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

#include <libraries/iewarp.h>
#include <ie_hwreg.h>

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

    /*
     * For large scroll regions, dispatch batched blit+fill to IE64 via
     * WARP_OP_SCROLL. The worker handles the copy of the scrolled region
     * and the fill of the exposed area in one operation.
     *
     * Parameters: REQ_PTR = bitmap base (from RastPort),
     *   REQ_LEN = width | (height << 16),
     *   RESP_PTR = xMin | (yMin << 16),
     *   RESP_CAP = dx | (dy << 16),
     *   TIMEOUT = stride (bytes per row).
     *
     * Falls back to the default graphics.library implementation if the
     * area is small or the coprocessor is unavailable.
     */
    if ((LONG)(width * height) >= IE_SCROLL_THRESHOLD && rp->BitMap)
    {
        ULONG stride = rp->BitMap->BytesPerRow;
        APTR  base   = rp->BitMap->Planes[0];

        if (base && stride)
        {
            ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
            ie_write32(IE_COPROC_OP, WARP_OP_SCROLL);
            ie_write32(IE_COPROC_REQ_PTR, (ULONG)base);
            ie_write32(IE_COPROC_REQ_LEN, (ULONG)width | ((ULONG)height << 16));
            ie_write32(IE_COPROC_RESP_PTR,
                       (ULONG)(UWORD)xMin | ((ULONG)(UWORD)yMin << 16));
            ie_write32(IE_COPROC_RESP_CAP,
                       (ULONG)(WORD)dx | ((ULONG)(WORD)dy << 16));
            ie_write32(IE_COPROC_TIMEOUT, stride);
            ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

            if (ie_read32(IE_COPROC_CMD_STATUS) == 0)
            {
                ULONG ticket = ie_read32(IE_COPROC_TICKET);
                ie_write32(IE_COPROC_TICKET, ticket);
                ie_write32(IE_COPROC_TIMEOUT, 5000);
                ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_WAIT_CMD);
                if (ie_read32(IE_COPROC_TICKET_STATUS) == IE_COPROC_ST_OK)
                    return;  /* IE64 handled it */
            }
        }
    }

    /* Fallback: delegate to default graphics.library ScrollRasterBF.
     * This goes through BltBitMap + EraseRect which are already
     * accelerated via the IEGfx HIDD. */
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

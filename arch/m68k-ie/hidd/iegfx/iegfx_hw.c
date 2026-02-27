/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Intuition Engine Graphics hardware access layer.
          Much simpler than SAGA — no PLL, no sprite, no palette modes.
          IE VideoChip: set mode index + enable bit, framebuffer at VRAM.
*/

#define DEBUG 0
#include <aros/debug.h>

#include <exec/types.h>
#include "iegfx_hw.h"

BOOL IE_VideoInit(void)
{
    /*
     * Check if the IE VideoChip is present by reading the status register.
     * On a real IE, the status register is always readable.
     * Return TRUE — we know the hardware is there (we ARE the hardware).
     */
    D(bug("[IEGfx] IE_VideoInit()\n"));
    return TRUE;
}

void IE_VideoEnable(BOOL enable)
{
    D(bug("[IEGfx] IE_VideoEnable(%d)\n", enable));
    if (enable)
        ie_write32(IE_VIDEO_CTRL, IE_VIDEO_CTRL_ENABLE);
    else
        ie_write32(IE_VIDEO_CTRL, 0);
}

void IE_VideoSetMode(UWORD mode)
{
    D(bug("[IEGfx] IE_VideoSetMode(%d)\n", mode));
    ie_write32(IE_VIDEO_MODE, (ULONG)mode);
}

void IE_BlitCopy(ULONG src, ULONG dst, UWORD width, UWORD height,
                 UWORD src_stride, UWORD dst_stride)
{
    D(bug("[IEGfx] BlitCopy(src=%08lx, dst=%08lx, %dx%d)\n",
          src, dst, width, height));

    ie_write32(IE_BLT_OP, IE_BLT_OP_COPY);
    ie_write32(IE_BLT_SRC, src);
    ie_write32(IE_BLT_DST, dst);
    ie_write32(IE_BLT_WIDTH, (ULONG)width);
    ie_write32(IE_BLT_HEIGHT, (ULONG)height);
    ie_write32(IE_BLT_SRC_STRIDE, (ULONG)src_stride);
    ie_write32(IE_BLT_DST_STRIDE, (ULONG)dst_stride);
    ie_write32(IE_BLT_CTRL, IE_BLT_CTRL_START);

    /* IE blitter is synchronous — completes immediately after start */
}

void IE_BlitFill(ULONG dst, UWORD width, UWORD height,
                 UWORD dst_stride, ULONG color)
{
    D(bug("[IEGfx] BlitFill(dst=%08lx, %dx%d, color=%08lx)\n",
          dst, width, height, color));

    ie_write32(IE_BLT_OP, IE_BLT_OP_FILL);
    ie_write32(IE_BLT_DST, dst);
    ie_write32(IE_BLT_WIDTH, (ULONG)width);
    ie_write32(IE_BLT_HEIGHT, (ULONG)height);
    ie_write32(IE_BLT_DST_STRIDE, (ULONG)dst_stride);
    ie_write32(IE_BLT_COLOR, color);
    ie_write32(IE_BLT_CTRL, IE_BLT_CTRL_START);
}

void IE_BlitWait(void)
{
    /* IE blitter is synchronous — no wait needed */
    /* Left as a placeholder for future async blitter support */
}

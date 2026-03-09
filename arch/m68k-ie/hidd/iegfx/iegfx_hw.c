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

void IE_SetColorMode(UWORD mode)
{
    D(bug("[IEGfx] IE_SetColorMode(%d)\n", mode));
    ie_write32(IE_VIDEO_COLOR_MODE, (ULONG)mode);
}

void IE_SetFBBase(ULONG addr)
{
    D(bug("[IEGfx] IE_SetFBBase(0x%08lx)\n", addr));
    ie_write32(IE_VIDEO_FB_BASE, addr);
}

void IE_LoadCLUT(ULONG *palette, UWORD startIndex, UWORD count)
{
    UWORD i;

    D(bug("[IEGfx] IE_LoadCLUT(start=%d, count=%d)\n", startIndex, count));

    for (i = 0; i < count; i++)
    {
        ie_write32(IE_VIDEO_PAL_ENTRY(startIndex + i), palette[i]);
    }
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
    ie_write32(IE_BLT_FLAGS, 0);
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
    ie_write32(IE_BLT_FLAGS, 0);
    ie_write32(IE_BLT_CTRL, IE_BLT_CTRL_START);
}

void IE_BlitWait(void)
{
    /* IE blitter is synchronous — no wait needed */
    /* Left as a placeholder for future async blitter support */
}

void IE_BlitFillEx(ULONG dst, UWORD w, UWORD h, UWORD stride,
                   ULONG color, ULONG flags)
{
    D(bug("[IEGfx] BlitFillEx(dst=%08lx, %dx%d, color=%08lx, flags=%08lx)\n",
          dst, w, h, color, flags));

    ie_write32(IE_BLT_OP, IE_BLT_OP_FILL);
    ie_write32(IE_BLT_DST, dst);
    ie_write32(IE_BLT_WIDTH, (ULONG)w);
    ie_write32(IE_BLT_HEIGHT, (ULONG)h);
    ie_write32(IE_BLT_DST_STRIDE, (ULONG)stride);
    ie_write32(IE_BLT_COLOR, color);
    ie_write32(IE_BLT_FLAGS, flags);
    ie_write32(IE_BLT_CTRL, IE_BLT_CTRL_START);
}

void IE_BlitCopyEx(ULONG src, ULONG dst, UWORD w, UWORD h,
                   UWORD src_stride, UWORD dst_stride, ULONG flags)
{
    D(bug("[IEGfx] BlitCopyEx(src=%08lx, dst=%08lx, %dx%d, flags=%08lx)\n",
          src, dst, w, h, flags));

    ie_write32(IE_BLT_OP, IE_BLT_OP_COPY);
    ie_write32(IE_BLT_SRC, src);
    ie_write32(IE_BLT_DST, dst);
    ie_write32(IE_BLT_WIDTH, (ULONG)w);
    ie_write32(IE_BLT_HEIGHT, (ULONG)h);
    ie_write32(IE_BLT_SRC_STRIDE, (ULONG)src_stride);
    ie_write32(IE_BLT_DST_STRIDE, (ULONG)dst_stride);
    ie_write32(IE_BLT_FLAGS, flags);
    ie_write32(IE_BLT_CTRL, IE_BLT_CTRL_START);
}

void IE_BlitColorExpand(ULONG mask, ULONG dst, UWORD w, UWORD h,
                        UWORD mask_mod, UWORD mask_srcx, UWORD dst_stride,
                        ULONG fg, ULONG bg, ULONG flags)
{
    D(bug("[IEGfx] BlitColorExpand(mask=%08lx, dst=%08lx, %dx%d, fg=%08lx, bg=%08lx)\n",
          mask, dst, w, h, fg, bg));

    ie_write32(IE_BLT_OP, IE_BLT_OP_COLOR_EXPAND);
    ie_write32(IE_BLT_MASK, mask);
    ie_write32(IE_BLT_DST, dst);
    ie_write32(IE_BLT_WIDTH, (ULONG)w);
    ie_write32(IE_BLT_HEIGHT, (ULONG)h);
    ie_write32(IE_BLT_MASK_MOD, (ULONG)mask_mod);
    ie_write32(IE_BLT_MASK_SRCX, (ULONG)mask_srcx);
    ie_write32(IE_BLT_DST_STRIDE, (ULONG)dst_stride);
    ie_write32(IE_BLT_FG, fg);
    ie_write32(IE_BLT_BG, bg);
    ie_write32(IE_BLT_FLAGS, flags);
    ie_write32(IE_BLT_CTRL, IE_BLT_CTRL_START);
}

void IE_BlitLineEx(ULONG dst, UWORD dst_stride, WORD x0, WORD y0,
                   WORD x1, WORD y1, ULONG color, ULONG flags)
{
    D(bug("[IEGfx] BlitLineEx(dst=%08lx, stride=%d, (%d,%d)-(%d,%d), color=%08lx)\n",
          dst, dst_stride, x0, y0, x1, y1, color));

    /*
     * Extended line mode: BLT_FLAGS != 0 signals the Go blitter to use
     * BLT_DST as framebuffer base and BLT_WIDTH as packed endpoint coords.
     * BLT_SRC holds the start point, BLT_COLOR holds the line color.
     */
    ie_write32(IE_BLT_OP, IE_BLT_OP_LINE);
    ie_write32(IE_BLT_SRC, ((ULONG)(UWORD)y0 << 16) | (ULONG)(UWORD)x0);
    ie_write32(IE_BLT_WIDTH, ((ULONG)(UWORD)y1 << 16) | (ULONG)(UWORD)x1);
    ie_write32(IE_BLT_DST, dst);
    ie_write32(IE_BLT_DST_STRIDE, (ULONG)dst_stride);
    ie_write32(IE_BLT_COLOR, color);
    ie_write32(IE_BLT_FLAGS, flags);
    ie_write32(IE_BLT_CTRL, IE_BLT_CTRL_START);
}

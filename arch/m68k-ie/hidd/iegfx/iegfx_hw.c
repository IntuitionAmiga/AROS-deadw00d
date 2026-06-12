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

/*
 * Masked copy: copies src pixels to dst where the 1-bit mask is set.
 * mask_stride is the mask row stride in bytes (0 = packed), mask_srcx the
 * starting bit offset within the mask row. Pass IE_BLT_FLAGS_MASK_MSB for
 * Amiga PLANEPTR bit order.
 */
void IE_BlitMaskedCopy(ULONG src, ULONG dst, UWORD w, UWORD h,
                       UWORD src_stride, UWORD dst_stride,
                       ULONG mask, UWORD mask_stride, UWORD mask_srcx,
                       ULONG flags)
{
    D(bug("[IEGfx] BlitMaskedCopy(src=%08lx, dst=%08lx, %dx%d, mask=%08lx)\n",
          src, dst, w, h, mask));
    ie_write32(IE_BLT_OP, IE_BLT_OP_MASKED_COPY);
    ie_write32(IE_BLT_SRC, src);
    ie_write32(IE_BLT_DST, dst);
    ie_write32(IE_BLT_WIDTH, (ULONG)w);
    ie_write32(IE_BLT_HEIGHT, (ULONG)h);
    ie_write32(IE_BLT_SRC_STRIDE, (ULONG)src_stride);
    ie_write32(IE_BLT_DST_STRIDE, (ULONG)dst_stride);
    ie_write32(IE_BLT_MASK, mask);
    ie_write32(IE_BLT_MASK_MOD, (ULONG)mask_stride);
    ie_write32(IE_BLT_MASK_SRCX, (ULONG)mask_srcx);
    ie_write32(IE_BLT_FLAGS, flags);
    ie_write32(IE_BLT_CTRL, IE_BLT_CTRL_START);
}

/*
 * Alpha-template blend: src is an 8bpp alpha plane; each set pixel blends
 * the fg colour over dst (source-over). fg is in blitter little-endian
 * order (use ie_blt_color-style swapping at the call site for RGBA32).
 */
void IE_BlitAlphaTemplate(ULONG alpha, ULONG dst, UWORD w, UWORD h,
                          UWORD alpha_stride, UWORD dst_stride,
                          ULONG fg, ULONG flags)
{
    D(bug("[IEGfx] BlitAlphaTemplate(a=%08lx, dst=%08lx, %dx%d, fg=%08lx)\n",
          alpha, dst, w, h, fg));
    ie_write32(IE_BLT_OP, IE_BLT_OP_ALPHA_COPY);
    ie_write32(IE_BLT_SRC, alpha);
    ie_write32(IE_BLT_DST, dst);
    ie_write32(IE_BLT_WIDTH, (ULONG)w);
    ie_write32(IE_BLT_HEIGHT, (ULONG)h);
    ie_write32(IE_BLT_SRC_STRIDE, (ULONG)alpha_stride);
    ie_write32(IE_BLT_DST_STRIDE, (ULONG)dst_stride);
    ie_write32(IE_BLT_FG, fg);
    ie_write32(IE_BLT_FLAGS, flags | IE_BLT_FLAGS_ALPHA_TMPL);
    ie_write32(IE_BLT_CTRL, IE_BLT_CTRL_START);
}

/*
 * Nearest-neighbour scaled copy. Destination size is packed into
 * BLT_COLOR (low word = width, high word = height).
 */
void IE_BlitScale(ULONG src, ULONG dst, UWORD src_w, UWORD src_h,
                  UWORD dst_w, UWORD dst_h,
                  UWORD src_stride, UWORD dst_stride, ULONG flags)
{
    D(bug("[IEGfx] BlitScale(src=%08lx %dx%d -> dst=%08lx %dx%d)\n",
          src, src_w, src_h, dst, dst_w, dst_h));
    ie_write32(IE_BLT_OP, IE_BLT_OP_SCALE);
    ie_write32(IE_BLT_SRC, src);
    ie_write32(IE_BLT_DST, dst);
    ie_write32(IE_BLT_WIDTH, (ULONG)src_w);
    ie_write32(IE_BLT_HEIGHT, (ULONG)src_h);
    ie_write32(IE_BLT_SRC_STRIDE, (ULONG)src_stride);
    ie_write32(IE_BLT_DST_STRIDE, (ULONG)dst_stride);
    ie_write32(IE_BLT_COLOR, (ULONG)dst_w | ((ULONG)dst_h << 16));
    ie_write32(IE_BLT_FLAGS, flags);
    ie_write32(IE_BLT_CTRL, IE_BLT_CTRL_START);
}

/*
 * Affine (Mode 7) textured fill of a w x h destination rectangle.
 * Texture coordinates are 16.16 fixed point; tex masks are 2^n - 1.
 */
void IE_BlitMode7(ULONG src, ULONG dst, UWORD w, UWORD h,
                  UWORD dst_stride,
                  LONG u0, LONG v0, LONG du_col, LONG dv_col,
                  LONG du_row, LONG dv_row,
                  UWORD tex_w_mask, UWORD tex_h_mask, ULONG flags)
{
    ie_write32(IE_BLT_OP, IE_BLT_OP_MODE7);
    ie_write32(IE_BLT_SRC, src);
    ie_write32(IE_BLT_DST, dst);
    ie_write32(IE_BLT_WIDTH, (ULONG)w);
    ie_write32(IE_BLT_HEIGHT, (ULONG)h);
    ie_write32(IE_BLT_DST_STRIDE, (ULONG)dst_stride);
    ie_write32(IE_BLT_MODE7_U0, (ULONG)u0);
    ie_write32(IE_BLT_MODE7_V0, (ULONG)v0);
    ie_write32(IE_BLT_MODE7_DU_COL, (ULONG)du_col);
    ie_write32(IE_BLT_MODE7_DV_COL, (ULONG)dv_col);
    ie_write32(IE_BLT_MODE7_DU_ROW, (ULONG)du_row);
    ie_write32(IE_BLT_MODE7_DV_ROW, (ULONG)dv_row);
    ie_write32(IE_BLT_MODE7_TEX_W, (ULONG)tex_w_mask);
    ie_write32(IE_BLT_MODE7_TEX_H, (ULONG)tex_h_mask);
    ie_write32(IE_BLT_FLAGS, flags);
    ie_write32(IE_BLT_CTRL, IE_BLT_CTRL_START);
}

/*
 * Wait for the next vertical blank. Used to synchronise framebuffer
 * base swaps so screen switches are tear-free.
 */
void IE_WaitVBlank(void)
{
    ULONG spin;
    /* If currently in vblank, wait for it to end first */
    for (spin = 0; spin < 1000000; spin++)
    {
        if (!(ie_read32(IE_VIDEO_STATUS) & IE_VIDEO_STATUS_VBLANK))
            break;
    }
    for (spin = 0; spin < 1000000; spin++)
    {
        if (ie_read32(IE_VIDEO_STATUS) & IE_VIDEO_STATUS_VBLANK)
            break;
    }
}

/*
 * Load and start a copper display list (WAIT/MOVE/SETBASE/END
 * instructions, see ie_hwreg.h).
 */
void IE_CopperLoad(ULONG listaddr)
{
    ie_write32(IE_COPPER_PTR, listaddr);
    ie_write32(IE_COPPER_CTRL, 1);
}

void IE_CopperStop(void)
{
    ie_write32(IE_COPPER_CTRL, 0);
}

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Intuition Engine Graphics hardware interface.
*/

#ifndef IEGFX_HW_H
#define IEGFX_HW_H

#include <exec/types.h>
#include "ie_hwreg.h"

/* IE VideoChip display modes — aliases for ie_hwreg.h constants */
#define IE_MODE_640x480         IE_VIDEO_MODE_640x480
#define IE_MODE_800x600         IE_VIDEO_MODE_800x600
#define IE_MODE_1024x768        IE_VIDEO_MODE_1024x768
#define IE_MODE_1280x960        IE_VIDEO_MODE_1280x960

/* IE blitter operations */
#define IE_BLT_OP_COPY          0
#define IE_BLT_OP_FILL          1
#define IE_BLT_OP_LINE          2
#define IE_BLT_OP_MASKED_COPY   3
#define IE_BLT_OP_ALPHA_COPY    4
#define IE_BLT_OP_MODE7         5

/* Blitter control bits */
#define IE_BLT_CTRL_START       (1 << 0)
#define IE_BLT_CTRL_BUSY        (1 << 1)

/* VideoChip control bits */
#define IE_VIDEO_CTRL_ENABLE    (1 << 0)

/* VideoChip status bits */
#define IE_VIDEO_STATUS_VBLANK  (1 << 1)

/* Color mode values */
#define IE_COLORMODE_RGBA32     0
#define IE_COLORMODE_CLUT8      1

/* Hardware functions */
BOOL IE_VideoInit(void);
void IE_VideoEnable(BOOL enable);
void IE_VideoSetMode(UWORD mode);
void IE_SetColorMode(UWORD mode);
void IE_SetFBBase(ULONG addr);
void IE_LoadCLUT(ULONG *palette, UWORD startIndex, UWORD count);
void IE_BlitCopy(ULONG src, ULONG dst, UWORD width, UWORD height,
                 UWORD src_stride, UWORD dst_stride);
void IE_BlitFill(ULONG dst, UWORD width, UWORD height,
                 UWORD dst_stride, ULONG color);
void IE_BlitWait(void);

#endif /* IEGFX_HW_H */

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Intuition Engine Graphics bitmap header.
*/

#ifndef IEGFX_BITMAP_H
#define IEGFX_BITMAP_H

#include "iegfx_hidd.h"
#include "iegfx_hw.h"

#include <hidd/gfx.h>

#define IS_BM_ATTR(attr, idx) ( ( (idx) = (attr) - HiddBitMapAttrBase) < num_Hidd_BitMap_Attrs)

/*
   This structure is used as instance data for the bitmap class.
   Supports RGBA32 (direct color) and CLUT8 (256-color palette).
*/
struct IEGfxBitmapData
{
    UBYTE       *VideoData;     /* Framebuffer pointer (in VRAM) */
    LONG        width;          /* Bitmap size */
    LONG        height;
    UBYTE       bytesperpix;
    ULONG       bytesperline;
    BYTE        disp;           /* !=0 - displayable */
    OOP_Object  *pixfmtobj;    /* Cached pixelformat object */
    OOP_Object  *gfxhidd;     /* Cached driver object */
    LONG        xoffset;        /* Bitmap offset */
    LONG        yoffset;
    ULONG       *CLUT;          /* Palette (256 entries) for CLUT8 bitmaps, NULL for RGBA32 */
};

#endif /* IEGFX_BITMAP_H */

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Bitmap class for IE Gfx HIDD.
          Displayable bitmaps are allocated in VRAM (0x100000+).
          Supports RGBA32 (direct color) and CLUT8 (256-color palette).
          Hardware-accelerated FillRect, Clear, PutTemplate, PutAlphaTemplate, PutImage, GetImage, DrawLine.
*/

#define __OOP_NOATTRBASES__

#define DEBUG 0
#include <aros/debug.h>

#include <proto/oop.h>
#include <proto/utility.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <graphics/rastport.h>
#include <graphics/gfx.h>
#include <hidd/hidd.h>
#include <hidd/gfx.h>
#include <oop/oop.h>
#include <aros/symbolsets.h>

#include <string.h>

#include <ie_hwreg.h>
#ifdef __mc68000__
#include <libraries/iewarp.h>

static struct Library *IEWarpBase = NULL;
#include <iewarp_consumer.h>
#endif

#include "iegfx_bitmap.h"
#include "iegfx_hidd.h"

#include LC_LIBDEFS_FILE

/* Coprocessor warp dispatch threshold (bytes) */
#define WARP_THRESHOLD 4096

#ifdef __mc68000__
/*
 * Dispatch a fill or copy operation to the IE64 coprocessor via MMIO.
 * Returns TRUE if dispatched and completed, FALSE if caller should fallback.
 */
static BOOL IE_WarpDoFillRect(ULONG dst, UWORD w, UWORD h, UWORD stride,
                              ULONG color, UBYTE bpp)
{
    ULONG size = (ULONG)w * (ULONG)h * (ULONG)bpp;
    if (size < WARP_THRESHOLD)
        return FALSE;

    if (!IEWARP_OPEN())
        return FALSE;

    IEWarpSetCaller(IEWARP_CALLER_IEGFX);
    {
        ULONG ticket = IEWarpFillRect(
            (APTR)dst, (UWORD)(w * bpp), h, stride, color);
        if (ticket)
        {
            IEWarpWait(ticket);
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL IE_WarpDoBlitCopy(ULONG src, ULONG dst, UWORD w, UWORD h,
                              UWORD src_stride, UWORD dst_stride, UBYTE bpp)
{
    ULONG size = (ULONG)w * (ULONG)h * (ULONG)bpp;
    if (size < WARP_THRESHOLD)
        return FALSE;

    if (!IEWARP_OPEN())
        return FALSE;

    IEWarpSetCaller(IEWARP_CALLER_IEGFX);
    {
        ULONG ticket = IEWarpBlitCopy(
            (APTR)src, (APTR)dst,
            (UWORD)(w * bpp), h,
            src_stride, dst_stride, 0xC0);
        if (ticket)
        {
            IEWarpWait(ticket);
            return TRUE;
        }
    }
    return FALSE;
}
#endif

/*********** BitMap::New() *************************************/

OOP_Object *METHOD(IEBitMap, Root, New)
{
    D(bug("[IEBitMap] BitMap::New()\n"));

    o = (OOP_Object *)OOP_DoSuperMethod(cl, o, (OOP_Msg) msg);
    if (o)
    {
        struct IEGfxBitmapData *data;
        HIDDT_ModeID            modeid;
        OOP_Object              *sync, *pf;
        IPTR                    colormodel;
        struct TagItem attrs[] = {
            { aHidd_ChunkyBM_Buffer, 0 },
            { TAG_DONE, 0UL }
        };

        data = OOP_INST_DATA(cl, o);

        /* Get attr values */
        OOP_GetAttr(o, aHidd_BitMap_GfxHidd , (APTR)&data->gfxhidd);
        OOP_GetAttr(o, aHidd_BitMap_PixFmt  , (APTR)&data->pixfmtobj);
        OOP_GetAttr(o, aHidd_BitMap_ModeID  , &modeid);

        HIDD_Gfx_GetMode(data->gfxhidd, modeid, &sync, &pf);

        data->width         = OOP_GET(o, aHidd_BitMap_Width);
        data->height        = OOP_GET(o, aHidd_BitMap_Height);
        data->bytesperline  = OOP_GET(o, aHidd_BitMap_BytesPerRow);
        data->bytesperpix   = OOP_GET(data->pixfmtobj, aHidd_PixFmt_BytesPerPixel);

        /* Detect palette mode and allocate CLUT */
        OOP_GetAttr(data->pixfmtobj, aHidd_PixFmt_ColorModel, &colormodel);
        data->CLUT = NULL;

        if (colormodel == vHidd_ColorModel_Palette)
        {
            struct IEGfx_staticdata *xsd = XSD(cl);

            D(bug("[IEBitMap] Palette bitmap — allocating CLUT\n"));
            data->CLUT = AllocVecPooled(xsd->mempool, 256 * sizeof(ULONG));
            if (data->CLUT)
                memset(data->CLUT, 0, 256 * sizeof(ULONG));
        }

        D(bug("[IEBitMap] Bitmap %ld x %ld, %u bytes per pixel, %u bytes per line\n",
                data->width, data->height, data->bytesperpix, data->bytesperline));

        /*
         * Allocate framebuffer in VRAM via bump allocator.
         * VRAM at 0x100000-0x5FFFFF is directly accessible by both
         * the CPU and the VideoChip — no copy needed for display.
         */
        {
            struct IEGfx_staticdata *xsd = XSD(cl);
            ULONG size = data->bytesperline * data->height;

            /* 32-byte alignment for DMA-friendly access */
            UBYTE *aligned = (UBYTE *)(((IPTR)xsd->vram_next + 31) & ~31);

            if (aligned + size <= xsd->vram_end)
            {
                data->VideoData = aligned;
                xsd->vram_next = aligned + size;

                D(bug("[IEBitMap] Allocated %lu bytes in VRAM at %p\n",
                      size, data->VideoData));
            }
            else
            {
                D(bug("[IEBitMap] VRAM full! Falling back to pool allocation\n"));
                data->VideoData = AllocVecPooled(xsd->mempool,
                                                 data->bytesperline * data->height + 64);
                if (data->VideoData)
                    data->VideoData = (UBYTE *)(((IPTR)data->VideoData + 31) & ~31);
            }
        }

        if (!data->VideoData)
        {
            OOP_MethodID disp_mid = OOP_GetMethodID(IID_Root, moRoot_Dispose);
            OOP_CoerceMethod(cl, o, (OOP_Msg)&disp_mid);
            return NULL;
        }

        /* Clear the framebuffer */
        {
            ULONG clearSize = data->bytesperline * data->height;
#ifdef __mc68000__
            BOOL cleared = FALSE;

            if (clearSize >= WARP_THRESHOLD && IEWARP_OPEN())
            {
                IEWarpSetCaller(IEWARP_CALLER_IEGFX);
                {
                    ULONG ticket = IEWarpMemSet(data->VideoData, 0, clearSize);
                    if (ticket)
                    {
                        IEWarpWait(ticket);
                        cleared = TRUE;
                    }
                }
            }

            if (!cleared)
#endif
                memset(data->VideoData, 0, clearSize);
        }

        /* Tell ChunkyBM superclass where to draw */
        attrs[0].ti_Data = (IPTR)data->VideoData;
        OOP_SetAttrs(o, attrs);

        data->disp = 1;

        D(bug("[IEBitMap] Video data at 0x%p (%lu bytes)\n",
              data->VideoData, data->bytesperline * data->height));
    }

    D(bug("[IEBitMap] Returning object %p\n", o));

    return o;
}

/*********** BitMap::Dispose() *********************************/

VOID METHOD(IEBitMap, Root, Dispose)
{
    struct IEGfxBitmapData *data = OOP_INST_DATA(cl, o);

    D(bug("[IEBitMap] Dispose(0x%p)\n", o));

    /*
     * Free CLUT if allocated. CLUT is pool-allocated so we can free it.
     * VRAM-allocated buffers use a bump allocator — no individual free.
     */
    if (data->CLUT)
    {
        struct IEGfx_staticdata *xsd = XSD(cl);
        FreeVecPooled(xsd->mempool, data->CLUT);
        data->CLUT = NULL;
    }

    OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
}

/*********** BitMap::Get() *************************************/

VOID METHOD(IEBitMap, Root, Get)
{
    struct IEGfxBitmapData *data = OOP_INST_DATA(cl, o);
    ULONG idx;

    if (IS_BM_ATTR(msg->attrID, idx))
    {
        switch (idx)
        {
        case aoHidd_BitMap_Visible:
            *msg->storage = data->disp;
            return;

        case aoHidd_BitMap_LeftEdge:
            *msg->storage = data->xoffset;
            return;

        case aoHidd_BitMap_TopEdge:
            *msg->storage = data->yoffset;
            return;
        }
    }
    OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
}

/*********** BitMap::SetColors() ********************************/

BOOL METHOD(IEBitMap, Hidd_BitMap, SetColors)
{
    struct IEGfxBitmapData *data = OOP_INST_DATA(cl, o);
    UWORD i;

    /* Let superclass handle the colormap update first */
    if (!OOP_DoSuperMethod(cl, o, (OOP_Msg)msg))
        return FALSE;

    if (!data->CLUT)
        return TRUE;

    /* Convert 16-bit AROS colors to 0x00RRGGBB and store in CLUT */
    for (i = 0; i < msg->numColors; i++)
    {
        UWORD idx = msg->firstColor + i;
        if (idx >= 256)
            break;

        UBYTE r = msg->colors[i].red   >> 8;
        UBYTE g = msg->colors[i].green >> 8;
        UBYTE b = msg->colors[i].blue  >> 8;

        data->CLUT[idx] = ((ULONG)r << 16) | ((ULONG)g << 8) | (ULONG)b;
    }

    /* If this bitmap is currently visible, upload palette to hardware */
    {
        struct IEGfx_staticdata *xsd = XSD(cl);
        if (xsd->visible == o)
        {
            IE_LoadCLUT(data->CLUT + msg->firstColor,
                        msg->firstColor, msg->numColors);
        }
    }

    return TRUE;
}

/*********** BitMap::FillRect() ********************************/

VOID METHOD(IEBitMap, Hidd_BitMap, FillRect)
{
    struct IEGfxBitmapData *data = OOP_INST_DATA(cl, o);
    ULONG mode = GC_DRMD(msg->gc);
    HIDDT_Pixel fg = GC_FG(msg->gc);

    D(bug("[IEBitMap] FillRect(%d,%d - %d,%d, mode=%d, fg=0x%08lx)\n",
          msg->minX, msg->minY, msg->maxX, msg->maxY, mode, fg));

    /* COLMASK guard: partial masks need per-pixel read-modify-write */
    if (GC_COLMASK(msg->gc) != (HIDDT_Pixel)~0)
    {
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
        return;
    }

    {
        UWORD w = msg->maxX - msg->minX + 1;
        UWORD h = msg->maxY - msg->minY + 1;
        ULONG dst = (ULONG)data->VideoData +
                    msg->minY * data->bytesperline +
                    msg->minX * data->bytesperpix;

        /* Try coprocessor for large Copy-mode fills */
#ifdef __mc68000__
        if (mode == vHidd_GC_DrawMode_Copy &&
            IE_WarpDoFillRect(dst, w, h, data->bytesperline,
                            fg, data->bytesperpix))
            return;
#endif

        {
            ULONG bpp_flag = (data->bytesperpix == 1) ?
                IE_BLT_FLAGS_BPP_CLUT8 : IE_BLT_FLAGS_BPP_RGBA32;
            ULONG flags = IE_BLT_MAKE_FLAGS(bpp_flag, mode);

            IE_BlitFillEx(dst, w, h, data->bytesperline, fg, flags);
        }
    }
}

/*********** BitMap::Clear() ***********************************/

VOID METHOD(IEBitMap, Hidd_BitMap, Clear)
{
    struct IEGfxBitmapData *data = OOP_INST_DATA(cl, o);
    HIDDT_Pixel bg = GC_BG(msg->gc);

    D(bug("[IEBitMap] Clear(bg=0x%08lx)\n", bg));

    {
        ULONG dst = (ULONG)data->VideoData;

        /* Full bitmap clear — always qualifies for coprocessor */
#ifdef __mc68000__
        if (IE_WarpDoFillRect(dst, data->width, data->height,
                            data->bytesperline, bg, data->bytesperpix))
            return;
#endif

        {
            ULONG bpp_flag = (data->bytesperpix == 1) ?
                IE_BLT_FLAGS_BPP_CLUT8 : IE_BLT_FLAGS_BPP_RGBA32;
            ULONG flags = IE_BLT_MAKE_FLAGS(bpp_flag, vHidd_GC_DrawMode_Copy);

            IE_BlitFillEx(dst, data->width, data->height,
                          data->bytesperline, bg, flags);
        }
    }
}

/*********** BitMap::PutTemplate() *****************************/

VOID METHOD(IEBitMap, Hidd_BitMap, PutTemplate)
{
    struct IEGfxBitmapData *data = OOP_INST_DATA(cl, o);
    ULONG mode = GC_DRMD(msg->gc);

    D(bug("[IEBitMap] PutTemplate(%d,%d %dx%d, mode=%d)\n",
          msg->x, msg->y, msg->width, msg->height, mode));

    /* Only handle Copy and Invert draw modes; others fall to superclass */
    if (mode != vHidd_GC_DrawMode_Copy && mode != vHidd_GC_DrawMode_Invert)
    {
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
        return;
    }

    {
        ULONG bpp_flag = (data->bytesperpix == 1) ?
            IE_BLT_FLAGS_BPP_CLUT8 : IE_BLT_FLAGS_BPP_RGBA32;
        ULONG flags = IE_BLT_MAKE_FLAGS(bpp_flag, vHidd_GC_DrawMode_Copy);

        if (mode == vHidd_GC_DrawMode_Invert)
        {
            /* Invert mode: FG/BG ignored, XOR destination where template bit=1 */
            flags |= IE_BLT_FLAGS_INVERT_MODE;
        }
        else
        {
            /* Copy mode: check color expansion type */
            if (GC_COLEXP(msg->gc) == vHidd_GC_ColExp_Transparent)
                flags |= IE_BLT_FLAGS_JAM1;
            /* else JAM2 (opaque): bit cleared = write BG */
        }

        if (msg->inverttemplate)
            flags |= IE_BLT_FLAGS_INVERT_TMPL;

        ULONG fg = GC_FG(msg->gc);
        ULONG bg = GC_BG(msg->gc);
        ULONG mask = (ULONG)msg->masktemplate;
        ULONG dst = (ULONG)data->VideoData +
                    msg->y * data->bytesperline +
                    msg->x * data->bytesperpix;

        IE_BlitColorExpand(mask, dst, msg->width, msg->height,
                           msg->modulo, msg->srcx, data->bytesperline,
                           fg, bg, flags);
    }
}

/*********** BitMap::PutAlphaTemplate() ************************/

VOID METHOD(IEBitMap, Hidd_BitMap, PutAlphaTemplate)
{
    struct IEGfxBitmapData *data = OOP_INST_DATA(cl, o);

    D(bug("[IEBitMap] PutAlphaTemplate(%d,%d %dx%d)\n",
          msg->x, msg->y, msg->width, msg->height));

    /* Only accelerate RGBA32 bitmaps */
    if (data->bytesperpix != 4)
    {
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
        return;
    }

    {
        ULONG size = (ULONG)msg->width * (ULONG)msg->height;
        ULONG fg = GC_FG(msg->gc);
        ULONG dst = (ULONG)data->VideoData +
                    msg->y * data->bytesperline +
                    msg->x * data->bytesperpix;

        /* Try iewarp.library for large alpha blits */
#ifdef __mc68000__
        if (size >= WARP_THRESHOLD && IEWARP_OPEN())
        {
            IEWarpSetCaller(IEWARP_CALLER_IEGFX);
            {
                ULONG ticket = IEWarpBlitAlpha(
                    (APTR)msg->alpha, (APTR)dst,
                    msg->width, msg->height,
                    msg->modulo, data->bytesperline);
                if (ticket)
                {
                    IEWarpWait(ticket);
                    return;
                }
            }
            /* Dispatch failed — fall through to superclass */
        }
#endif
    }

    OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
}

/*********** BitMap::PutImage() ********************************/

VOID METHOD(IEBitMap, Hidd_BitMap, PutImage)
{
    struct IEGfxBitmapData *data = OOP_INST_DATA(cl, o);
    ULONG mode = GC_DRMD(msg->gc);

    D(bug("[IEBitMap] PutImage(%d,%d %dx%d, fmt=%d, mode=%d)\n",
          msg->x, msg->y, msg->width, msg->height, msg->pixFmt, mode));

    /* Only accelerate Copy mode with full color mask */
    if (mode != vHidd_GC_DrawMode_Copy || GC_COLMASK(msg->gc) != (HIDDT_Pixel)~0)
    {
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
        return;
    }

    /* Only accelerate Native format (src pixels match bitmap BPP) */
    if (msg->pixFmt == vHidd_StdPixFmt_Native)
    {
        ULONG src = (ULONG)msg->pixels;
        ULONG dst = (ULONG)data->VideoData +
                    msg->y * data->bytesperline +
                    msg->x * data->bytesperpix;

        /* Try coprocessor for large images */
#ifdef __mc68000__
        if (IE_WarpDoBlitCopy(src, dst, msg->width, msg->height,
                            msg->modulo, data->bytesperline,
                            data->bytesperpix))
            return;
#endif

        {
            ULONG bpp_flag = (data->bytesperpix == 1) ?
                IE_BLT_FLAGS_BPP_CLUT8 : IE_BLT_FLAGS_BPP_RGBA32;
            ULONG flags = IE_BLT_MAKE_FLAGS(bpp_flag, vHidd_GC_DrawMode_Copy);

            IE_BlitCopyEx(src, dst, msg->width, msg->height,
                          msg->modulo, data->bytesperline, flags);
        }
        return;
    }

    /* Native32 is always 4 bpp — only fast-path when bitmap is also RGBA32 */
    if (msg->pixFmt == vHidd_StdPixFmt_Native32 && data->bytesperpix == 4)
    {
        ULONG src = (ULONG)msg->pixels;
        ULONG dst = (ULONG)data->VideoData +
                    msg->y * data->bytesperline +
                    msg->x * data->bytesperpix;

        /* Try coprocessor for large images */
#ifdef __mc68000__
        if (IE_WarpDoBlitCopy(src, dst, msg->width, msg->height,
                            msg->modulo, data->bytesperline, 4))
            return;
#endif

        {
            ULONG flags = IE_BLT_MAKE_FLAGS(IE_BLT_FLAGS_BPP_RGBA32,
                                             vHidd_GC_DrawMode_Copy);

            IE_BlitCopyEx(src, dst, msg->width, msg->height,
                          msg->modulo, data->bytesperline, flags);
        }
        return;
    }

    /* Try IE64 format conversion for known 32-bit and 24-bit formats → RGBA32 */
#ifdef __mc68000__
    if (data->bytesperpix == 4)
    {
        ULONG warpSrcFmt = 0;

        switch (msg->pixFmt)
        {
        case vHidd_StdPixFmt_ARGB32: warpSrcFmt = WARP_PIXFMT_ARGB32; break;
        case vHidd_StdPixFmt_BGRA32: warpSrcFmt = WARP_PIXFMT_BGRA32; break;
        case vHidd_StdPixFmt_ABGR32: warpSrcFmt = WARP_PIXFMT_ABGR32; break;
        case vHidd_StdPixFmt_RGB24:  warpSrcFmt = WARP_PIXFMT_RGB24;  break;
        case vHidd_StdPixFmt_BGR24:  warpSrcFmt = WARP_PIXFMT_BGR24;  break;
        default: break;
        }

        if (warpSrcFmt)
        {
            ULONG size = (ULONG)msg->width * (ULONG)msg->height;

            if (size >= WARP_THRESHOLD && IEWARP_OPEN())
            {
                ULONG dst = (ULONG)data->VideoData +
                            msg->y * data->bytesperline +
                            msg->x * data->bytesperpix;

                IEWarpSetCaller(IEWARP_CALLER_IEGFX);
                {
                    ULONG ticket = IEWarpBlitConvert(
                        (APTR)msg->pixels, (APTR)dst,
                        msg->width, msg->height,
                        warpSrcFmt, WARP_PIXFMT_RGBA32);
                    if (ticket)
                    {
                        IEWarpWait(ticket);
                        return;
                    }
                }
                /* Dispatch failed — fall through to superclass */
            }
        }
    }
#endif

    /* Other pixel formats: fall back to superclass for conversion */
    OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
}

/*********** BitMap::DrawLine() *******************************/

/*
 * Cohen-Sutherland line clipping.
 * Clips line (x0,y0)-(x1,y1) to the rectangle (cx1,cy1)-(cx2,cy2).
 * Returns TRUE if the clipped line is visible, FALSE if fully rejected.
 */
#define CS_INSIDE 0
#define CS_LEFT   1
#define CS_RIGHT  2
#define CS_BOTTOM 4
#define CS_TOP    8

static inline int cs_outcode(WORD x, WORD y, WORD cx1, WORD cy1, WORD cx2, WORD cy2)
{
    int code = CS_INSIDE;
    if (x < cx1) code |= CS_LEFT;
    else if (x > cx2) code |= CS_RIGHT;
    if (y < cy1) code |= CS_TOP;
    else if (y > cy2) code |= CS_BOTTOM;
    return code;
}

static BOOL cs_clip_line(WORD *px0, WORD *py0, WORD *px1, WORD *py1,
                         WORD cx1, WORD cy1, WORD cx2, WORD cy2)
{
    WORD x0 = *px0, y0 = *py0, x1 = *px1, y1 = *py1;
    int oc0 = cs_outcode(x0, y0, cx1, cy1, cx2, cy2);
    int oc1 = cs_outcode(x1, y1, cx1, cy1, cx2, cy2);

    for (;;)
    {
        if (!(oc0 | oc1))
        {
            /* Both inside */
            *px0 = x0; *py0 = y0;
            *px1 = x1; *py1 = y1;
            return TRUE;
        }
        if (oc0 & oc1)
        {
            /* Both outside same edge — fully rejected */
            return FALSE;
        }

        /* Pick the endpoint that is outside */
        {
            int oc = oc0 ? oc0 : oc1;
            WORD x, y;
            LONG dx = (LONG)x1 - (LONG)x0;
            LONG dy = (LONG)y1 - (LONG)y0;

            if (oc & CS_BOTTOM)
            {
                x = x0 + (WORD)(dx * (cy2 - y0) / dy);
                y = cy2;
            }
            else if (oc & CS_TOP)
            {
                x = x0 + (WORD)(dx * (cy1 - y0) / dy);
                y = cy1;
            }
            else if (oc & CS_RIGHT)
            {
                y = y0 + (WORD)(dy * (cx2 - x0) / dx);
                x = cx2;
            }
            else /* CS_LEFT */
            {
                y = y0 + (WORD)(dy * (cx1 - x0) / dx);
                x = cx1;
            }

            if (oc == oc0)
            {
                x0 = x; y0 = y;
                oc0 = cs_outcode(x0, y0, cx1, cy1, cx2, cy2);
            }
            else
            {
                x1 = x; y1 = y;
                oc1 = cs_outcode(x1, y1, cx1, cy1, cx2, cy2);
            }
        }
    }
}

VOID METHOD(IEBitMap, Hidd_BitMap, DrawLine)
{
    struct IEGfxBitmapData *data = OOP_INST_DATA(cl, o);
    ULONG mode = GC_DRMD(msg->gc);

    D(bug("[IEBitMap] DrawLine(%d,%d - %d,%d, mode=%d)\n",
          msg->x1, msg->y1, msg->x2, msg->y2, mode));

    /* COLMASK guard: partial masks need per-pixel read-modify-write */
    if (GC_COLMASK(msg->gc) != (HIDDT_Pixel)~0)
    {
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
        return;
    }

    /* Line pattern guard: dashed/dotted lines stay in software */
    if (GC_LINEPAT(msg->gc) != (UWORD)~0)
    {
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
        return;
    }

    /* Only accelerate RGBA32 — CLUT8 lines remain in software */
    if (data->bytesperpix != 4)
    {
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
        return;
    }

    {
        ULONG dst = (ULONG)data->VideoData;
        HIDDT_Pixel fg = GC_FG(msg->gc);
        ULONG flags = IE_BLT_MAKE_FLAGS(IE_BLT_FLAGS_BPP_RGBA32, mode);
        WORD x0 = msg->x1, y0 = msg->y1;
        WORD x1 = msg->x2, y1 = msg->y2;

        /* Clip line to GC clip rectangle if clipping is enabled */
        if (GC_DOCLIP(msg->gc))
        {
            if (!cs_clip_line(&x0, &y0, &x1, &y1,
                              GC_CLIPX1(msg->gc), GC_CLIPY1(msg->gc),
                              GC_CLIPX2(msg->gc), GC_CLIPY2(msg->gc)))
                return; /* Line fully outside clip rect */
        }

        IE_BlitLineEx(dst, data->bytesperline, x0, y0, x1, y1, fg, flags);
    }
}

/*********** BitMap::GetImage() ********************************/

VOID METHOD(IEBitMap, Hidd_BitMap, GetImage)
{
    struct IEGfxBitmapData *data = OOP_INST_DATA(cl, o);

    D(bug("[IEBitMap] GetImage(%d,%d %dx%d, fmt=%d)\n",
          msg->x, msg->y, msg->width, msg->height, msg->pixFmt));

    /* Only accelerate Native format */
    if (msg->pixFmt == vHidd_StdPixFmt_Native)
    {
        ULONG src = (ULONG)data->VideoData +
                    msg->y * data->bytesperline +
                    msg->x * data->bytesperpix;
        ULONG dst = (ULONG)msg->pixels;
        ULONG bpp_flag = (data->bytesperpix == 1) ?
            IE_BLT_FLAGS_BPP_CLUT8 : IE_BLT_FLAGS_BPP_RGBA32;
        ULONG flags = IE_BLT_MAKE_FLAGS(bpp_flag, vHidd_GC_DrawMode_Copy);

        IE_BlitCopyEx(src, dst, msg->width, msg->height,
                      data->bytesperline, msg->modulo, flags);
        return;
    }

    /* Native32 — only fast-path when bitmap is RGBA32 */
    if (msg->pixFmt == vHidd_StdPixFmt_Native32 && data->bytesperpix == 4)
    {
        ULONG src = (ULONG)data->VideoData +
                    msg->y * data->bytesperline +
                    msg->x * data->bytesperpix;
        ULONG dst = (ULONG)msg->pixels;
        ULONG flags = IE_BLT_MAKE_FLAGS(IE_BLT_FLAGS_BPP_RGBA32,
                                         vHidd_GC_DrawMode_Copy);

        IE_BlitCopyEx(src, dst, msg->width, msg->height,
                      data->bytesperline, msg->modulo, flags);
        return;
    }

    OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
}

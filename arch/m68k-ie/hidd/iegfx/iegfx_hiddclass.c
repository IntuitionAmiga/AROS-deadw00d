/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Intuition Engine Graphics HIDD class.
          Multi-resolution (1920x1080, 1280x960, 1024x768, 800x600, 640x480)
          with CLUT8 and RGBA32 pixel formats.  Hardware blitter, no sprite.
*/

#define __OOP_NOATTRBASES__

#define DEBUG 0
#include <aros/debug.h>

#include <aros/asmcall.h>
#include <proto/exec.h>
#include <proto/oop.h>
#include <proto/utility.h>
#include <aros/symbolsets.h>
#include <exec/alerts.h>
#include <exec/memory.h>
#include <hidd/hidd.h>
#include <hidd/gfx.h>
#include <oop/oop.h>
#include <clib/alib_protos.h>
#include <string.h>

#include <ie_hwreg.h>
#include "iegfx_hidd.h"
#include "iegfx_bitmap.h"
#include "iegfx_hw.h"

#define MAKE_SYNC(name,clock,hdisp,hstart,hend,htotal,vdisp,vstart,vend,vtotal,flags,descr)   \
    struct TagItem sync_ ## name[]={            \
        { aHidd_Sync_PixelClock,    clock*1000  },  \
        { aHidd_Sync_HDisp,         hdisp   },  \
        { aHidd_Sync_HSyncStart,    hstart  },  \
        { aHidd_Sync_HSyncEnd,      hend    },  \
        { aHidd_Sync_HTotal,        htotal  },  \
        { aHidd_Sync_VDisp,         vdisp   },  \
        { aHidd_Sync_VSyncStart,    vstart  },  \
        { aHidd_Sync_VSyncEnd,      vend    },  \
        { aHidd_Sync_VTotal,        vtotal  },  \
        { aHidd_Sync_Flags,         flags   },  \
        { aHidd_Sync_Description,       (IPTR)descr},   \
        { TAG_DONE, 0UL }}


/*********** Root::New() *************************************/

OOP_Object *METHOD(IEGfx, Root, New)
{
    /*
     * IE supports display modes with standard VESA timings.
     * The pixel clocks are notional (IE renders at host framerate) but
     * correct timings are provided for AROS mode enumeration / ScreenMode prefs.
     * 1920x1080 is listed first so Workbench's default mode selection prefers
     * the full IE desktop while smaller modes remain available.
     */
    MAKE_SYNC(640x480,   25175,  640,  656,  752,  800,  480, 490, 492, 525, 0, "IE:640x480");
    MAKE_SYNC(800x600,   40000,  800,  840,  968, 1056,  600, 601, 605, 628, 0, "IE:800x600");
    MAKE_SYNC(1024x768,  65000, 1024, 1048, 1184, 1344,  768, 771, 777, 806, 0, "IE:1024x768");
    MAKE_SYNC(1280x960, 108000, 1280, 1376, 1488, 1800,  960, 961, 964,1000, 0, "IE:1280x960");
    MAKE_SYNC(1920x1080,148500, 1920, 2008, 2052, 2200, 1080,1084,1089,1125, 0, "IE:1920x1080");

    struct TagItem syncs[] = {
        { aHidd_Gfx_SyncTags,       (IPTR)sync_1920x1080 },
        { aHidd_Gfx_SyncTags,       (IPTR)sync_1280x960 },
        { aHidd_Gfx_SyncTags,       (IPTR)sync_1024x768 },
        { aHidd_Gfx_SyncTags,       (IPTR)sync_800x600  },
        { aHidd_Gfx_SyncTags,       (IPTR)sync_640x480  },
        { TAG_DONE, 0UL }
    };

    /*
     * IE pixel format: R8G8B8A8 in memory (R at byte 0).  These shift
     * values MUST match AROS's canonical RGBA32 entry in
     * rom/hidds/gfx/stdpixfmts_be.h (mask 0xFF000000 -> shift 0):
     *
     *   Red   mask 0xFF000000  shift 0
     *   Green mask 0x00FF0000  shift 8
     *   Blue  mask 0x0000FF00  shift 16
     *   Alpha mask 0x000000FF  shift 24
     *
     * AROS MapColor positions each component by its shift; using the
     * reversed shifts (24/16/8/0) masks the red and green contributions
     * to zero, producing single-channel pens (the "everything is one
     * colour" bug).  MapColor therefore yields 0xRRGGBBAA, which a native
     * M68K (big-endian) store lands as VRAM bytes [R,G,B,A].
     *
     * NOTE: the IE blitter's BLT_COLOR register is consumed little-endian
     * (R in the LOW byte) and stored to VRAM little-endian, so the blitter
     * dispatch path byte-swaps this value via ie_blt_color(); the warp and
     * CPU-direct paths take the native value unchanged.
     */
    struct TagItem pftags_32bpp[] = {
        { aHidd_PixFmt_RedShift,        0       },
        { aHidd_PixFmt_GreenShift,      8       },
        { aHidd_PixFmt_BlueShift,       16      },
        { aHidd_PixFmt_AlphaShift,      24      },
        { aHidd_PixFmt_RedMask,         0xFF000000 },
        { aHidd_PixFmt_GreenMask,       0x00FF0000 },
        { aHidd_PixFmt_BlueMask,        0x0000FF00 },
        { aHidd_PixFmt_AlphaMask,       0x000000FF },
        { aHidd_PixFmt_ColorModel,      vHidd_ColorModel_TrueColor },
        { aHidd_PixFmt_Depth,           32      },
        { aHidd_PixFmt_BytesPerPixel,   4       },
        { aHidd_PixFmt_BitsPerPixel,    32      },
        { aHidd_PixFmt_StdPixFmt,       vHidd_StdPixFmt_RGBA32 },
        { aHidd_PixFmt_BitMapType,      vHidd_BitMapType_Chunky },
        { TAG_DONE, 0UL }
    };

    struct TagItem pftags_8bpp[] = {
        { aHidd_PixFmt_RedShift,        0       },
        { aHidd_PixFmt_GreenShift,      0       },
        { aHidd_PixFmt_BlueShift,       0       },
        { aHidd_PixFmt_AlphaShift,      0       },
        { aHidd_PixFmt_RedMask,         0x00FF0000 },
        { aHidd_PixFmt_GreenMask,       0x0000FF00 },
        { aHidd_PixFmt_BlueMask,        0x000000FF },
        { aHidd_PixFmt_AlphaMask,       0x00000000 },
        { aHidd_PixFmt_ColorModel,      vHidd_ColorModel_Palette },
        { aHidd_PixFmt_CLUTMask,        0xFF    },
        { aHidd_PixFmt_CLUTShift,       0       },
        { aHidd_PixFmt_Depth,           8       },
        { aHidd_PixFmt_BytesPerPixel,   1       },
        { aHidd_PixFmt_BitsPerPixel,    8       },
        { aHidd_PixFmt_StdPixFmt,       vHidd_StdPixFmt_LUT8 },
        { aHidd_PixFmt_BitMapType,      vHidd_BitMapType_Chunky },
        { TAG_DONE, 0UL }
    };

    struct TagItem modetags[] = {
        { aHidd_Gfx_PixFmtTags, (IPTR)pftags_8bpp   },
        { aHidd_Gfx_PixFmtTags, (IPTR)pftags_32bpp  },
        { TAG_MORE,             (IPTR)syncs },
        { TAG_DONE, 0UL }
    };

    struct TagItem ienewtags[] =
    {
        { aHidd_Gfx_ModeTags    , (IPTR)modetags                    },
        { aHidd_Name            , (IPTR)"IEGfx"                     },
        { aHidd_HardwareName    , (IPTR)"IE VideoChip"              },
        { aHidd_ProducerName    , (IPTR)"Intuition Engine"           },
        { TAG_MORE, (IPTR)msg->attrList }
    };

    struct pRoot_New newmsg;

    /* Singleton — only one instance allowed */
    if (XSD(cl)->iegfxhidd)
        return NULL;

    newmsg.mID = msg->mID;
    newmsg.attrList = ienewtags;
    msg = &newmsg;

    D(bug("[IEGfx] Root::New() called\n"));

    o = (OOP_Object *)OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
    if (o)
    {
        D(bug("[IEGfx] DoSuperMethod() returned %p\n", o));
        XSD(cl)->iegfxhidd = o;
    }

    D(bug("[IEGfx] Root::New() = %p\n", o));

    return o;
}

/*********** Root::Dispose() *********************************/

VOID METHOD(IEGfx, Root, Dispose)
{
    D(bug("[IEGfx] Root::Dispose()\n"));

    IE_VideoEnable(FALSE);
    XSD(cl)->iegfxhidd = NULL;
    DeletePool(XSD(cl)->mempool);
    OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
}

/*********** Root::Get() *************************************/

VOID METHOD(IEGfx, Root, Get)
{
    ULONG idx;
    int found = FALSE;

    if (IS_GFX_ATTR(msg->attrID, idx))
    {
        switch (idx)
        {
            case aoHidd_Gfx_NoFrameBuffer:
                found = TRUE;
                *msg->storage = TRUE;
                break;

            case aoHidd_Gfx_HWSpriteTypes:
                /* IE has no hardware sprite — cursor is software-rendered */
                found = TRUE;
                *msg->storage = 0;
                break;
        }
    }

    if (FALSE == found)
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
}

/*********** GfxHidd::NominalDimensions() ********************/

VOID METHOD(IEGfx, Hidd_Gfx, NominalDimensions)
{
    if (msg->width)
        *(msg->width) = 1920;
    if (msg->height)
        *(msg->height) = 1080;
    if (msg->depth)
        *(msg->depth) = 32;
}

/*********** GfxHidd::CreateObject() *************************/

OOP_Object *METHOD(IEGfx, Hidd_Gfx, CreateObject)
{
    OOP_Object      *object = NULL;

    D(bug("[IEGfx] Hidd_Gfx::CreateObject()\n"));

    if (msg->cl == XSD(cl)->basebm)
    {
        BOOL displayable;
        struct TagItem tags[2] =
        {
            {TAG_IGNORE, 0                  },
            {TAG_MORE  , (IPTR)msg->attrList}
        };
        struct pHidd_Gfx_CreateObject p;

        displayable = GetTagData(aHidd_BitMap_Displayable, FALSE, msg->attrList);

        D(bug("[IEGfx] displayable=%d\n", displayable));

        if (displayable)
        {
            D(bug("[IEGfx] Displayable bitmap — using IE bitmap class.\n"));

            /* Only displayable bitmaps are bitmaps of our class */
            tags[0].ti_Tag  = aHidd_BitMap_ClassPtr;
            tags[0].ti_Data = (IPTR)XSD(cl)->bmclass;
        }
        else
        {
            /*
             * Always use the IE bitmap class for offscreen bitmaps.
             * The IE has no planar display hardware — all bitmaps must be
             * chunky (linear). If we let the base class choose, it may
             * fall back to PBM (planar bitmap) whose PutPixel crashes
             * because it expects allocated bitplanes that don't exist.
             */
            D(bug("[IEGfx] Offscreen bitmap — using IE bitmap class.\n"));

            tags[0].ti_Tag  = aHidd_BitMap_ClassPtr;
            tags[0].ti_Data = (IPTR)XSD(cl)->bmclass;
        }

        p.mID = msg->mID;
        p.cl = msg->cl;
        p.attrList = tags;

        object = (OOP_Object *)OOP_DoSuperMethod(cl, o, (OOP_Msg)&p);
    }
    else
        object = (OOP_Object *)OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);

    D(bug("[IEGfx] CreateObject returns %p\n", object));

    return object;
}

/*********** GfxHidd::CopyBox() ******************************/

void METHOD(IEGfx, Hidd_Gfx, CopyBox)
{
    ULONG mode = GC_DRMD(msg->gc);

    D(bug("[IEGfx] CopyBox(%p->%p, sx:%d,sy:%d, dx:%d,dy:%d, %dx%d, mode=%d)\n",
          msg->src, msg->dest,
          msg->srcX, msg->srcY, msg->destX, msg->destY,
          msg->width, msg->height, mode));

    if (OOP_OCLASS(msg->src) != XSD(cl)->bmclass ||
        OOP_OCLASS(msg->dest) != XSD(cl)->bmclass)
    {
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
        return;
    }

    /* COLMASK guard: partial masks require per-pixel read-modify-write */
    if (GC_COLMASK(msg->gc) != (HIDDT_Pixel)~0)
    {
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
        return;
    }

    {
        struct IEGfxBitmapData *bm_src = OOP_INST_DATA(OOP_OCLASS(msg->src), msg->src);
        struct IEGfxBitmapData *bm_dst = OOP_INST_DATA(OOP_OCLASS(msg->dest), msg->dest);

        /* Both bitmaps must have matching BPP */
        if (bm_src->bytesperpix != bm_dst->bytesperpix)
        {
            OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
            return;
        }

        ULONG src_offset = (ULONG)bm_src->VideoData +
                           msg->srcY * bm_src->bytesperline +
                           msg->srcX * bm_src->bytesperpix;
        ULONG dst_offset = (ULONG)bm_dst->VideoData +
                           msg->destY * bm_dst->bytesperline +
                           msg->destX * bm_dst->bytesperpix;

        {
            ULONG bpp_flag = (bm_src->bytesperpix == 1) ?
                IE_BLT_FLAGS_BPP_CLUT8 : IE_BLT_FLAGS_BPP_RGBA32;
            ULONG flags = IE_BLT_MAKE_FLAGS(bpp_flag, mode);

            IE_BlitCopyEx(src_offset, dst_offset,
                          msg->width, msg->height,
                          bm_src->bytesperline, bm_dst->bytesperline,
                          flags);
        }
    }
}

/*********** GfxHidd::CopyBoxMasked() *************************/

BOOL METHOD(IEGfx, Hidd_Gfx, CopyBoxMasked)
{
    D(bug("[IEGfx] CopyBoxMasked(%p->%p, sx:%d,sy:%d, dx:%d,dy:%d, %dx%d)\n",
          msg->src, msg->dest,
          msg->srcX, msg->srcY, msg->destX, msg->destY,
          msg->width, msg->height));

    /* Hardware masked blit when both bitmaps are ours, RGBA32, mask
     * given and the GC asks for a plain Copy. Other draw modes (Or,
     * AndInverted, ...) have masked semantics the blitter does not
     * implement, so they take the superclass path. */
    if (OOP_OCLASS(msg->src) == XSD(cl)->bmclass &&
        OOP_OCLASS(msg->dest) == XSD(cl)->bmclass &&
        msg->mask != NULL && msg->width > 0 && msg->height > 0 &&
        (!msg->gc || GC_DRMD(msg->gc) == vHidd_GC_DrawMode_Copy))
    {
        struct IEGfxBitmapData *bm_src = OOP_INST_DATA(OOP_OCLASS(msg->src), msg->src);
        struct IEGfxBitmapData *bm_dst = OOP_INST_DATA(OOP_OCLASS(msg->dest), msg->dest);

        if (bm_src->bytesperpix == 4 && bm_dst->bytesperpix == 4)
        {
            ULONG src_off = (ULONG)bm_src->VideoData +
                            msg->srcY * bm_src->bytesperline +
                            msg->srcX * bm_src->bytesperpix;
            ULONG dst_off = (ULONG)bm_dst->VideoData +
                            msg->destY * bm_dst->bytesperline +
                            msg->destX * bm_dst->bytesperpix;
            /* The mask is a PLANEPTR laid out for the FULL source bitmap
             * width (rounded up to the bitmap alignment), indexed by
             * srcX/srcY; it is not repacked to the copied rectangle.
             * Mirror the superclass mask_bpr computation. */
            IPTR mask_width = 0, mask_align = 0;
            UWORD mask_stride;
            ULONG mask_row;

            OOP_GetAttr(msg->src, aHidd_BitMap_Width, &mask_width);
            OOP_GetAttr(msg->src, aHidd_BitMap_Align, &mask_align);
            if (mask_align == 0)
                mask_align = 16;
            mask_align--;
            mask_stride = (UWORD)(((mask_width + mask_align) & ~mask_align) >> 3);
            mask_row = (ULONG)msg->mask + (ULONG)msg->srcY * mask_stride;

            IE_BlitMaskedCopy(src_off, dst_off,
                              msg->width, msg->height,
                              bm_src->bytesperline, bm_dst->bytesperline,
                              mask_row, mask_stride, (UWORD)msg->srcX,
                              IE_BLT_FLAGS_MASK_MSB |
                              IE_BLT_MAKE_FLAGS(IE_BLT_FLAGS_BPP_RGBA32,
                                                vHidd_GC_DrawMode_Copy));
            return TRUE;
        }
    }

    return (BOOL)OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
}

/*********** GfxHidd::Show() *********************************/

OOP_Object *METHOD(IEGfx, Hidd_Gfx, Show)
{
    struct IEGfx_staticdata *data = XSD(cl);

    D(bug("[IEGfx] Show(0x%p), old visible 0x%p\n", msg->bitMap, data->visible));

    if (msg->bitMap)
    {
        struct IEGfxBitmapData *bmdata = OOP_INST_DATA(data->bmclass, msg->bitMap);

        D(bug("[IEGfx] Showing bitmap at VideoData=%p (%ldx%ld, %d bpp)\n",
              bmdata->VideoData, bmdata->width, bmdata->height, bmdata->bytesperpix));

        /* Swap framebuffer base at vertical blank for a tear-free flip */
        IE_WaitVBlank();
        IE_SetFBBase((ULONG)bmdata->VideoData);

        if (bmdata->CLUT)
        {
            /* CLUT8 bitmap — upload palette and set indexed mode */
            IE_LoadCLUT(bmdata->CLUT, 0, 256);
            IE_SetColorMode(IE_COLORMODE_CLUT8);
        }
        else
        {
            /* RGBA32 bitmap — direct color mode */
            IE_SetColorMode(IE_COLORMODE_RGBA32);
        }

        /* Map bitmap dimensions to the correct VideoChip mode */
        {
            UWORD ie_mode = IE_MODE_640x480;
            if (bmdata->width >= 1920 && bmdata->height >= 1080)
                ie_mode = IE_MODE_1920x1080;
            else if (bmdata->width >= 1280 && bmdata->height >= 960)
                ie_mode = IE_MODE_1280x960;
            else if (bmdata->width >= 1024 && bmdata->height >= 768)
                ie_mode = IE_MODE_1024x768;
            else if (bmdata->width >= 800 && bmdata->height >= 600)
                ie_mode = IE_MODE_800x600;
            IE_VideoSetMode(ie_mode);
        }
        IE_VideoEnable(TRUE);
    }
    else
    {
        D(bug("[IEGfx] No bitmap — disabling video.\n"));
        IE_VideoEnable(FALSE);
    }

    data->visible = msg->bitMap;

    D(bug("[IEGfx] Show() done\n"));

    return msg->bitMap;
}

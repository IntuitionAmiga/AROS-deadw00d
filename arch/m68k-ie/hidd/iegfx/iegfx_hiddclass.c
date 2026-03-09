/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Intuition Engine Graphics HIDD class.
          Multi-resolution (640x480, 800x600, 1024x768, 1280x960)
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
#include <libraries/iewarp.h>

#include "iegfx_hidd.h"
#include "iegfx_bitmap.h"
#include "iegfx_hw.h"

/* Coprocessor warp dispatch threshold (bytes) */
#define WARP_THRESHOLD 4096

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
     * IE supports four display modes with standard VESA timings.
     * The pixel clocks are notional (IE renders at host framerate) but
     * correct timings are provided for AROS mode enumeration / ScreenMode prefs.
     */
    MAKE_SYNC(640x480,   25175,  640,  656,  752,  800,  480, 490, 492, 525, 0, "IE:640x480");
    MAKE_SYNC(800x600,   40000,  800,  840,  968, 1056,  600, 601, 605, 628, 0, "IE:800x600");
    MAKE_SYNC(1024x768,  65000, 1024, 1048, 1184, 1344,  768, 771, 777, 806, 0, "IE:1024x768");
    MAKE_SYNC(1280x960, 108000, 1280, 1376, 1488, 1800,  960, 961, 964,1000, 0, "IE:1280x960");

    struct TagItem syncs[] = {
        { aHidd_Gfx_SyncTags,       (IPTR)sync_640x480  },
        { aHidd_Gfx_SyncTags,       (IPTR)sync_800x600  },
        { aHidd_Gfx_SyncTags,       (IPTR)sync_1024x768 },
        { aHidd_Gfx_SyncTags,       (IPTR)sync_1280x960 },
        { TAG_DONE, 0UL }
    };

    /*
     * IE pixel format: RGBA byte order in memory (R at byte 0).
     * The IE bus is little-endian; M68K Write32 byte-swaps to LE.
     * With RGBA32, M68K pixel value (R<<24)|(G<<16)|(B<<8)|A
     * byte-swaps to LE memory: byte[0]=R, byte[1]=G, byte[2]=B, byte[3]=A
     * which matches the VideoChip's expected RGBA byte layout.
     */
    struct TagItem pftags_32bpp[] = {
        { aHidd_PixFmt_RedShift,        24      },
        { aHidd_PixFmt_GreenShift,      16      },
        { aHidd_PixFmt_BlueShift,       8       },
        { aHidd_PixFmt_AlphaShift,      0       },
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

        /* Try coprocessor for large Copy-mode blits */
        if (mode == vHidd_GC_DrawMode_Copy)
        {
            ULONG size = (ULONG)msg->width * (ULONG)msg->height *
                         (ULONG)bm_src->bytesperpix;
            if (size >= WARP_THRESHOLD)
            {
                ULONG widthBytes = (ULONG)msg->width * (ULONG)bm_src->bytesperpix;

                ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
                ie_write32(IE_COPROC_OP, WARP_OP_BLIT_COPY);
                ie_write32(IE_COPROC_REQ_PTR, src_offset);
                ie_write32(IE_COPROC_REQ_LEN,
                           widthBytes | ((ULONG)msg->height << 16));
                ie_write32(IE_COPROC_RESP_PTR, dst_offset);
                ie_write32(IE_COPROC_RESP_CAP,
                           (ULONG)bm_src->bytesperline |
                           ((ULONG)bm_dst->bytesperline << 16));
                ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

                if (ie_read32(IE_COPROC_CMD_STATUS) == 0)
                {
                    ULONG ticket = ie_read32(IE_COPROC_TICKET);
                    ie_write32(IE_COPROC_TICKET, ticket);
                    ie_write32(IE_COPROC_TIMEOUT, 5000);
                    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_WAIT_CMD);

                    if (ie_read32(IE_COPROC_TICKET_STATUS) == IE_COPROC_ST_OK)
                        return;
                }
                /* Enqueue or completion failed — fall through to blitter */
            }
        }

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

    /* Only accelerate when both bitmaps are our class */
    if (OOP_OCLASS(msg->src) != XSD(cl)->bmclass ||
        OOP_OCLASS(msg->dest) != XSD(cl)->bmclass)
    {
        return (BOOL)OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
    }

    {
        struct IEGfxBitmapData *bm_src = OOP_INST_DATA(OOP_OCLASS(msg->src), msg->src);
        struct IEGfxBitmapData *bm_dst = OOP_INST_DATA(OOP_OCLASS(msg->dest), msg->dest);

        /* Both RGBA32, mask present, large enough for coprocessor */
        if (bm_src->bytesperpix == 4 && bm_dst->bytesperpix == 4 &&
            msg->mask != NULL)
        {
            ULONG size = (ULONG)msg->width * (ULONG)msg->height * 4;

            if (size >= WARP_THRESHOLD)
            {
                /*
                 * Expand 1-bit PLANEPTR mask to 8-bit byte mask.
                 * AROS mask: 1 bit per pixel, MSB first, word-aligned rows.
                 * Worker expects: 1 byte per pixel (0x00 or 0xFF).
                 */
                ULONG maskBytes = (ULONG)msg->width * (ULONG)msg->height;
                UBYTE *byteMask = AllocMem(maskBytes, MEMF_ANY);

                if (byteMask)
                {
                    UWORD maskStride = ((msg->width + 15) >> 4) << 1;
                    UBYTE *mp = (UBYTE *)msg->mask;
                    UBYTE *bp = byteMask;
                    UWORD row, col;

                    for (row = 0; row < msg->height; row++)
                    {
                        UBYTE *rowMask = mp + row * maskStride;
                        for (col = 0; col < msg->width; col++)
                        {
                            *bp++ = (rowMask[col >> 3] & (0x80 >> (col & 7)))
                                    ? 0xFF : 0x00;
                        }
                    }

                    {
                        ULONG src_off = (ULONG)bm_src->VideoData +
                                        msg->srcY * bm_src->bytesperline +
                                        msg->srcX * bm_src->bytesperpix;
                        ULONG dst_off = (ULONG)bm_dst->VideoData +
                                        msg->destY * bm_dst->bytesperline +
                                        msg->destX * bm_dst->bytesperpix;

                        ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
                        ie_write32(IE_COPROC_OP, WARP_OP_BLIT_MASK);
                        ie_write32(IE_COPROC_REQ_PTR, src_off);
                        ie_write32(IE_COPROC_REQ_LEN,
                                   ((ULONG)msg->width * 4) |
                                   ((ULONG)msg->height << 16));
                        ie_write32(IE_COPROC_RESP_PTR, dst_off);
                        ie_write32(IE_COPROC_RESP_CAP,
                                   (ULONG)bm_src->bytesperline |
                                   ((ULONG)bm_dst->bytesperline << 16));
                        ie_write32(IE_COPROC_TIMEOUT, (ULONG)byteMask);
                        ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

                        if (ie_read32(IE_COPROC_CMD_STATUS) == 0)
                        {
                            ULONG ticket = ie_read32(IE_COPROC_TICKET);
                            ie_write32(IE_COPROC_TICKET, ticket);
                            ie_write32(IE_COPROC_TIMEOUT, 5000);
                            ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_WAIT_CMD);

                            if (ie_read32(IE_COPROC_TICKET_STATUS) ==
                                IE_COPROC_ST_OK)
                            {
                                FreeMem(byteMask, maskBytes);
                                return TRUE;
                            }
                        }
                    }

                    FreeMem(byteMask, maskBytes);
                }
                /* Allocation or dispatch failed — fall through */
            }
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

        /* Set framebuffer base to this bitmap's VRAM address */
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
            if (bmdata->width >= 1280 && bmdata->height >= 960)
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

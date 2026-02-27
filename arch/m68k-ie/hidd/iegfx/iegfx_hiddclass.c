/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Intuition Engine Graphics HIDD class.
          Single mode (640x480 RGBA32), hardware blitter, no sprite.
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
     * IE supports one display mode: 640x480 @ 60Hz with standard VGA timing.
     * The pixel clock is notional (IE renders at host framerate) but we
     * provide correct VGA timings for AROS mode enumeration.
     */
    MAKE_SYNC(640x480, 25175, 640, 656, 752, 800, 480, 490, 492, 525, 0, "IE:640x480");

    struct TagItem syncs[] = {
        { aHidd_Gfx_SyncTags,       (IPTR)sync_640x480 },
        { TAG_DONE, 0UL }
    };

    /*
     * IE pixel format: RGBA byte order in memory (R at byte 0).
     * On big-endian M68K, a 32-bit read gives 0xRRGGBBAA:
     *   R at bits 31:24, G at 23:16, B at 15:8, A at 7:0
     * AROS StdPixFmt names go LSB→MSB: ABGR32.
     * Shifts follow byte-position-in-memory convention (byte_index * 8).
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
        { aHidd_PixFmt_StdPixFmt,       vHidd_StdPixFmt_ABGR32 },
        { aHidd_PixFmt_BitMapType,      vHidd_BitMapType_Chunky },
        { TAG_DONE, 0UL }
    };

    struct TagItem modetags[] = {
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
            /* Non-displayable friends of our bitmaps are our bitmaps too */
            OOP_Object *friend = (OOP_Object *)GetTagData(aHidd_BitMap_Friend, 0, msg->attrList);

            D(bug("[IEGfx] Not displayable. Friend=%p.\n", friend));

            if (friend && (OOP_OCLASS(friend) == XSD(cl)->bmclass))
            {
                D(bug("[IEGfx] Friend is IE bitmap, using our class\n"));
                tags[0].ti_Tag  = aHidd_BitMap_ClassPtr;
                tags[0].ti_Data = (IPTR)XSD(cl)->bmclass;
            }
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

    D(bug("[IEGfx] CopyBox(%p->%p, sx:%d,sy:%d, dx:%d,dy:%d, %dx%d)\n",
          msg->src, msg->dest,
          msg->srcX, msg->srcY, msg->destX, msg->destY,
          msg->width, msg->height));

    if (OOP_OCLASS(msg->src) != XSD(cl)->bmclass ||
        OOP_OCLASS(msg->dest) != XSD(cl)->bmclass)
    {
        /* One or both bitmaps are not IE bitmaps — use software fallback */
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
        return;
    }

    if (mode != vHidd_GC_DrawMode_Copy)
    {
        /* Non-copy draw modes use software path */
        OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
        return;
    }

    {
        struct IEGfxBitmapData *bm_src = OOP_INST_DATA(OOP_OCLASS(msg->src), msg->src);
        struct IEGfxBitmapData *bm_dst = OOP_INST_DATA(OOP_OCLASS(msg->dest), msg->dest);

        ULONG src_offset = (ULONG)bm_src->VideoData +
                           msg->srcY * bm_src->bytesperline +
                           msg->srcX * bm_src->bytesperpix;
        ULONG dst_offset = (ULONG)bm_dst->VideoData +
                           msg->destY * bm_dst->bytesperline +
                           msg->destX * bm_dst->bytesperpix;

        IE_BlitCopy(src_offset, dst_offset,
                    msg->width, msg->height,
                    bm_src->bytesperline, bm_dst->bytesperline);
    }
}

/*********** GfxHidd::Show() *********************************/

OOP_Object *METHOD(IEGfx, Hidd_Gfx, Show)
{
    struct IEGfx_staticdata *data = XSD(cl);

    D(bug("[IEGfx] Show(0x%p), old visible 0x%p\n", msg->bitMap, data->visible));

    if (msg->bitMap)
    {
        struct IEGfxBitmapData *bmdata = OOP_INST_DATA(data->bmclass, msg->bitMap);

        D(bug("[IEGfx] Showing bitmap at VideoData=%p (%ldx%ld)\n",
              bmdata->VideoData, bmdata->width, bmdata->height));

        /* Set 640x480 mode and enable the VideoChip */
        IE_VideoSetMode(IE_MODE_640x480);
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

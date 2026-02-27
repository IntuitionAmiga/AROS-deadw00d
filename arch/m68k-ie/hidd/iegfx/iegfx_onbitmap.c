/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Bitmap class for IE Gfx HIDD.
          Displayable bitmaps are allocated in VRAM (0x100000+).
          IE only supports RGBA32 — no palette or CLUT.
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

#include "iegfx_bitmap.h"
#include "iegfx_hidd.h"

#include LC_LIBDEFS_FILE

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
        memset(data->VideoData, 0, data->bytesperline * data->height);

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
    D(bug("[IEBitMap] Dispose(0x%p)\n", o));

    /*
     * VRAM-allocated buffers use a bump allocator — no individual free.
     * Pool-allocated buffers would need FreeVecPooled, but we don't track
     * which allocator was used. For now, this is acceptable — VRAM is
     * reclaimed on driver dispose (which resets the bump pointer).
     */

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

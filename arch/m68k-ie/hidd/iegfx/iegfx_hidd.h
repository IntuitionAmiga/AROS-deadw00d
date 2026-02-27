/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Intuition Engine Graphics HIDD header.
*/

#ifndef IEGFX_HIDD_H
#define IEGFX_HIDD_H

#include <exec/memory.h>
#include <exec/types.h>
#include <oop/oop.h>
#include <hidd/gfx.h>

#define ATTRBASES_NUM 7

#define CLID_Hidd_Gfx_IE "hidd.gfx.ie"

struct IEGfx_data
{
    int __dummy__;
};

struct IEGfx_staticdata
{
    OOP_Class *     basebm;
    OOP_Class *     iegfxclass;
    OOP_Class *     bmclass;
    OOP_Object *    iegfxhidd;
    OOP_Object *    visible;        /* Currently visible bitmap */
    OOP_AttrBase    attrBases[ATTRBASES_NUM];

    APTR            mempool;

    /* VRAM allocator: bump pointer for framebuffer allocation */
    UBYTE *         vram_next;      /* Next free VRAM address */
    UBYTE *         vram_end;       /* End of VRAM region */
};

struct IEGfxBase
{
    struct Library library;
    struct IEGfx_staticdata vsd;
};

#define XSD(cl)	(&((struct IEGfxBase *)cl->UserData)->vsd)

#undef HiddChunkyBMAttrBase
#undef HiddBitMapAttrBase
#undef HiddGfxAttrBase
#undef HiddPixFmtAttrBase
#undef HiddSyncAttrBase
#undef HiddAttrBase
#undef HiddColorMapAttrBase

/* These must stay in the same order as interfaces[] array in iegfx_init.c */
#define HiddChunkyBMAttrBase  XSD(cl)->attrBases[0]
#define HiddBitMapAttrBase    XSD(cl)->attrBases[1]
#define HiddGfxAttrBase       XSD(cl)->attrBases[2]
#define HiddPixFmtAttrBase    XSD(cl)->attrBases[3]
#define HiddSyncAttrBase      XSD(cl)->attrBases[4]
#define HiddAttrBase          XSD(cl)->attrBases[5]
#define HiddColorMapAttrBase  XSD(cl)->attrBases[6]

#define METHOD(base, id, name) \
  base ## __ ## id ## __ ## name (OOP_Class *cl, OOP_Object *o, struct p ## id ## _ ## name *msg)

#define METHOD_NAME(base, id, name) \
  base ## __ ## id ## __ ## name

#define METHOD_NAME_S(base, id, name) \
  # base "__" # id "__" # name

#endif /* IEGFX_HIDD_H */

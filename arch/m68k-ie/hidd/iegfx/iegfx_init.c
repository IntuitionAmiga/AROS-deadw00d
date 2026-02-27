/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Intuition Engine Graphics HIDD initialisation.
*/

#define __OOP_NOATTRBASES__

#define DEBUG 0
#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/oop.h>
#include <exec/types.h>
#include <exec/lists.h>
#include <graphics/driver.h>
#include <graphics/gfxbase.h>
#include <hidd/gfx.h>
#include <oop/oop.h>
#include <utility/utility.h>
#include <aros/symbolsets.h>

#include "iegfx_hidd.h"
#include "iegfx_hw.h"

#include LC_LIBDEFS_FILE

static void FreeAttrBases(const STRPTR *iftable, OOP_AttrBase *bases, ULONG num)
{
    ULONG i;

    for (i = 0; i < num; i++)
    {
        if (bases[i])
            OOP_ReleaseAttrBase(iftable[i]);
    }
}

static BOOL GetAttrBases(const STRPTR *iftable, OOP_AttrBase *bases, ULONG num)
{
    ULONG i;

    for (i = 0; i < num; i++)
    {
        bases[i] = OOP_ObtainAttrBase(iftable[i]);
        if (!bases[i])
        {
            FreeAttrBases(iftable, bases, i);
            return FALSE;
        }
    }

    return TRUE;
}

/* These must stay in the same order as attrBases[] entries in iegfx_hidd.h */
static const STRPTR interfaces[ATTRBASES_NUM] =
{
    IID_Hidd_ChunkyBM,
    IID_Hidd_BitMap,
    IID_Hidd_Gfx,
    IID_Hidd_PixFmt,
    IID_Hidd_Sync,
    IID_Hidd,
    IID_Hidd_ColorMap
};

static int IEGfx_Init(LIBBASETYPEPTR LIBBASE)
{
    struct IEGfx_staticdata *xsd = &LIBBASE->vsd;
    struct GfxBase *GfxBase;
    int res = FALSE;

    D(bug("[IEGfx] IEGfx_Init() called\n"));

    if (IE_VideoInit() == FALSE)
        return FALSE;

    xsd->mempool = CreatePool(MEMF_FAST | MEMF_CLEAR, 32768, 16384);
    if (xsd->mempool == NULL)
        return FALSE;

    xsd->visible = NULL;

    /* Initialise VRAM bump allocator */
    xsd->vram_next = (UBYTE *)IE_VRAM_BASE;
    xsd->vram_end = (UBYTE *)(IE_VRAM_BASE + IE_VRAM_SIZE);

    /* Obtain AttrBases */
    if (!GetAttrBases(interfaces, xsd->attrBases, ATTRBASES_NUM))
        return FALSE;

    D(bug("[IEGfx] IE VideoChip detected. Installing the driver\n"));

    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 41);
    if (!GfxBase)
    {
        D(bug("[IEGfx] Failed to open graphics.library!\n"));
        return FALSE;
    }

    LIBBASE->vsd.basebm = OOP_FindClass(CLID_Hidd_BitMap);

    /* We use ourselves, and no one else does */
    LIBBASE->library.lib_OpenCnt = 1;
    res = TRUE;

    CloseLibrary(&GfxBase->LibNode);

    return res;
}

ADD2LIBS((STRPTR)"gfx.hidd", 0, static struct Library *, __gfxbase);
ADD2INITLIB(IEGfx_Init, 0)

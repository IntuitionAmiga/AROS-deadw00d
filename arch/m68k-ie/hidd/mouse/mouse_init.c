/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE Mouse HIDD initialisation.
*/

#define DEBUG 0
#include <aros/debug.h>

#include <aros/symbolsets.h>
#include <proto/oop.h>

#include <hidd/hidd.h>
#include <hidd/input.h>
#include <hidd/mouse.h>

#include "mouse.h"

#undef MSD
#define MSD(cl)         msd

static int IEMouse_Init(struct mousebase *LIBBASE)
{
    struct mouse_staticdata *msd = &LIBBASE->msd;
    struct OOP_ABDescr attrbases[] =
    {
        { IID_Hidd, &HiddAttrBase },
        { IID_Hidd_Input, &HiddInputAB },
        { IID_Hidd_Mouse, &HiddMouseAB },
        { NULL          , NULL              }
    };
    OOP_Object *ms;
    OOP_Object *drv = NULL;

    EnterFunc(bug("IEMouse_Init\n"));

    InitSemaphore(&msd->sema);

    ms = OOP_NewObject(NULL, CLID_Hidd_Mouse, NULL);
    if (ms) {
        if (OOP_ObtainAttrBases(attrbases)) {
            OOP_MethodID HiddInputBase = OOP_GetMethodID(IID_Hidd_Input, 0);
            drv = HIDD_Input_AddHardwareDriver(ms, LIBBASE->msd.mouseclass, NULL);
        }
        OOP_DisposeObject(ms);
    }

    if (!drv)
        return FALSE;

    LIBBASE->library.lib_OpenCnt = 1;

    ReturnInt("IEMouse_Init", int, TRUE);
}

static int IEMouse_Expunge(struct mousebase *LIBBASE)
{
    struct mouse_staticdata *msd = &LIBBASE->msd;
    struct OOP_ABDescr attrbases[] =
    {
        { IID_Hidd, &HiddAttrBase },
        { IID_Hidd_Input, &HiddInputAB },
        { IID_Hidd_Mouse, &HiddMouseAB },
        { NULL          , NULL              }
    };

    EnterFunc(bug("IEMouse_Expunge\n"));

    OOP_ReleaseAttrBases(attrbases);

    ReturnInt("IEMouse_Expunge", int, TRUE);
}

ADD2INITLIB(IEMouse_Init, 0)
ADD2EXPUNGELIB(IEMouse_Expunge, 0)

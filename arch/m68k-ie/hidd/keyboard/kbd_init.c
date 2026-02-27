/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE Keyboard HIDD initialisation.
*/

#define DEBUG 0
#include <aros/debug.h>

#include <aros/symbolsets.h>
#include <proto/oop.h>

#include <hidd/hidd.h>
#include <hidd/input.h>

#include "kbd.h"

#undef XSD
#define XSD(cl)         ksd

static int IEKbd_Init(struct kbdbase *LIBBASE)
{
    struct kbd_staticdata *ksd = &LIBBASE->ksd;
    struct OOP_ABDescr attrbases[] =
    {
        {IID_Hidd,          &HiddAttrBase   },
        {IID_Hidd_Input,    &HiddInputAB    },
        {NULL,              NULL            }
    };
    OOP_Object *kbd;
    OOP_Object *drv = NULL;

    EnterFunc(bug("IEKbd_Init\n"));

    InitSemaphore(&ksd->sema);

    kbd = OOP_NewObject(NULL, CLID_Hidd_Kbd, NULL);
    if (kbd) {
        if (OOP_ObtainAttrBases(attrbases)) {
            OOP_MethodID HiddInputBase = OOP_GetMethodID(IID_Hidd_Input, 0);
            drv = HIDD_Input_AddHardwareDriver(kbd, LIBBASE->ksd.kbdclass, NULL);
        }
        OOP_DisposeObject(kbd);
    }

    if (!drv)
        return FALSE;

    LIBBASE->library.lib_OpenCnt = 1;

    ReturnInt("IEKbd_Init", int, TRUE);
}

static int IEKbd_Expunge(struct kbdbase *LIBBASE)
{
    struct kbd_staticdata *ksd = &LIBBASE->ksd;
    struct OOP_ABDescr attrbases[] =
    {
        {IID_Hidd_Input   , &HiddInputAB   },
        {NULL             , NULL            }
    };

    EnterFunc(bug("IEKbd_Expunge\n"));

    OOP_ReleaseAttrBases(attrbases);

    ReturnInt("IEKbd_Expunge", int, TRUE);
}

ADD2INITLIB(IEKbd_Init, 0)
ADD2EXPUNGELIB(IEKbd_Expunge, 0)

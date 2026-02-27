/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE Keyboard HIDD header.
*/

#ifndef HIDD_KBD_H
#define HIDD_KBD_H

#ifndef EXEC_LIBRARIES_H
#   include <exec/libraries.h>
#endif

#ifndef OOP_OOP_H
#   include <oop/oop.h>
#endif

#ifndef EXEC_SEMAPHORES_H
#   include <exec/semaphores.h>
#endif

#ifndef EXEC_INTERRUPTS_H
#   include <exec/interrupts.h>
#endif

#include <dos/bptr.h>

#include <hidd/input.h>
#include <hidd/keyboard.h>

/* IDs */
#define IID_Hidd_HwKbd		"hidd.kbd.hw"
#define CLID_Hidd_HwKbd		"hidd.kbd.hw"

/* Methods */
enum
{
    moHidd_Kbd_HandleEvent
};

struct pHidd_Kbd_HandleEvent
{
    OOP_MethodID 		mID;
    ULONG 			event;
};

struct kbd_staticdata
{
    struct SignalSemaphore sema;
    struct Interrupt kbint;         /* VBL interrupt for keyboard polling */

    OOP_Class *kbdclass;
    OOP_Object *kbdhidd;

    OOP_AttrBase hiddAB;
    OOP_AttrBase hiddInputAB;

    OOP_MethodID hiddKbdBase;

    BPTR                cs_SegList;
    struct Library     *cs_OOPBase;
};

struct kbdbase
{
    struct Library library;
    struct kbd_staticdata ksd;
};

struct kbd_data
{
    InputIrqCallBack_t kbd_callback;
    APTR    callbackdata;
};

#define XSD(cl) 	(&((struct kbdbase *)cl->UserData)->ksd)

#undef HiddAttrBase
#define HiddAttrBase	(XSD(cl)->hiddAB)

#undef HiddInputAB
#define HiddInputAB	(XSD(cl)->hiddInputAB)

#undef HiddKbdBase
#define HiddKbdBase	(XSD(cl)->hiddKbdBase)

#define OOPBase		(XSD(cl)->cs_OOPBase)

#endif /* HIDD_KBD_H */

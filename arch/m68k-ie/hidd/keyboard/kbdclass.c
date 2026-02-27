/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE Keyboard HIDD — polls IE scancode registers during VBL.
          No CIA handshake needed — IE provides ready-to-use Amiga rawkeys.
*/

#define DEBUG 0
#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/oop.h>
#include <oop/oop.h>

#include <exec/alerts.h>
#include <exec/memory.h>

#include <hidd/hidd.h>
#include <hidd/input.h>

#include <aros/system.h>
#include <aros/symbolsets.h>

#include <hardware/intbits.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>

#include "kbd.h"
#include "ie_hwreg.h"

/*
 * VBL interrupt handler — polls IE keyboard register.
 *
 * The IE scancode register (IE_SCAN_CODE) provides Amiga-compatible rawkeys:
 *   0x00-0x7F = key down
 *   0x80-0xFF = key up
 *
 * IE_SCAN_STATUS bit 0 indicates a scancode is available.
 * Reading IE_SCAN_CODE dequeues the scancode.
 * Multiple keys per VBL are drained in a loop.
 */
static AROS_INTH1(keyboard_vblank, struct kbd_data *, kbddata)
{
    AROS_INTFUNC_INIT

    struct pHidd_Kbd_Event kEvt;

    /* Drain all queued scancodes this VBL */
    while (ie_read32(IE_SCAN_STATUS) & 1)
    {
        ULONG raw = ie_read32(IE_SCAN_CODE);
        UBYTE keycode = (UBYTE)(raw & 0xFF);

        kEvt.flags = 0;
        kEvt.code = keycode;

        /* Caps Lock is a toggle key */
        if ((keycode & ~0x80) == 0x62)
            kEvt.code |= (KBD_KEYTOGGLE << 16);

        D(bug("[kbd:ie] key=%02x\n", keycode));

        kbddata->kbd_callback(kbddata->callbackdata, &kEvt);
    }

    return 0;

    AROS_INTFUNC_EXIT
}

OOP_Object * IEKbd__Root__New(OOP_Class *cl, OOP_Object *o, struct pRoot_New *msg)
{
    struct TagItem      kbd_tags[] =
    {
        {aHidd_Name,            (IPTR)"IEKbd"                           },
        {aHidd_HardwareName,    (IPTR)"IE Keyboard Controller"          },
        {TAG_MORE,              (IPTR)msg->attrList                     }
    };
    struct pRoot_New    new_msg =
    {
        .mID = msg->mID,
        .attrList = kbd_tags
    };
    struct TagItem      *tag, *tstate;
    InputIrqCallBack_t    callback = NULL;
    BOOL                has_kbd_hidd = FALSE;
    struct Library      *UtilityBase = TaggedOpenLibrary(TAGGEDOPEN_UTILITY);

    EnterFunc(bug("[kbd:ie]:New()\n"));

    if (!UtilityBase)
        ReturnPtr("[kbd:ie]:New", OOP_Object *, NULL);

    ObtainSemaphoreShared(&XSD(cl)->sema);
    if (XSD(cl)->kbdhidd)
        has_kbd_hidd = TRUE;
    ReleaseSemaphore(&XSD(cl)->sema);

    if (has_kbd_hidd) {
        CloseLibrary(UtilityBase);
        ReturnPtr("[kbd:ie]:New", OOP_Object *, NULL);
    }

    tstate = msg->attrList;

    while ((tag = NextTagItem(&tstate))) {
        ULONG idx;

        if (IS_HIDDINPUT_ATTR(tag->ti_Tag, idx)) {
            switch (idx) {
                case aoHidd_Input_IrqHandler:
                    callback = (APTR)tag->ti_Data;
                    D(bug("[kbd:ie] callback @ 0x%p\n", callback));
                    break;
            }
        }
    }
    CloseLibrary(UtilityBase);

    if (NULL == callback)
        ReturnPtr("[kbd:ie]:New", OOP_Object *, NULL);

    o = (OOP_Object *)OOP_DoSuperMethod(cl, o, &new_msg.mID);

    if (o) {
        struct Interrupt *inter = &XSD(cl)->kbint;
        struct kbd_data *data = OOP_INST_DATA(cl, o);

        data->kbd_callback = callback;
        OOP_GetAttr(o, aHidd_Input_IrqHandlerData, (IPTR *)&data->callbackdata);

        /* Register VBL interrupt to poll keyboard */
        inter->is_Node.ln_Pri = 0;
        inter->is_Node.ln_Type = NT_INTERRUPT;
        inter->is_Node.ln_Name = "IE Keyboard";
        inter->is_Code = (APTR)keyboard_vblank;
        inter->is_Data = data;

        AddIntServer(INTB_VERTB, inter);

        D(bug("[kbd:ie] Keyboard VBL handler installed\n"));
    }

    ReturnPtr("[kbd:ie]:New", OOP_Object *, o);
}

VOID IEKbd__Root__Dispose(OOP_Class *cl, OOP_Object *o, OOP_Msg msg)
{
    ObtainSemaphore(&XSD(cl)->sema);
    RemIntServer(INTB_VERTB, &XSD(cl)->kbint);
    ReleaseSemaphore(&XSD(cl)->sema);
    OOP_DoSuperMethod(cl, o, msg);
}

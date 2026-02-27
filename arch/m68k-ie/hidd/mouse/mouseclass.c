/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE Mouse HIDD — polls IE mouse registers during VBL.
          IE provides absolute coordinates; we compute deltas for AROS.
          Button state: bit 0=left, 1=right, 2=middle.
          MOUSE_STATUS bit 0 is clear-on-read (set when mouse moved).
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
#include <hidd/mouse.h>

#include <hardware/intbits.h>
#include <devices/inputevent.h>
#include <string.h>

#include <aros/symbolsets.h>

#include "mouse.h"
#include "ie_hwreg.h"

/*
 * VBL interrupt handler — polls IE mouse registers.
 *
 * IE provides absolute mouse coordinates (IE_MOUSE_X, IE_MOUSE_Y).
 * AROS expects relative delta motion, so we track the last position
 * and compute the difference each frame.
 *
 * IE_MOUSE_BUTTONS: bit 0=left, 1=right, 2=middle (same as our defines).
 * IE_MOUSE_STATUS: bit 0=changed (clear-on-read).
 */
static AROS_INTH1(mouse_vblank, struct mouse_data *, mousedata)
{
    AROS_INTFUNC_INIT

    struct pHidd_Mouse_Event *e = &mousedata->event;
    WORD curX, curY;
    UWORD buttons;

    /* Read current absolute position */
    curX = (WORD)ie_read32(IE_MOUSE_X);
    curY = (WORD)ie_read32(IE_MOUSE_Y);
    buttons = (UWORD)(ie_read32(IE_MOUSE_BUTTONS) & 0x07);

    /* Read and clear the status register */
    ie_read32(IE_MOUSE_STATUS);

    if (mousedata->first) {
        /* First read — initialise last position, no delta */
        mousedata->lastX = curX;
        mousedata->lastY = curY;
        mousedata->first = FALSE;
    }

    /* Compute motion delta */
    e->x = curX - mousedata->lastX;
    e->y = curY - mousedata->lastY;
    mousedata->lastX = curX;
    mousedata->lastY = curY;

    /* Report motion if there's any movement */
    if (e->x || e->y) {
        e->button = vHidd_Mouse_NoButton;
        e->type = vHidd_Mouse_Motion;
        mousedata->mouse_callback(mousedata->callbackdata, e);
    }

    /* Report button changes */
    if (buttons != mousedata->buttons) {
        int i;
        for (i = 0; i < 3; i++) {
            if ((buttons & (1 << i)) != (mousedata->buttons & (1 << i))) {
                e->button = vHidd_Mouse_Button1 + i;
                e->type = (buttons & (1 << i)) ? vHidd_Mouse_Press : vHidd_Mouse_Release;
                mousedata->mouse_callback(mousedata->callbackdata, e);
            }
        }
        mousedata->buttons = buttons;
    }

    return 0;

    AROS_INTFUNC_EXIT
}

/***** Mouse::New()  ***************************************/

OOP_Object * IEMouse__Root__New(OOP_Class *cl, OOP_Object *o, struct pRoot_New *msg)
{
    struct TagItem mouse_tags[] =
    {
        {aHidd_Name        , (IPTR)"IEMouse"                      },
        {aHidd_HardwareName, (IPTR)"IE Mouse Controller"          },
        {TAG_MORE          , (IPTR)msg->attrList                   }
    };
    struct pRoot_New new_msg =
    {
        .mID = msg->mID,
        .attrList = mouse_tags
    };

    BOOL has_mouse_hidd = FALSE;
    struct Library *UtilityBase = TaggedOpenLibrary(TAGGEDOPEN_UTILITY);

    EnterFunc(bug("[mouse:ie]:New()\n"));

    if (!UtilityBase)
        ReturnPtr("[mouse:ie]:New", OOP_Object *, NULL);

    ObtainSemaphoreShared(&MSD(cl)->sema);
    if (MSD(cl)->mousehidd)
        has_mouse_hidd = TRUE;
    ReleaseSemaphore(&MSD(cl)->sema);

    if (has_mouse_hidd) {
        CloseLibrary(UtilityBase);
        ReturnPtr("[mouse:ie]:New", OOP_Object *, NULL);
    }

    o = (OOP_Object *)OOP_DoSuperMethod(cl, o, &new_msg.mID);
    if (o)
    {
        struct mouse_data   *data = OOP_INST_DATA(cl, o);
        struct TagItem      *tag, *tstate;
        struct Interrupt    *inter;

        tstate = msg->attrList;

        while ((tag = NextTagItem(&tstate)))
        {
            ULONG idx;

            if (IS_HIDDINPUT_ATTR(tag->ti_Tag, idx))
            {
                switch (idx)
                {
                    case aoHidd_Input_IrqHandler:
                        data->mouse_callback = (APTR)tag->ti_Data;
                        break;
                }
            }
        }
        OOP_GetAttr(o, aHidd_Input_IrqHandlerData, (IPTR *)&data->callbackdata);

        /* Initialise mouse state */
        data->buttons = 0;
        data->lastX = 0;
        data->lastY = 0;
        data->first = TRUE;

        /* Register VBL interrupt to poll mouse */
        inter = &MSD(cl)->mouseint;
        inter->is_Code         = (APTR)mouse_vblank;
        inter->is_Data         = data;
        inter->is_Node.ln_Name = "IE Mouse VBlank";
        inter->is_Node.ln_Pri  = 10;
        inter->is_Node.ln_Type = NT_INTERRUPT;

        AddIntServer(INTB_VERTB, inter);

        ObtainSemaphore(&MSD(cl)->sema);
        MSD(cl)->mousehidd = o;
        ReleaseSemaphore(&MSD(cl)->sema);

        D(bug("[mouse:ie] Mouse VBL handler installed\n"));
    }

    CloseLibrary(UtilityBase);
    return o;
}

/***** Mouse::Dispose()  ***********************************/

VOID IEMouse__Root__Dispose(OOP_Class *cl, OOP_Object *o, OOP_Msg msg)
{
    ObtainSemaphore(&MSD(cl)->sema);
    MSD(cl)->mousehidd = NULL;
    RemIntServer(INTB_VERTB, &MSD(cl)->mouseint);
    ReleaseSemaphore(&MSD(cl)->sema);

    OOP_DoSuperMethod(cl, o, msg);
}

/***** Mouse::Get()  ***************************************/

VOID IEMouse__Root__Get(OOP_Class *cl, OOP_Object *o, struct pRoot_Get *msg)
{
    struct mouse_data *data = OOP_INST_DATA(cl, o);
    ULONG              idx;

    if (IS_HIDDMOUSE_ATTR(msg->attrID, idx)) {
        switch (idx) {
            case aoHidd_Mouse_State:
                return;

            case aoHidd_Mouse_RelativeCoords:
                /* We convert absolute IE coords to relative deltas */
                *msg->storage = TRUE;
                return;

            case aoHidd_Mouse_Extended:
                *msg->storage = FALSE;
                return;
        }
    }

    OOP_DoSuperMethod(cl, o, (OOP_Msg)msg);
}

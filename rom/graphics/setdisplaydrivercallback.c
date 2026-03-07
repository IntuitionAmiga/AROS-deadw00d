/*
    Copyright (C) 2010, The AROS Development Team. All rights reserved.

    Desc: Set display driver notification callback
*/

#include <aros/debug.h>
#include <graphics/driver.h>
#include <oop/oop.h>
#include <proto/utility.h>

#include "graphics_intern.h"

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH2(void, SetDisplayDriverCallback,

/*  SYNOPSIS */
        AROS_LHA(APTR, callback, A0),
        AROS_LHA(APTR, userdata, A1),

/*  LOCATION */
        struct GfxBase *, GfxBase, 108, Graphics)

/*  FUNCTION
        Specify a display driver notification callback.
        
        The callback function is called using "C" calling convention and its
        declaration should have a form:

        APTR DriverNotify(APTR object, BOOL add, APTR userdata);

        The function will be called upon display driver insertion and removal.
        Upon insertion the parameters will be the following:
          object   - A pointer to a struct MonitorHandle for the new driver
          add      - TRUE, indicates driver insertion
          userdata - User data originally passed to SetDisplayDriverCallback()
        The function should return a pointer to opaque data object which will
        be stored in the display driver handle structure.

        Upon driver removal the parameters mean:
          object   - A pointer to opaque object returned by the callback when
                     the driver was added.
          add      - FALSE, indicates driver removal.
          userdata - User data originally passed to SetDisplayDriverCallback()
        Callback return value is ignored in removal mode.

    INPUTS
        callback - A pointer to a function to call.
        userdata - User-defined data, will be passed to the callback function

    RESULT
        None.

    NOTES
        This function is private to AROS. Do not use it in any end-user software,
        the specification may change at any moment.

    EXAMPLE

    BUGS

    SEE ALSO
        AddDisplayDriverA()

    INTERNALS

    HISTORY

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    APTR (*notify)(APTR, BOOL, APTR) = (APTR (*)(APTR, BOOL, APTR))callback;

    CDD(GfxBase)->DriverNotify = callback;
    CDD(GfxBase)->notify_data  = userdata;

    /*
     * Retroactively notify for any drivers that were registered before
     * the callback was set. This handles the case where a display HIDD
     * (e.g. IEGfx) calls AddDisplayDriverA() during ROM init, before
     * Intuition has called SetDisplayDriverCallback(). Without this,
     * the MonitorClass objects are never created and FindMonitorNode()
     * fails for all mode IDs.
     */
    if (notify)
    {
        struct monitor_driverdata *mdd;

        ObtainSemaphore(&CDD(GfxBase)->displaydb_sem);

        for (mdd = CDD(GfxBase)->monitors; mdd; mdd = mdd->next)
        {
            if (!mdd->userdata)
            {
                mdd->userdata = notify(mdd, TRUE, userdata);
            }
        }

        ReleaseSemaphore(&CDD(GfxBase)->displaydb_sem);
    }

    AROS_LIBFUNC_EXIT
}

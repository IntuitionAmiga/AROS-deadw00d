/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IEWarpMon timer module (based on SysMon timer.c)
*/

#include <devices/timer.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>

#include "iewarpmon_intern.h"

static struct MsgPort *timerport = NULL;
static struct timerequest *timermsg = NULL;
static ULONG SIG_TIMER = 0;

BOOL InitTimer(void)
{
    if ((timerport = CreatePort(0, 0)) == NULL)
        return FALSE;

    if ((timermsg = (struct timerequest *)
         CreateExtIO(timerport, sizeof(struct timerequest))) == NULL)
    {
        DeletePort(timerport);
        timerport = NULL;
        return FALSE;
    }

    if (OpenDevice("timer.device", UNIT_VBLANK,
                   (struct IORequest *)timermsg, 0) != 0)
    {
        DeletePort(timerport);
        timerport = NULL;
        DeleteExtIO((struct IORequest *)timermsg);
        timermsg = NULL;
        return FALSE;
    }

    SIG_TIMER = 1 << timerport->mp_SigBit;
    return TRUE;
}

VOID SignalMeAfter(ULONG msecs)
{
    timermsg->tr_node.io_Command = TR_ADDREQUEST;
    timermsg->tr_time.tv_secs = msecs / 1000;
    timermsg->tr_time.tv_micro = (msecs % 1000) * 1000;
    SendIO((struct IORequest *)timermsg);
}

VOID DeInitTimer(void)
{
    if (timermsg != NULL)
    {
        AbortIO((struct IORequest *)timermsg);
        WaitIO((struct IORequest *)timermsg);
        CloseDevice((struct IORequest *)timermsg);
        DeleteExtIO((struct IORequest *)timermsg);
    }
    if (timerport != NULL)
        DeletePort(timerport);
}

ULONG GetSIG_TIMER(void)
{
    return SIG_TIMER;
}

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: ReadEClock() - read the base frequency of timers.
          IE version: synthetic E-clock from Level 5 interrupt counting.
*/

#include <devices/timer.h>
#include <proto/timer.h>
#include <proto/exec.h>

#include <timer_intern.h>

/* See rom/timer/readeclock.c for documentation */

AROS_LH1(ULONG, ReadEClock,
    AROS_LHA(struct EClockVal *, dest, A0),
    struct TimerBase *, TimerBase, 10, Timer)
{
    AROS_LIBFUNC_INIT

    ULONG old;

    Disable();
    old = dest->ev_lo = TimerBase->tb_eclock.ev_lo;
    dest->ev_hi = TimerBase->tb_eclock.ev_hi;
    Enable();

    /* No sub-tick counter on IE — GetEClock always returns 0 */

    return TimerBase->tb_eclock_rate;

    AROS_LIBFUNC_EXIT
}

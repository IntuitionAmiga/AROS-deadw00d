/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: GetSysTime() - Find out what time it is.
          IE version: no CIA sub-tick counter, uses e-clock overflow tracking.
*/
#include <proto/exec.h>

#define DEBUG 0
#include <aros/debug.h>

#include <timer_intern.h>

#include <devices/timer.h>
#include <proto/timer.h>

/* See rom/timer/getsystime.c for documentation */

AROS_LH1(void, GetSysTime,
    AROS_LHA(struct timeval *, dest, A0),
    struct Device *, TimerBase, 11, Timer)
{
    AROS_LIBFUNC_INIT

    struct TimerBase *timerBase = (struct TimerBase *)TimerBase;
    ULONG tb_eclock_to_usec;

    Disable();
    tb_eclock_to_usec = timerBase->tb_eclock_to_usec;
    Enable();
    dest->tv_secs = timerBase->tb_CurrentTime.tv_secs;
    dest->tv_micro = (ULONG)(((long long)tb_eclock_to_usec * timerBase->tb_eclock_micro_mult) >> 16);
    if (dest->tv_micro > timerBase->tb_lastsystime.tv_secs + timerBase->lastsystimetweak) {
        timerBase->lastsystimetweak = 0;
    } else {
        timerBase->lastsystimetweak++;
    }
    timerBase->tb_lastsystime.tv_secs = dest->tv_secs;
    timerBase->tb_lastsystime.tv_micro = dest->tv_micro;
    dest->tv_micro += timerBase->lastsystimetweak;
    if (dest->tv_micro >= 1000000) {
        dest->tv_micro -= 1000000;
        dest->tv_secs++;
    }
    D(bug("ie systime=%d/%d\n", dest->tv_secs, dest->tv_micro));

    AROS_LIBFUNC_EXIT
}

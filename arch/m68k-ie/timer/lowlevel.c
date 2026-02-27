/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE timer.device low-level functions.
          Adapted from arch/m68k-amiga/timer/lowlevel.c.
          All CIA hardware reads replaced with interrupt counting.
          The IE Level 5 interrupt fires at 200Hz (5ms ticks).
*/
#include <exec/types.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/timer.h>

#include <timer_intern.h>

#define DEBUG 0
#include <aros/debug.h>

/* convert timeval pair to 64-bit e-clock unit or 32-bit vblank */
void convertunits(struct TimerBase *TimerBase, struct timeval *tr, int unit)
{
    if (unit == UNIT_VBLANK) {
        ULONG v = tr->tv_secs * TimerBase->tb_vblank_rate;
        v += tr->tv_micro / TimerBase->tb_vblank_micros;
        tr->tv_secs = 0;
        tr->tv_micro = v;
    } else if (unit == UNIT_MICROHZ) {
        long long v = (long long)tr->tv_secs * TimerBase->tb_eclock_rate;
        v += ((long long)tr->tv_micro * TimerBase->tb_micro_eclock_mult) >> 15;
        tr->tv_secs = v >> 32;
        tr->tv_micro = v;
    }
}

/* dst++ */
void inc64(struct timeval *dst)
{
    dst->tv_micro++;
    if (dst->tv_micro == 0)
        dst->tv_secs++;
}
/* dst += src */
void add64(struct timeval *dst, struct timeval *src)
{
    ULONG old = dst->tv_micro;
    dst->tv_micro += src->tv_micro;
    if (old > dst->tv_micro)
        dst->tv_secs++;
    dst->tv_secs += src->tv_secs;
}
/* true if tv1 == tv2 */
BOOL equ64(struct timeval *tv1, struct timeval *tv2)
{
    return tv1->tv_secs == tv2->tv_secs && tv1->tv_micro == tv2->tv_micro;
}
/* return true if tv1 >= tv2 */
BOOL cmp64(struct timeval *tv1, struct timeval *tv2)
{
    if (tv1->tv_secs > tv2->tv_secs)
        return TRUE;
    if (tv1->tv_secs == tv2->tv_secs && tv1->tv_micro >= tv2->tv_micro)
        return TRUE;
    return FALSE;
}
/* return (larger - smaller) or 0xffffffff if result does not fit in ULONG */
ULONG sub64(struct timeval *larger, struct timeval *smaller)
{
    if (larger->tv_secs == smaller->tv_secs)
        return larger->tv_micro - smaller->tv_micro;
    if (larger->tv_secs == smaller->tv_secs + 1) {
        if (larger->tv_micro > smaller->tv_micro)
            return 0xffffffff;
        return (~smaller->tv_micro) + larger->tv_micro + 1;
    }
    return 0xffffffff;
}

/*
 * addmicro: update the micro count for the current time position.
 * On Amiga this reads CIA counter registers to get sub-tick precision.
 * On IE, we have 5ms tick resolution — no sub-tick counter available,
 * so we simply use the tick count as-is.
 */
void addmicro(struct TimerBase *TimerBase, struct timeval *tv)
{
    add64(tv, &TimerBase->tb_micro_count);

    if (!TimerBase->tb_micro_on)
        return;
    if (IsListEmpty(&TimerBase->tb_Lists[UNIT_MICROHZ])) {
        TimerBase->tb_micro_on = FALSE;
        return;
    }
}

/* Disabled state assumed */
void CheckTimer(struct TimerBase *TimerBase, UWORD unitnum)
{
    if (unitnum == UNIT_VBLANK) {
        TimerBase->tb_vblank_on = TRUE;
    } else if (unitnum == UNIT_MICROHZ) {
        if (!TimerBase->tb_micro_on) {
            TimerBase->tb_micro_on = TRUE;
            D(bug("ie timer microhz kickstarted\n"));
        }
        /* On Amiga, this would force a CIA interrupt to re-check.
         * On IE, the next Level 5 tick will process the queue.
         */
    }
}

/*
 * GetEClock: return E-clock ticks since last overflow.
 * On Amiga this reads CIA timer counter registers.
 * On IE, we don't have a sub-tick counter, so always return 0.
 * The E-clock count is maintained entirely by the eclock interrupt handler.
 */
ULONG GetEClock(struct TimerBase *TimerBase)
{
    return 0;
}

/*
 * E-clock interrupt handler — called from Level 5 via INTB_EXTER.
 * Updates the synthetic E-clock counter and system time.
 */
AROS_INTH1(ie_eclock_handler, struct TimerBase *, TimerBase)
{
    AROS_INTFUNC_INIT

    ULONG old;

    D(bug("ie eclock tick\n"));

    /* Add one tick's worth of E-clock cycles */
    old = TimerBase->tb_eclock.ev_lo;
    TimerBase->tb_eclock.ev_lo += IE_ECLOCK_PER_TICK;
    if (old > TimerBase->tb_eclock.ev_lo)
        TimerBase->tb_eclock.ev_hi++;

    /* Track microseconds for system time */
    TimerBase->tb_eclock_to_usec += IE_ECLOCK_PER_TICK;
    if (TimerBase->tb_eclock_to_usec >= TimerBase->tb_eclock_rate) {
        TimerBase->tb_eclock_to_usec -= TimerBase->tb_eclock_rate;
        TimerBase->tb_CurrentTime.tv_secs++;
        TimerBase->tb_Elapsed.tv_secs++;
    }

    /* Process microhz timer request queue */
    if (TimerBase->tb_micro_on) {
        struct timerequest *tr, *next;

        /* Each tick = 5ms = 5000us worth of e-clock cycles */
        old = TimerBase->tb_micro_count.tv_micro;
        TimerBase->tb_micro_count.tv_micro += IE_ECLOCK_PER_TICK;
        if (old > TimerBase->tb_micro_count.tv_micro)
            TimerBase->tb_micro_count.tv_secs++;

        Disable();

        ForeachNodeSafe(&TimerBase->tb_Lists[UNIT_MICROHZ], tr, next) {
            if (cmp64(&TimerBase->tb_micro_count, &tr->tr_time)) {
                Remove((struct Node *)tr);
                tr->tr_time.tv_secs = tr->tr_time.tv_micro = 0;
                tr->tr_node.io_Error = 0;
                ReplyMsg((struct Message *)tr);
                D(bug("ie microhz %x done\n", tr));
            } else {
                break; /* first not finished, can stop searching */
            }
        }

        if (IsListEmpty(&TimerBase->tb_Lists[UNIT_MICROHZ])) {
            TimerBase->tb_micro_on = FALSE;
        }

        Enable();
    }

    return FALSE;

    AROS_INTFUNC_EXIT
}

/*
 * VBlank interrupt handler — called from Level 4 via INTB_VERTB.
 * Increments the VBlank counter and processes UNIT_VBLANK requests.
 */
AROS_INTH1(ie_vbint, struct TimerBase *, TimerBase)
{
    AROS_INTFUNC_INIT

    struct timerequest *tr, *next;

    if (TimerBase->tb_vblank_on == FALSE)
        return 0;
    inc64(&TimerBase->tb_vb_count);

    Disable();
    ForeachNodeSafe(&TimerBase->tb_Lists[UNIT_VBLANK], tr, next) {
        if (cmp64(&TimerBase->tb_vb_count, &tr->tr_time)) {
            Remove((struct Node *)tr);
            tr->tr_time.tv_secs = tr->tr_time.tv_micro = 0;
            tr->tr_node.io_Error = 0;
            ReplyMsg((struct Message *)tr);
            D(bug("ie vblank %x done\n", tr));
        } else {
            break;
        }
    }
    if (IsListEmpty(&TimerBase->tb_Lists[UNIT_VBLANK])) {
        TimerBase->tb_vblank_on = FALSE;
    }
    Enable();

    return 0;

    AROS_INTFUNC_EXIT
}

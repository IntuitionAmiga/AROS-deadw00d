#ifndef _TIMER_INTERN_H
#define _TIMER_INTERN_H

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE timer.device internal structures.
          Adapted from arch/m68k-amiga/timer/timer_intern.h.
          All CIA hardware references removed — the IE uses interrupt
          counting from the host-side timer goroutines:
            Level 4 (VBL, 60Hz) → UNIT_VBLANK
            Level 5 (System timer, 200Hz/5ms) → UNIT_MICROHZ/UNIT_ECLOCK
    Lang: english
*/

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef EXEC_LISTS_H
#include <exec/lists.h>
#endif
#ifndef EXEC_INTERRUPTS_H
#include <exec/interrupts.h>
#endif
#ifndef EXEC_IO_H
#include <exec/io.h>
#endif
#ifndef EXEC_DEVICES_H
#include <exec/devices.h>
#endif
#ifndef DEVICES_TIMER_H
#include <devices/timer.h>
#endif

#define NUM_LISTS       2

/*
 * Synthetic E-clock base unit.
 * The IE Level 5 timer fires at 200Hz. To maintain compatibility with
 * software that expects NTSC-like E-clock rates (~715909 Hz), we add
 * IE_ECLOCK_PER_TICK (715909/200 ≈ 3580) E-clock units per tick.
 */
#define ECLOCK_BASE     0x10000
#define IE_ECLOCK_PER_TICK  3580

struct TimerBase
{
    /* Required by the system */
    struct Device tb_Device;

    struct timeval tb_CurrentTime;       /* system time */
    struct timeval tb_Elapsed;           /* uptime */
    struct timeval tb_lastsystime;

    struct MinList tb_Lists[NUM_LISTS];

    /* E-Clock (synthetic, counted from Level 5 ticks) */
    struct EClockVal tb_eclock;
    ULONG tb_eclock_rate;                /* 715909 (NTSC) */
    ULONG tb_eclock_to_usec;             /* E-clock ticks since last second */

    /* Microhz timer (Level 5 interrupt, 5ms resolution) */
    struct Interrupt tb_timer_int;       /* Level 5 / INTB_EXTER handler */
    struct timeval tb_micro_count;       /* 64-bit tick counter */
    BOOL tb_micro_on;

    /* VBlank (Level 4 interrupt, 60Hz) */
    struct Interrupt tb_vbint;           /* Level 4 / INTB_VERTB handler */
    struct timeval tb_vb_count;
    UWORD tb_vblank_rate;                /* 60 */
    UWORD tb_vblank_micros;             /* 16667 */
    BOOL tb_vblank_on;

    /* Conversion constants */
    ULONG tb_eclock_micro_mult;          /* e-clock → microsecond multiplier */
    UWORD tb_micro_eclock_mult;          /* microsecond → e-clock multiplier */
    ULONG lastsystimetweak;
};

ULONG GetEClock(struct TimerBase *TimerBase);
void CheckTimer(struct TimerBase *TimerBase, UWORD unitnum);
void addmicro(struct TimerBase *TimerBase, struct timeval *tv);
BOOL cmp64(struct timeval *tv1, struct timeval *tv2);
ULONG sub64(struct timeval *larger, struct timeval *smaller);
void add64(struct timeval *dst, struct timeval *src);
void inc64(struct timeval *dst);
BOOL equ64(struct timeval *tv1, struct timeval *tv2);
void convertunits(struct TimerBase *TimerBase, struct timeval *tr, int unit);

#endif /* _TIMER_INTERN_H */

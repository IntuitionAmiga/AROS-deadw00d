/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: ReadBattClock() for Intuition Engine.
          Reads host UTC time via MMIO and converts to Amiga epoch.
*/

#include <proto/battclock.h>
#include "battclock_intern.h"
#include "ie_hwreg.h"

/* Amiga epoch: 1978-01-01 00:00:00 UTC
 * Unix epoch:  1970-01-01 00:00:00 UTC
 * Difference:  252460800 seconds (8 years, 2 leap days)
 */
#define UNIX_TO_AMIGA_OFFSET 252460800UL

AROS_LH0(ULONG, ReadBattClock, struct BattClockBase *, BattClockBase, 2, Battclock)
{
    AROS_LIBFUNC_INIT

    ULONG unix_secs = ie_read32(IE_RTC_EPOCH);

    if (unix_secs <= UNIX_TO_AMIGA_OFFSET)
        return 0;

    return unix_secs - UNIX_TO_AMIGA_OFFSET;

    AROS_LIBFUNC_EXIT
}

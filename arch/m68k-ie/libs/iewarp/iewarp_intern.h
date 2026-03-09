/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: iewarp.library internals
*/

#ifndef IEWARP_INTERN_H
#define IEWARP_INTERN_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/interrupts.h>
#include <libraries/iewarp.h>
#include <ie_hwreg.h>

/* Maximum concurrent tasks that can wait on coprocessor tickets */
#define IEWARP_MAX_WAITERS 8

struct IEWarpWaiter
{
    struct Task *task;      /* Waiting task (NULL = slot free) */
    ULONG        ticket;   /* Ticket being waited on */
    UBYTE        sigBit;   /* Signal bit allocated for this waiter */
};

struct IEWarpBase
{
    struct Library  lib;
    BOOL            workerRunning;  /* TRUE when IE64 worker is active */
    ULONG           threshold;      /* Byte threshold for acceleration */
    BOOL            batchMode;      /* TRUE between BatchBegin/BatchEnd */
    ULONG           lastTicket;     /* Last ticket from batched ops */
    struct Interrupt coprocInterrupt; /* Level 6 completion interrupt */
    struct IEWarpWaiter waiters[IEWARP_MAX_WAITERS]; /* Per-ticket wait slots */
};

/* Dispatch a warp operation to the IE64 coprocessor.
 * Returns a ticket ID, or 0 if fallback was used. */
ULONG ie_warp_dispatch(struct IEWarpBase *base, ULONG op,
                       APTR reqBuf, ULONG reqLen,
                       APTR respBuf, ULONG respCap);

#endif /* IEWARP_INTERN_H */

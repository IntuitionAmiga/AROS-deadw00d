/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: iewarp.library core dispatch — writes MMIO registers and enqueues ops
*/

#include <proto/exec.h>
#include <devices/timer.h>
#include "iewarp_intern.h"

ULONG ie_warp_dispatch(struct IEWarpBase *base, ULONG op,
                       APTR reqBuf, ULONG reqLen,
                       APTR respBuf, ULONG respCap)
{
    ULONG ticket;

    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_OP, op);
    ie_write32(IE_COPROC_REQ_PTR, (ULONG)reqBuf);
    ie_write32(IE_COPROC_REQ_LEN, reqLen);
    ie_write32(IE_COPROC_RESP_PTR, (ULONG)respBuf);
    ie_write32(IE_COPROC_RESP_CAP, respCap);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

    if (ie_read32(IE_COPROC_CMD_STATUS) != 0)
    {
        ie_warp_track_error(base, 0);
        return 0;
    }

    ticket = ie_read32(IE_COPROC_TICKET);

    ie_warp_track_accel(base, op, reqLen);

    if (base->batchMode)
        base->lastTicket = ticket;

    return ticket;
}

/* ── Stats tracking helpers ──────────────────────────────────── */

void ie_warp_track_accel(struct IEWarpBase *base, ULONG op, ULONG bytes)
{
    struct Task *me;
    int i, freeSlot;
    ULONG callerID;

    /* Per-op counters (unsynchronized — approximate) */
    if (op < IEWARP_MAX_OPS)
    {
        base->opCounters[op].accelCount++;
        base->opCounters[op].accelBytes += bytes;
    }

    /* Per-task + per-caller tracking (Disable/Enable protected) */
    me = FindTask(NULL);
    freeSlot = -1;

    Disable();
    for (i = 1; i < IEWARP_MAX_TASKS; i++)
    {
        if (base->taskStats[i].task == me)
        {
            base->taskStats[i].ops++;
            base->taskStats[i].bytes += bytes;
            callerID = base->taskCallerID[i];
            goto found_task;
        }
        if (freeSlot == -1 && !base->taskStats[i].task)
            freeSlot = i;
    }

    /* Task not found — allocate or overflow */
    if (freeSlot != -1)
    {
        const char *n = me->tc_Node.ln_Name;
        int j;
        base->taskStats[freeSlot].task = me;
        for (j = 0; j < 31 && n && n[j]; j++)
            base->taskStats[freeSlot].name[j] = n[j];
        base->taskStats[freeSlot].name[j] = 0;
        base->taskStats[freeSlot].ops = 1;
        base->taskStats[freeSlot].bytes = bytes;
        callerID = base->taskCallerID[freeSlot];
    }
    else
    {
        /* Overflow slot 0 */
        base->taskStats[0].ops++;
        base->taskStats[0].bytes += bytes;
        callerID = base->taskCallerID[0];
    }
found_task:

    /* Per-caller tracking (inside Disable/Enable — slot allocation must be atomic) */
    if (callerID > 0 && callerID < IEWARP_CALLER_MAX)
    {
        for (i = 0; i < IEWARP_MAX_CALLERS; i++)
        {
            if (base->callerStats[i].callerID == callerID)
            {
                base->callerStats[i].ops++;
                base->callerStats[i].bytes += bytes;
                break;
            }
            if (base->callerStats[i].callerID == 0)
            {
                base->callerStats[i].callerID = callerID;
                base->callerStats[i].ops = 1;
                base->callerStats[i].bytes = bytes;
                break;
            }
        }
    }
    Enable();

    /* Batch tracking */
    if (base->batchMode)
        base->batchStats.currentBatchOps++;

    /* Ring high water mark (approximate) */
    {
        ULONG head = ie_read32(IE_COPROC_RING_DEPTH);
        if (head > base->ringHighWater)
            base->ringHighWater = head;
    }
}

void ie_warp_track_fallback(struct IEWarpBase *base, ULONG op, ULONG bytes)
{
    if (op < IEWARP_MAX_OPS)
    {
        base->opCounters[op].fallbackCount++;
        base->opCounters[op].fallbackBytes += bytes;
    }
}

void ie_warp_track_error(struct IEWarpBase *base, ULONG errorCode)
{
    ULONG err = ie_read32(IE_COPROC_CMD_ERROR);
    switch (err)
    {
    case 5: base->errors.queueFull++; break;    /* COPROC_ERR_QUEUE_FULL */
    case 6: base->errors.workerDown++; break;   /* COPROC_ERR_NO_WORKER */
    case 7: base->errors.staleTicket++; break;  /* COPROC_ERR_STALE_TICKET */
    default: base->errors.enqueueFail++; break;
    }
}

AROS_LH1(ULONG, IEWarpWait,
    AROS_LHA(ULONG, ticket, D0),
    struct IEWarpBase *, IEWarpBase, 26, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (ticket == 0)
        return IE_COPROC_ST_OK;

    if (!IEWarpBase->workerRunning)
        return IE_COPROC_ST_WORKER_DOWN;

    /* Allocate a waiter slot for this task+ticket.
     * Each slot gets its own signal bit so the completion interrupt
     * can wake all waiters independently. */
    {
        struct Task *me = FindTask(NULL);
        BYTE sigBit = AllocSignal(-1);
        ULONG sigMask;
        int slot = -1;
        int i;

        if (sigBit == -1)
        {
            /* No signal bits available — fall back to CMD_WAIT */
            ie_write32(IE_COPROC_TICKET, ticket);
            ie_write32(IE_COPROC_TIMEOUT, 5000);
            ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_WAIT_CMD);
            return ie_read32(IE_COPROC_TICKET_STATUS);
        }

        sigMask = 1UL << sigBit;

        /* Find a free waiter slot */
        Disable();
        for (i = 0; i < IEWARP_MAX_WAITERS; i++)
        {
            if (!IEWarpBase->waiters[i].task)
            {
                IEWarpBase->waiters[i].task = me;
                IEWarpBase->waiters[i].ticket = ticket;
                IEWarpBase->waiters[i].sigBit = sigBit;
                slot = i;
                break;
            }
        }
        Enable();

        if (slot == -1)
        {
            /* All waiter slots full — fall back to CMD_WAIT */
            FreeSignal(sigBit);
            ie_write32(IE_COPROC_TICKET, ticket);
            ie_write32(IE_COPROC_TIMEOUT, 5000);
            ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_WAIT_CMD);
            return ie_read32(IE_COPROC_TICKET_STATUS);
        }

        /* Interrupt-driven poll+wait loop bounded by timer.device.
         * A 5-second one-shot timer ensures Wait() never blocks
         * indefinitely even if the completion IRQ is missed. */
        {
            struct MsgPort *timerPort = CreateMsgPort();
            struct timerequest *timerReq = NULL;
            ULONG timerSig = 0;
            BOOL timerOK = FALSE;

            if (timerPort)
            {
                timerReq = (struct timerequest *)
                    CreateIORequest(timerPort, sizeof(struct timerequest));
                if (timerReq)
                {
                    if (OpenDevice("timer.device", UNIT_VBLANK,
                                   (struct IORequest *)timerReq, 0) == 0)
                    {
                        timerSig = 1UL << timerPort->mp_SigBit;

                        /* Start 5-second one-shot timer */
                        timerReq->tr_node.io_Command = TR_ADDREQUEST;
                        timerReq->tr_time.tv_secs = 5;
                        timerReq->tr_time.tv_micro = 0;
                        SendIO((struct IORequest *)timerReq);
                        timerOK = TRUE;
                    }
                }
            }

            if (!timerOK)
            {
                /* Timer setup failed — fall back to CMD_WAIT */
                if (timerReq) DeleteIORequest((struct IORequest *)timerReq);
                if (timerPort) DeleteMsgPort(timerPort);
                IEWarpBase->waiters[slot].task = NULL;
                FreeSignal(sigBit);
                ie_write32(IE_COPROC_TICKET, ticket);
                ie_write32(IE_COPROC_TIMEOUT, 5000);
                ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_WAIT_CMD);
                return ie_read32(IE_COPROC_TICKET_STATUS);
            }

            for (;;)
            {
                ULONG sigs;

                SetSignal(0, sigMask);

                ie_write32(IE_COPROC_TICKET, ticket);
                ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_POLL);
                {
                    ULONG status = ie_read32(IE_COPROC_TICKET_STATUS);
                    if (status >= IE_COPROC_ST_OK)
                    {
                        /* Completed — cancel timer and clean up */
                        if (!CheckIO((struct IORequest *)timerReq))
                            AbortIO((struct IORequest *)timerReq);
                        WaitIO((struct IORequest *)timerReq);
                        CloseDevice((struct IORequest *)timerReq);
                        DeleteIORequest((struct IORequest *)timerReq);
                        DeleteMsgPort(timerPort);
                        IEWarpBase->waiters[slot].task = NULL;
                        FreeSignal(sigBit);
                        return status;
                    }
                }

                /* Wait for completion IRQ or timer expiry */
                sigs = Wait(sigMask | timerSig);

                if (sigs & timerSig)
                {
                    /* Timer expired — fall back to CMD_WAIT which
                     * has a real Go-side timeout guarantee. */
                    WaitIO((struct IORequest *)timerReq);
                    CloseDevice((struct IORequest *)timerReq);
                    DeleteIORequest((struct IORequest *)timerReq);
                    DeleteMsgPort(timerPort);
                    IEWarpBase->waiters[slot].task = NULL;
                    FreeSignal(sigBit);
                    ie_write32(IE_COPROC_TICKET, ticket);
                    ie_write32(IE_COPROC_TIMEOUT, 5000);
                    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_WAIT_CMD);
                    return ie_read32(IE_COPROC_TICKET_STATUS);
                }

                /* Completion signal — loop back and re-poll */
            }
        }
    }

    AROS_LIBFUNC_EXIT
}

AROS_LH1(ULONG, IEWarpPoll,
    AROS_LHA(ULONG, ticket, D0),
    struct IEWarpBase *, IEWarpBase, 27, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (ticket == 0)
        return IE_COPROC_ST_OK;

    ie_write32(IE_COPROC_TICKET, ticket);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_POLL);
    return ie_read32(IE_COPROC_TICKET_STATUS);

    AROS_LIBFUNC_EXIT
}

AROS_LH0(ULONG, IEWarpBatchBegin,
    struct IEWarpBase *, IEWarpBase, 24, IEWarp)
{
    AROS_LIBFUNC_INIT

    IEWarpBase->batchMode = TRUE;
    IEWarpBase->lastTicket = 0;
    IEWarpBase->batchStats.currentBatchOps = 0;
    return 0;

    AROS_LIBFUNC_EXIT
}

AROS_LH0(ULONG, IEWarpBatchEnd,
    struct IEWarpBase *, IEWarpBase, 25, IEWarp)
{
    AROS_LIBFUNC_INIT

    ULONG ticket = IEWarpBase->lastTicket;
    IEWarpBase->batchStats.batchCount++;
    IEWarpBase->batchStats.totalBatchedOps += IEWarpBase->batchStats.currentBatchOps;
    if (IEWarpBase->batchStats.currentBatchOps > IEWarpBase->batchStats.maxBatchSize)
        IEWarpBase->batchStats.maxBatchSize = IEWarpBase->batchStats.currentBatchOps;
    IEWarpBase->batchMode = FALSE;
    IEWarpBase->lastTicket = 0;
    return ticket;

    AROS_LIBFUNC_EXIT
}

AROS_LH0(ULONG, IEWarpGetThreshold,
    struct IEWarpBase *, IEWarpBase, 28, IEWarp)
{
    AROS_LIBFUNC_INIT
    return IEWarpBase->threshold;
    AROS_LIBFUNC_EXIT
}

AROS_LH1(ULONG, IEWarpGetStats,
    AROS_LHA(APTR, statsBuf, A0),
    struct IEWarpBase *, IEWarpBase, 29, IEWarp)
{
    AROS_LIBFUNC_INIT

    struct IEWarpStats *stats = (struct IEWarpStats *)statsBuf;
    if (stats)
    {
        stats->ops = ie_read32(IE_COPROC_STATS_OPS);
        stats->bytes = ie_read32(IE_COPROC_STATS_BYTES);
        stats->overheadNs = ie_read32(IE_COPROC_DISPATCH_OVERHEAD);
        stats->completedTicket = ie_read32(IE_COPROC_COMPLETED_TICKET);
        stats->threshold = IEWarpBase->threshold;
        stats->ringDepth = ie_read32(IE_COPROC_RING_DEPTH);
        stats->ringHighWater = IEWarpBase->ringHighWater;
        stats->uptimeSecs = ie_read32(IE_COPROC_WORKER_UPTIME);
        stats->irqEnabled = ie_read32(IE_COPROC_IRQ_CTRL);
        stats->workerState = ie_read32(IE_COPROC_WORKER_STATE);
        stats->busyPct = ie_read32(IE_COPROC_BUSY_PCT);
    }
    return 0;

    AROS_LIBFUNC_EXIT
}

/* ── New IEWarpMon functions (slots 30-38) ────────────────────── */

AROS_LH1(VOID, IEWarpSetCaller,
    AROS_LHA(ULONG, callerID, D0),
    struct IEWarpBase *, IEWarpBase, 30, IEWarp)
{
    AROS_LIBFUNC_INIT

    struct Task *me = FindTask(NULL);
    int i, freeSlot = -1;

    Disable();
    for (i = 1; i < IEWARP_MAX_TASKS; i++)
    {
        if (IEWarpBase->taskStats[i].task == me)
        {
            IEWarpBase->taskCallerID[i] = callerID;
            Enable();
            return;
        }
        if (freeSlot == -1 && !IEWarpBase->taskStats[i].task)
            freeSlot = i;
    }

    if (freeSlot != -1)
    {
        const char *n = me->tc_Node.ln_Name;
        int j;
        IEWarpBase->taskStats[freeSlot].task = me;
        for (j = 0; j < 31 && n && n[j]; j++)
            IEWarpBase->taskStats[freeSlot].name[j] = n[j];
        IEWarpBase->taskStats[freeSlot].name[j] = 0;
        IEWarpBase->taskStats[freeSlot].ops = 0;
        IEWarpBase->taskStats[freeSlot].bytes = 0;
        IEWarpBase->taskCallerID[freeSlot] = callerID;
    }
    else
    {
        IEWarpBase->taskCallerID[0] = callerID;
    }
    Enable();

    AROS_LIBFUNC_EXIT
}

AROS_LH2(ULONG, IEWarpGetOpStats,
    AROS_LHA(APTR,  buf, A0),
    AROS_LHA(ULONG, maxOps, D0),
    struct IEWarpBase *, IEWarpBase, 31, IEWarp)
{
    AROS_LIBFUNC_INIT

    struct IEWarpOpCounter *dst = (struct IEWarpOpCounter *)buf;
    ULONG count;
    ULONG i;

    if (!dst || maxOps == 0)
        return 0;

    /* Copy all entries up to maxOps, preserving index for caller */
    count = maxOps < IEWARP_MAX_OPS ? maxOps : IEWARP_MAX_OPS;
    for (i = 0; i < count; i++)
    {
        dst[i] = IEWarpBase->opCounters[i];
    }
    return count;

    AROS_LIBFUNC_EXIT
}

AROS_LH2(ULONG, IEWarpGetCallerStats,
    AROS_LHA(APTR,  buf, A0),
    AROS_LHA(ULONG, maxEntries, D0),
    struct IEWarpBase *, IEWarpBase, 32, IEWarp)
{
    AROS_LIBFUNC_INIT

    struct IEWarpCallerEntry *dst = (struct IEWarpCallerEntry *)buf;
    ULONG count = 0;
    int i;

    if (!dst || maxEntries == 0)
        return 0;

    Disable();
    for (i = 0; i < IEWARP_MAX_CALLERS && count < maxEntries; i++)
    {
        if (IEWarpBase->callerStats[i].callerID != 0)
        {
            dst[count] = IEWarpBase->callerStats[i];
            count++;
        }
    }
    Enable();
    return count;

    AROS_LIBFUNC_EXIT
}

AROS_LH2(ULONG, IEWarpGetTaskStats,
    AROS_LHA(APTR,  buf, A0),
    AROS_LHA(ULONG, maxEntries, D0),
    struct IEWarpBase *, IEWarpBase, 33, IEWarp)
{
    AROS_LIBFUNC_INIT

    struct IEWarpTaskEntry *dst = (struct IEWarpTaskEntry *)buf;
    ULONG count = 0;
    int i;

    if (!dst || maxEntries == 0)
        return 0;

    Disable();
    for (i = 0; i < IEWARP_MAX_TASKS && count < maxEntries; i++)
    {
        if (IEWarpBase->taskStats[i].task != NULL)
        {
            dst[count] = IEWarpBase->taskStats[i];
            count++;
        }
    }
    Enable();
    return count;

    AROS_LIBFUNC_EXIT
}

AROS_LH1(ULONG, IEWarpGetErrorStats,
    AROS_LHA(APTR, buf, A0),
    struct IEWarpBase *, IEWarpBase, 34, IEWarp)
{
    AROS_LIBFUNC_INIT

    struct IEWarpErrorStats *dst = (struct IEWarpErrorStats *)buf;
    if (dst)
        *dst = IEWarpBase->errors;
    return 0;

    AROS_LIBFUNC_EXIT
}

AROS_LH1(ULONG, IEWarpGetBatchStats,
    AROS_LHA(APTR, buf, A0),
    struct IEWarpBase *, IEWarpBase, 35, IEWarp)
{
    AROS_LIBFUNC_INIT

    struct IEWarpBatchStats *dst = (struct IEWarpBatchStats *)buf;
    if (dst)
        *dst = IEWarpBase->batchStats;
    return 0;

    AROS_LIBFUNC_EXIT
}

AROS_LH2(ULONG, IEWarpGetWaiterInfo,
    AROS_LHA(APTR,  buf, A0),
    AROS_LHA(ULONG, maxEntries, D0),
    struct IEWarpBase *, IEWarpBase, 36, IEWarp)
{
    AROS_LIBFUNC_INIT

    struct IEWarpWaiterInfo *dst = (struct IEWarpWaiterInfo *)buf;
    ULONG count = 0;
    int i;

    if (!dst || maxEntries == 0)
        return 0;

    Disable();
    for (i = 0; i < IEWARP_MAX_WAITERS && count < maxEntries; i++)
    {
        if (IEWarpBase->waiters[i].task)
        {
            const char *n = IEWarpBase->waiters[i].task->tc_Node.ln_Name;
            int j;
            for (j = 0; j < 31 && n && n[j]; j++)
                dst[count].taskName[j] = n[j];
            dst[count].taskName[j] = 0;
            dst[count].ticket = IEWarpBase->waiters[i].ticket;
            count++;
        }
    }
    Enable();
    return count;

    AROS_LIBFUNC_EXIT
}

AROS_LH1(VOID, IEWarpSetThreshold,
    AROS_LHA(ULONG, threshold, D0),
    struct IEWarpBase *, IEWarpBase, 37, IEWarp)
{
    AROS_LIBFUNC_INIT
    IEWarpBase->threshold = threshold;
    AROS_LIBFUNC_EXIT
}

AROS_LH0(VOID, IEWarpResetAllStats,
    struct IEWarpBase *, IEWarpBase, 38, IEWarp)
{
    AROS_LIBFUNC_INIT

    int i;

    Disable();

    /* Clear per-op counters */
    for (i = 0; i < IEWARP_MAX_OPS; i++)
    {
        IEWarpBase->opCounters[i].accelCount = 0;
        IEWarpBase->opCounters[i].accelBytes = 0;
        IEWarpBase->opCounters[i].fallbackCount = 0;
        IEWarpBase->opCounters[i].fallbackBytes = 0;
    }

    /* Clear per-caller stats */
    for (i = 0; i < IEWARP_MAX_CALLERS; i++)
    {
        IEWarpBase->callerStats[i].callerID = 0;
        IEWarpBase->callerStats[i].ops = 0;
        IEWarpBase->callerStats[i].bytes = 0;
    }

    /* Clear error and batch stats */
    IEWarpBase->errors.queueFull = 0;
    IEWarpBase->errors.workerDown = 0;
    IEWarpBase->errors.staleTicket = 0;
    IEWarpBase->errors.enqueueFail = 0;
    IEWarpBase->batchStats.batchCount = 0;
    IEWarpBase->batchStats.totalBatchedOps = 0;
    IEWarpBase->batchStats.maxBatchSize = 0;
    IEWarpBase->batchStats.currentBatchOps = 0;
    IEWarpBase->ringHighWater = 0;

    /* Clear task slots (keep overflow sentinel at slot 0) */
    for (i = 1; i < IEWARP_MAX_TASKS; i++)
    {
        IEWarpBase->taskStats[i].task = NULL;
        IEWarpBase->taskStats[i].ops = 0;
        IEWarpBase->taskStats[i].bytes = 0;
        IEWarpBase->taskCallerID[i] = 0;
    }
    IEWarpBase->taskStats[0].ops = 0;
    IEWarpBase->taskStats[0].bytes = 0;

    Enable();

    /* Clear Go-side global counters */
    ie_write32(IE_COPROC_STATS_RESET, 1);

    AROS_LIBFUNC_EXIT
}

/*
 * IEWarpWorkerStop — slot 42
 * Stops the IE64 worker and updates the library's workerRunning flag.
 * All subsequent iewarp dispatch calls will fall back to M68K until
 * IEWarpWorkerStart is called.
 */
AROS_LH0(VOID, IEWarpWorkerStop,
    struct IEWarpBase *, IEWarpBase, 42, IEWarp)
{
    AROS_LIBFUNC_INIT

    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_STOP);

    if (ie_read32(IE_COPROC_CMD_STATUS) == 0)
        IEWarpBase->workerRunning = FALSE;

    AROS_LIBFUNC_EXIT
}

/*
 * IEWarpWorkerStart — slot 43
 * Restarts the IE64 worker using the service binary path from library init.
 * Programs IE_COPROC_NAME_PTR before issuing START and updates workerRunning.
 */
AROS_LH0(VOID, IEWarpWorkerStart,
    struct IEWarpBase *, IEWarpBase, 43, IEWarp)
{
    AROS_LIBFUNC_INIT

    /* ie64_service is the static path string from iewarp_init.c —
     * we redeclare it here since it's a fixed ROM constant. */
    static const char ie64_service[] = "sdk/examples/asm/iewarp_service.ie64";

    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_NAME_PTR, (ULONG)ie64_service);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_START);

    if (ie_read32(IE_COPROC_CMD_STATUS) == 0)
        IEWarpBase->workerRunning = TRUE;

    AROS_LIBFUNC_EXIT
}

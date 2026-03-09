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
        return 0;

    ticket = ie_read32(IE_COPROC_TICKET);

    if (base->batchMode)
        base->lastTicket = ticket;

    return ticket;
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
    return 0;

    AROS_LIBFUNC_EXIT
}

AROS_LH0(ULONG, IEWarpBatchEnd,
    struct IEWarpBase *, IEWarpBase, 25, IEWarp)
{
    AROS_LIBFUNC_INIT

    ULONG ticket = IEWarpBase->lastTicket;
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
    }
    return 0;

    AROS_LIBFUNC_EXIT
}

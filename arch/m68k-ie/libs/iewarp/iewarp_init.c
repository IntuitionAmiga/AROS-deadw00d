/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: iewarp.library initialization — auto-starts IE64 worker on boot
*/

#include <aros/symbolsets.h>
#include <hardware/intbits.h>
#include <proto/exec.h>

#include "iewarp_intern.h"

/* Service binary path (relative to baseDir) */
static const char ie64_service[] = "sdk/examples/asm/iewarp_service.ie64";

/* Completion interrupt handler — signals all waiting tasks */
static AROS_INTH1(coprocCompletionHandler, struct IEWarpBase *, base)
{
    AROS_INTFUNC_INIT

    int i;
    for (i = 0; i < IEWARP_MAX_WAITERS; i++)
    {
        if (base->waiters[i].task)
            Signal(base->waiters[i].task, 1UL << base->waiters[i].sigBit);
    }

    return 0;

    AROS_INTFUNC_EXIT
}

static int IEWarp_Init(struct IEWarpBase *base)
{
    ULONG nameAddr;

    base->workerRunning = FALSE;
    base->threshold = 1024;  /* Default 1KB threshold, calibrated later */
    base->batchMode = FALSE;
    base->lastTicket = 0;

    /* Clear all waiter slots */
    {
        int i;
        for (i = 0; i < IEWARP_MAX_WAITERS; i++)
            base->waiters[i].task = NULL;
    }

    /* The service filename string is in ROM (static const).
     * Pass its bus address directly — the Go-side readFileName()
     * reads from bus memory, and ROM at 0x600000+ is bus-accessible. */
    nameAddr = (ULONG)ie64_service;

    /* Start IE64 coprocessor worker */
    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_NAME_PTR, nameAddr);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_START);

    if (ie_read32(IE_COPROC_CMD_STATUS) == 0)
    {
        base->workerRunning = TRUE;

        /* Calibrate dispatch overhead with a NOP operation */
        ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
        ie_write32(IE_COPROC_OP, WARP_OP_NOP);
        ie_write32(IE_COPROC_REQ_PTR, 0);
        ie_write32(IE_COPROC_REQ_LEN, 0);
        ie_write32(IE_COPROC_RESP_PTR, 0);
        ie_write32(IE_COPROC_RESP_CAP, 0);
        ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

        if (ie_read32(IE_COPROC_CMD_STATUS) == 0)
        {
            ULONG ticket = ie_read32(IE_COPROC_TICKET);
            ULONG overhead;

            /* Wait for calibration NOP to complete */
            ie_write32(IE_COPROC_TICKET, ticket);
            ie_write32(IE_COPROC_TIMEOUT, 1000);
            ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_WAIT_CMD);

            /* Read calibrated overhead */
            overhead = ie_read32(IE_COPROC_DISPATCH_OVERHEAD);
            if (overhead > 0)
            {
                /* threshold = overhead_ns * 28 bytes/us / 1000
                 * (M68K copies ~28 bytes/microsecond at 7MHz) */
                base->threshold = (overhead * 28) / 1000;
                if (base->threshold < 64)
                    base->threshold = 64;
                /* Round up to next power of 2 */
                base->threshold--;
                base->threshold |= base->threshold >> 1;
                base->threshold |= base->threshold >> 2;
                base->threshold |= base->threshold >> 4;
                base->threshold |= base->threshold >> 8;
                base->threshold |= base->threshold >> 16;
                base->threshold++;
            }
        }

        /* Install completion interrupt handler */
        base->coprocInterrupt.is_Node.ln_Type = NT_INTERRUPT;
        base->coprocInterrupt.is_Node.ln_Pri = 0;
        base->coprocInterrupt.is_Node.ln_Name = "iewarp.library";
        base->coprocInterrupt.is_Data = base;
        base->coprocInterrupt.is_Code = (VOID_FUNC)coprocCompletionHandler;
        AddIntServer(INTB_COPER, &base->coprocInterrupt);

        /* Enable completion IRQ */
        ie_write32(IE_COPROC_IRQ_CTRL, 1);
    }

    return TRUE;
}

static int IEWarp_Expunge(struct IEWarpBase *base)
{
    if (base->workerRunning)
    {
        /* Disable completion IRQ */
        ie_write32(IE_COPROC_IRQ_CTRL, 0);
        RemIntServer(INTB_COPER, &base->coprocInterrupt);

        /* Stop IE64 worker */
        ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
        ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_STOP);
        base->workerRunning = FALSE;
    }

    return TRUE;
}

ADD2INITLIB(IEWarp_Init, 0)
ADD2EXPUNGELIB(IEWarp_Expunge, 0)

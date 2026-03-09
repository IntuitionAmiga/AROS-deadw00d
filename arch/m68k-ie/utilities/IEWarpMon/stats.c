/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IEWarpMon stats collection and Listview population
*/

#include <stdio.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/iewarp.h>
#include <ie_hwreg.h>

#include "iewarpmon_intern.h"

static char buf[64];

static const char *FormatBytes(ULONG bytes)
{
    if (bytes >= 1048576)
        snprintf(buf, sizeof(buf), "%lu.%lu MB",
                 bytes / 1048576, (bytes % 1048576) * 10 / 1048576);
    else if (bytes >= 1024)
        snprintf(buf, sizeof(buf), "%lu.%lu KB",
                 bytes / 1024, (bytes % 1024) * 10 / 1024);
    else
        snprintf(buf, sizeof(buf), "%lu B", bytes);
    return buf;
}

void UpdateSummary(struct IEWarpMonData *data)
{
    struct IEWarpStats stats;
    struct IEWarpErrorStats errors;
    struct IEWarpBatchStats batch;
    ULONG opsPerSec, bytesPerSec;
    static char sbuf[32];

    IEWarpGetStats(&stats);
    IEWarpGetErrorStats(&errors);
    IEWarpGetBatchStats(&batch);

    /* Worker status */
    snprintf(sbuf, sizeof(sbuf), "%s",
             (stats.workerState & (1 << 2)) ? "Running" : "Stopped");
    set(data->workerStatus, MUIA_Text_Contents, (IPTR)sbuf);

    /* Uptime */
    snprintf(sbuf, sizeof(sbuf), "%lus", stats.uptimeSecs);
    set(data->workerUptime, MUIA_Text_Contents, (IPTR)sbuf);

    /* Busy % */
    snprintf(sbuf, sizeof(sbuf), "%lu%%", stats.busyPct);
    set(data->workerBusyPct, MUIA_Text_Contents, (IPTR)sbuf);

    /* IRQ */
    set(data->irqStatus, MUIA_Text_Contents,
        (IPTR)(stats.irqEnabled ? "Enabled" : "Disabled"));

    /* Throughput */
    snprintf(sbuf, sizeof(sbuf), "%lu", stats.ops);
    set(data->opsTotal, MUIA_Text_Contents, (IPTR)sbuf);

    opsPerSec = (stats.ops - data->prevOps) * 4; /* 250ms interval */
    snprintf(sbuf, sizeof(sbuf), "%lu", opsPerSec);
    set(data->opsPerSec, MUIA_Text_Contents, (IPTR)sbuf);

    set(data->bytesTotal, MUIA_Text_Contents, (IPTR)FormatBytes(stats.bytes));

    bytesPerSec = (stats.bytes - data->prevBytes) * 4;
    snprintf(sbuf, sizeof(sbuf), "%s/s", FormatBytes(bytesPerSec));
    set(data->bytesPerSec, MUIA_Text_Contents, (IPTR)sbuf);

    snprintf(sbuf, sizeof(sbuf), "%lu ns", stats.overheadNs);
    set(data->overheadNs, MUIA_Text_Contents, (IPTR)sbuf);

    snprintf(sbuf, sizeof(sbuf), "%lu", stats.threshold);
    set(data->thresholdTxt, MUIA_Text_Contents, (IPTR)sbuf);

    /* Ring */
    snprintf(sbuf, sizeof(sbuf), "%lu / 16", stats.ringDepth);
    set(data->ringDepth, MUIA_Text_Contents, (IPTR)sbuf);

    snprintf(sbuf, sizeof(sbuf), "%lu", stats.ringHighWater);
    set(data->ringHighWater, MUIA_Text_Contents, (IPTR)sbuf);

    snprintf(sbuf, sizeof(sbuf), "#%lu", stats.completedTicket);
    set(data->lastTicket, MUIA_Text_Contents, (IPTR)sbuf);

    /* Errors */
    snprintf(sbuf, sizeof(sbuf), "%lu", errors.queueFull);
    set(data->errQueueFull, MUIA_Text_Contents, (IPTR)sbuf);

    snprintf(sbuf, sizeof(sbuf), "%lu", errors.workerDown);
    set(data->errWorkerDown, MUIA_Text_Contents, (IPTR)sbuf);

    snprintf(sbuf, sizeof(sbuf), "%lu", errors.staleTicket);
    set(data->errStaleTicket, MUIA_Text_Contents, (IPTR)sbuf);

    snprintf(sbuf, sizeof(sbuf), "%lu", errors.enqueueFail);
    set(data->errEnqueueFail, MUIA_Text_Contents, (IPTR)sbuf);

    /* Batches */
    snprintf(sbuf, sizeof(sbuf), "%lu", batch.batchCount);
    set(data->batchCount, MUIA_Text_Contents, (IPTR)sbuf);

    if (batch.batchCount > 0)
        snprintf(sbuf, sizeof(sbuf), "%lu",
                 batch.totalBatchedOps / batch.batchCount);
    else
        snprintf(sbuf, sizeof(sbuf), "-");
    set(data->batchAvgSize, MUIA_Text_Contents, (IPTR)sbuf);

    snprintf(sbuf, sizeof(sbuf), "%lu", batch.maxBatchSize);
    set(data->batchMaxSize, MUIA_Text_Contents, (IPTR)sbuf);

    data->prevOps = stats.ops;
    data->prevBytes = stats.bytes;
}

void UpdateOpList(struct IEWarpMonData *data)
{
    struct IEWarpOpCounter allOps[IEWARP_MAX_OPS];
    static char line[128];
    ULONG i;

    DoMethod(data->opList, MUIM_List_Clear);

    /* GetOpStats copies all entries preserving index */
    IEWarpGetOpStats(allOps, IEWARP_MAX_OPS);

    for (i = 0; i < IEWARP_MAX_OPS; i++)
    {
        ULONG total, pct;
        const char *name;

        if (!allOps[i].accelCount && !allOps[i].fallbackCount)
            continue;

        total = allOps[i].accelCount + allOps[i].fallbackCount;
        pct = allOps[i].accelCount * 100 / total;

        name = (i < OP_NAMES_COUNT && opNames[i]) ? opNames[i] : "?";

        snprintf(line, sizeof(line),
                 "%-3lu %-14s  Accel:%-6lu  Fall:%-6lu  %3lu%%  %s",
                 i, name,
                 allOps[i].accelCount,
                 allOps[i].fallbackCount,
                 pct,
                 FormatBytes(allOps[i].accelBytes));

        DoMethod(data->opList, MUIM_List_InsertSingle,
                 (IPTR)line, MUIV_List_Insert_Bottom);
    }
}

void UpdateTaskList(struct IEWarpMonData *data)
{
    struct IEWarpTaskEntry tasks[IEWARP_MAX_TASKS];
    ULONG count, i;
    static char line[128];

    DoMethod(data->taskList, MUIM_List_Clear);

    count = IEWarpGetTaskStats(tasks, IEWARP_MAX_TASKS);
    for (i = 0; i < count; i++)
    {
        snprintf(line, sizeof(line), "%-20s  Ops:%-8lu  %s",
                 tasks[i].name,
                 tasks[i].ops,
                 FormatBytes(tasks[i].bytes));

        DoMethod(data->taskList, MUIM_List_InsertSingle,
                 (IPTR)line, MUIV_List_Insert_Bottom);
    }
}

void UpdateCallerList(struct IEWarpMonData *data)
{
    struct IEWarpCallerEntry callers[IEWARP_MAX_CALLERS];
    ULONG count, i;
    static char line[128];

    DoMethod(data->callerList, MUIM_List_Clear);

    count = IEWarpGetCallerStats(callers, IEWARP_MAX_CALLERS);
    for (i = 0; i < count; i++)
    {
        const char *name = "Unknown";
        if (callers[i].callerID < CALLER_NAMES_COUNT &&
            callerNames[callers[i].callerID])
            name = callerNames[callers[i].callerID];

        snprintf(line, sizeof(line), "%-14s  Ops:%-8lu  %s",
                 name,
                 callers[i].ops,
                 FormatBytes(callers[i].bytes));

        DoMethod(data->callerList, MUIM_List_InsertSingle,
                 (IPTR)line, MUIV_List_Insert_Bottom);
    }
}

void UpdateWaiterList(struct IEWarpMonData *data)
{
    struct IEWarpWaiterInfo waiters[8];
    ULONG count, i;
    static char line[128];

    DoMethod(data->waiterList, MUIM_List_Clear);

    count = IEWarpGetWaiterInfo(waiters, 8);
    for (i = 0; i < count; i++)
    {
        snprintf(line, sizeof(line), "%-20s  Ticket:#%lu",
                 waiters[i].taskName,
                 waiters[i].ticket);

        DoMethod(data->waiterList, MUIM_List_InsertSingle,
                 (IPTR)line, MUIV_List_Insert_Bottom);
    }

    if (count == 0)
    {
        DoMethod(data->waiterList, MUIM_List_InsertSingle,
                 (IPTR)"(no tasks waiting)", MUIV_List_Insert_Bottom);
    }
}

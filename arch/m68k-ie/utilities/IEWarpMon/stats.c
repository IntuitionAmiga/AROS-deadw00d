/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IEWarpMon stats collection and Listview population
*/

#include "iewarpmon_intern.h"

static char buf[64];

static void str_append(char *dst, int *pos, const char *src)
{
    while (*src) dst[(*pos)++] = *src++;
}

static void uint_append(char *dst, int *pos, ULONG val)
{
    char tmp[12];
    int i = 0, j;
    if (val == 0) { dst[(*pos)++] = '0'; return; }
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    for (j = 0; j < i; j++) dst[(*pos)++] = tmp[i - 1 - j];
}

static const char *FormatBytes(ULONG bytes)
{
    int p = 0;
    if (bytes >= 1048576)
    {
        uint_append(buf, &p, bytes / 1048576);
        buf[p++] = '.';
        uint_append(buf, &p, (bytes % 1048576) * 10 / 1048576);
        str_append(buf, &p, " MB");
    }
    else if (bytes >= 1024)
    {
        uint_append(buf, &p, bytes / 1024);
        buf[p++] = '.';
        uint_append(buf, &p, (bytes % 1024) * 10 / 1024);
        str_append(buf, &p, " KB");
    }
    else
    {
        uint_append(buf, &p, bytes);
        str_append(buf, &p, " B");
    }
    buf[p] = 0;
    return buf;
}

/* Each field gets its own buffer to avoid pointer reuse issues with MUI */
static char s_worker[16], s_uptime[16], s_busy[16];
static char s_ops[16], s_opsSec[16], s_bytes[32], s_bytesSec[32];
static char s_overhead[16], s_thresh[16];
static char s_rdepth[16], s_rhigh[16], s_ticket[16];
static char s_eqf[16], s_ewd[16], s_est[16], s_eef[16];
static char s_bc[16], s_ba[16], s_bm[16];

static void ulong_to_str(char *dst, ULONG val)
{
    char tmp[12];
    int i = 0, j;
    if (val == 0) { dst[0] = '0'; dst[1] = 0; return; }
    while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; }
    for (j = 0; j < i; j++) dst[j] = tmp[i - 1 - j];
    dst[i] = 0;
}

void UpdateSummary(struct IEWarpMonData *data)
{
    struct IEWarpStats stats = {0};
    struct IEWarpErrorStats errors = {0};
    struct IEWarpBatchStats batch = {0};
    ULONG opsPerSec, bytesPerSec;

    IEWarpGetStats(&stats);
    IEWarpGetErrorStats(&errors);
    IEWarpGetBatchStats(&batch);

    /* Worker status */
    {
        const char *src = (stats.workerState & (1 << 2)) ? "Running" : "Stopped";
        int i; for (i = 0; src[i]; i++) s_worker[i] = src[i]; s_worker[i] = 0;
    }
    set(data->workerStatus, MUIA_Text_Contents, (IPTR)s_worker);

    /* Uptime */
    ulong_to_str(s_uptime, stats.uptimeSecs);
    { int i = 0; while (s_uptime[i]) i++; s_uptime[i] = 's'; s_uptime[i+1] = 0; }
    set(data->workerUptime, MUIA_Text_Contents, (IPTR)s_uptime);

    /* Busy % */
    ulong_to_str(s_busy, stats.busyPct);
    { int i = 0; while (s_busy[i]) i++; s_busy[i] = '%'; s_busy[i+1] = 0; }
    set(data->workerBusyPct, MUIA_Text_Contents, (IPTR)s_busy);

    /* IRQ */
    set(data->irqStatus, MUIA_Text_Contents,
        (IPTR)(stats.irqEnabled ? "Enabled" : "Disabled"));

    /* Throughput */
    ulong_to_str(s_ops, stats.ops);
    set(data->opsTotal, MUIA_Text_Contents, (IPTR)s_ops);

    opsPerSec = (stats.ops - data->prevOps) * 4;
    ulong_to_str(s_opsSec, opsPerSec);
    set(data->opsPerSec, MUIA_Text_Contents, (IPTR)s_opsSec);

    set(data->bytesTotal, MUIA_Text_Contents, (IPTR)FormatBytes(stats.bytes));

    bytesPerSec = (stats.bytes - data->prevBytes) * 4;
    {
        const char *fb = FormatBytes(bytesPerSec);
        int i; for (i = 0; fb[i]; i++) s_bytesSec[i] = fb[i];
        s_bytesSec[i++] = '/'; s_bytesSec[i++] = 's'; s_bytesSec[i] = 0;
    }
    set(data->bytesPerSec, MUIA_Text_Contents, (IPTR)s_bytesSec);

    ulong_to_str(s_overhead, stats.overheadNs);
    { int i = 0; while (s_overhead[i]) i++;
      s_overhead[i++] = ' '; s_overhead[i++] = 'n'; s_overhead[i++] = 's'; s_overhead[i] = 0; }
    set(data->overheadNs, MUIA_Text_Contents, (IPTR)s_overhead);

    ulong_to_str(s_thresh, stats.threshold);
    set(data->thresholdTxt, MUIA_Text_Contents, (IPTR)s_thresh);

    /* Ring */
    ulong_to_str(s_rdepth, stats.ringDepth);
    { int i = 0; while (s_rdepth[i]) i++;
      s_rdepth[i++] = '/'; s_rdepth[i++] = '1'; s_rdepth[i++] = '6'; s_rdepth[i] = 0; }
    set(data->ringDepth, MUIA_Text_Contents, (IPTR)s_rdepth);

    ulong_to_str(s_rhigh, stats.ringHighWater);
    set(data->ringHighWater, MUIA_Text_Contents, (IPTR)s_rhigh);

    s_ticket[0] = '#';
    ulong_to_str(s_ticket + 1, stats.completedTicket);
    set(data->lastTicket, MUIA_Text_Contents, (IPTR)s_ticket);

    /* Errors */
    ulong_to_str(s_eqf, errors.queueFull);
    set(data->errQueueFull, MUIA_Text_Contents, (IPTR)s_eqf);

    ulong_to_str(s_ewd, errors.workerDown);
    set(data->errWorkerDown, MUIA_Text_Contents, (IPTR)s_ewd);

    ulong_to_str(s_est, errors.staleTicket);
    set(data->errStaleTicket, MUIA_Text_Contents, (IPTR)s_est);

    ulong_to_str(s_eef, errors.enqueueFail);
    set(data->errEnqueueFail, MUIA_Text_Contents, (IPTR)s_eef);

    /* Batches */
    ulong_to_str(s_bc, batch.batchCount);
    set(data->batchCount, MUIA_Text_Contents, (IPTR)s_bc);

    if (batch.batchCount > 0)
        ulong_to_str(s_ba, batch.totalBatchedOps / batch.batchCount);
    else
    { s_ba[0] = '-'; s_ba[1] = 0; }
    set(data->batchAvgSize, MUIA_Text_Contents, (IPTR)s_ba);

    ulong_to_str(s_bm, batch.maxBatchSize);
    set(data->batchMaxSize, MUIA_Text_Contents, (IPTR)s_bm);

    data->prevOps = stats.ops;
    data->prevBytes = stats.bytes;
}

void UpdateOpList(struct IEWarpMonData *data)
{
    struct IEWarpOpCounter allOps[IEWARP_MAX_OPS];
    static char line[128];
    ULONG i;

    DoMethod(data->opList, MUIM_List_Clear);
    IEWarpGetOpStats(allOps, IEWARP_MAX_OPS);

    for (i = 0; i < IEWARP_MAX_OPS; i++)
    {
        ULONG total, pct;
        const char *name;
        int p = 0;

        if (!allOps[i].accelCount && !allOps[i].fallbackCount)
            continue;

        total = allOps[i].accelCount + allOps[i].fallbackCount;
        pct = allOps[i].accelCount * 100 / total;
        name = (i < OP_NAMES_COUNT && opNames[i]) ? opNames[i] : "?";

        uint_append(line, &p, i);
        str_append(line, &p, " ");
        str_append(line, &p, name);
        str_append(line, &p, "  A:");
        uint_append(line, &p, allOps[i].accelCount);
        str_append(line, &p, " F:");
        uint_append(line, &p, allOps[i].fallbackCount);
        str_append(line, &p, " ");
        uint_append(line, &p, pct);
        str_append(line, &p, "% ");
        { const char *fb = FormatBytes(allOps[i].accelBytes);
          str_append(line, &p, fb); }
        line[p] = 0;

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
        int p = 0;
        str_append(line, &p, tasks[i].name);
        str_append(line, &p, "  Ops:");
        uint_append(line, &p, tasks[i].ops);
        str_append(line, &p, "  ");
        str_append(line, &p, FormatBytes(tasks[i].bytes));
        line[p] = 0;
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
        int p = 0;
        if (callers[i].callerID < CALLER_NAMES_COUNT &&
            callerNames[callers[i].callerID])
            name = callerNames[callers[i].callerID];
        str_append(line, &p, name);
        str_append(line, &p, "  Ops:");
        uint_append(line, &p, callers[i].ops);
        str_append(line, &p, "  ");
        str_append(line, &p, FormatBytes(callers[i].bytes));
        line[p] = 0;
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
        int p = 0;
        str_append(line, &p, waiters[i].taskName);
        str_append(line, &p, "  Ticket:#");
        uint_append(line, &p, waiters[i].ticket);
        line[p] = 0;
        DoMethod(data->waiterList, MUIM_List_InsertSingle,
                 (IPTR)line, MUIV_List_Insert_Bottom);
    }

    if (count == 0)
    {
        DoMethod(data->waiterList, MUIM_List_InsertSingle,
                 (IPTR)"(no tasks waiting)", MUIV_List_Insert_Bottom);
    }
}

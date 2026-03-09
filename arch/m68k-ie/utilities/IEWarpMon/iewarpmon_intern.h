/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IEWarpMon — coprocessor monitor application internals
*/

#ifndef IEWARPMON_INTERN_H
#define IEWARPMON_INTERN_H

#include <exec/types.h>
#include <libraries/mui.h>
#include <libraries/iewarp.h>

struct IEWarpMonData
{
    Object *application;
    Object *mainwindow;
    Object *pages;

    /* Tab 1: Summary */
    Object *workerStatus;
    Object *workerUptime;
    Object *workerBusyPct;
    Object *irqStatus;
    Object *opsTotal;
    Object *opsPerSec;
    Object *bytesTotal;
    Object *bytesPerSec;
    Object *overheadNs;
    Object *thresholdTxt;
    Object *ringDepth;
    Object *ringHighWater;
    Object *lastTicket;
    /* Errors */
    Object *errQueueFull;
    Object *errWorkerDown;
    Object *errStaleTicket;
    Object *errEnqueueFail;
    /* Batches */
    Object *batchCount;
    Object *batchAvgSize;
    Object *batchMaxSize;
    /* Controls */
    Object *startStopBtn;
    Object *resetStatsBtn;
    Object *irqToggleBtn;
    Object *thresholdStr;
    Object *thresholdSetBtn;

    /* Tab 2: Operations */
    Object *opList;

    /* Tab 3: Tasks */
    Object *taskList;

    /* Tab 4: Libraries */
    Object *callerList;

    /* Tab 5: Waiters */
    Object *waiterList;

    /* Rate computation */
    ULONG prevOps;
    ULONG prevBytes;

    struct Library *IEWarpBase;
};

/* Op code to name mapping */
static const char *opNames[] =
{
    [0]  = "NOP",
    [1]  = "MemCpy",
    [2]  = "MemCpyQuick",
    [3]  = "MemSet",
    [4]  = "MemMove",
    [10] = "BlitCopy",
    [11] = "BlitMask",
    [12] = "BlitScale",
    [13] = "BlitConvert",
    [14] = "BlitAlpha",
    [15] = "FillRect",
    [16] = "AreaFill",
    [17] = "PixelProcess",
    [20] = "AudioMix",
    [21] = "AudioResample",
    [22] = "AudioDecode",
    [30] = "FPBatch",
    [31] = "MatrixMul",
    [32] = "CRC32",
    [40] = "GradientFill",
    [41] = "GlyphRender",
    [42] = "Scroll",
};
#define OP_NAMES_COUNT (sizeof(opNames) / sizeof(opNames[0]))

/* Caller ID to name mapping */
static const char *callerNames[] =
{
    [0]  = "Unknown",
    [1]  = "exec",
    [2]  = "graphics",
    [3]  = "IEGfx",
    [4]  = "cgfx",
    [5]  = "AHI",
    [6]  = "icon",
    [7]  = "math",
    [8]  = "MUI",
    [9]  = "freetype",
    [10] = "datatypes",
    [11] = "app",
};
#define CALLER_NAMES_COUNT (sizeof(callerNames) / sizeof(callerNames[0]))

/* Timer module */
BOOL InitTimer(void);
VOID DeInitTimer(void);
VOID SignalMeAfter(ULONG msecs);
ULONG GetSIG_TIMER(void);

/* Stats module */
void UpdateSummary(struct IEWarpMonData *data);
void UpdateOpList(struct IEWarpMonData *data);
void UpdateTaskList(struct IEWarpMonData *data);
void UpdateCallerList(struct IEWarpMonData *data);
void UpdateWaiterList(struct IEWarpMonData *data);

#endif /* IEWARPMON_INTERN_H */

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IEWarpMon — IE64 coprocessor monitor (MUI application)
*/

#include <stdio.h>

#include "iewarpmon_intern.h"

struct Library *MUIMasterBase = NULL;
struct Library *IEWarpBase = NULL;

#define ID_START_STOP    1
#define ID_RESET_STATS   2
#define ID_IRQ_TOGGLE    3
#define ID_THRESHOLD_SET 4

/* LVO stubs for WorkerStop/WorkerStart (slots 42-43) — not yet in
 * auto-generated defines/iewarp.h; superseded after AROS rebuild. */
#ifndef IEWarpWorkerStop
#define IEWarpWorkerStop() \
    ((void)AROS_LC0(ULONG, IEWarpWorkerStop, \
        struct Library *, IEWarpBase, 42, IEWarp))
#endif
#ifndef IEWarpWorkerStart
#define IEWarpWorkerStart() \
    ((void)AROS_LC0(ULONG, IEWarpWorkerStart, \
        struct Library *, IEWarpBase, 43, IEWarp))
#endif

/* Helper: create a read-only text gadget for ColGroup(2) */
#define LText()     TextObject, MUIA_Text_SetMin, FALSE, End

static Object *MakePageSummary(struct IEWarpMonData *d)
{
    return VGroup,
        /* Status row */
        Child, HGroup,
            Child, LLabel("Worker:"),
            Child, d->workerStatus = LText(),
            Child, LLabel("Uptime:"),
            Child, d->workerUptime = LText(),
            Child, LLabel("Busy:"),
            Child, d->workerBusyPct = LText(),
            Child, LLabel("IRQ:"),
            Child, d->irqStatus = LText(),
        End,

        /* Throughput */
        Child, HGroup,
            Child, VGroup, GroupFrameT("Throughput"),
                Child, ColGroup(2),
                    Child, LLabel("Total Ops:"),
                    Child, d->opsTotal = LText(),
                    Child, LLabel("Ops/sec:"),
                    Child, d->opsPerSec = LText(),
                    Child, LLabel("Total Bytes:"),
                    Child, d->bytesTotal = LText(),
                    Child, LLabel("Bytes/sec:"),
                    Child, d->bytesPerSec = LText(),
                    Child, LLabel("Overhead:"),
                    Child, d->overheadNs = LText(),
                    Child, LLabel("Threshold:"),
                    Child, d->thresholdTxt = LText(),
                End,
            End,
            Child, VGroup, GroupFrameT("Ring Buffer"),
                Child, ColGroup(2),
                    Child, LLabel("Depth:"),
                    Child, d->ringDepth = LText(),
                    Child, LLabel("High Water:"),
                    Child, d->ringHighWater = LText(),
                    Child, LLabel("Last Ticket:"),
                    Child, d->lastTicket = LText(),
                End,
            End,
        End,

        /* Errors + Batches */
        Child, HGroup,
            Child, VGroup, GroupFrameT("Errors"),
                Child, ColGroup(2),
                    Child, LLabel("Queue Full:"),
                    Child, d->errQueueFull = LText(),
                    Child, LLabel("Worker Down:"),
                    Child, d->errWorkerDown = LText(),
                    Child, LLabel("Stale Ticket:"),
                    Child, d->errStaleTicket = LText(),
                    Child, LLabel("Enqueue Fail:"),
                    Child, d->errEnqueueFail = LText(),
                End,
            End,
            Child, VGroup, GroupFrameT("Batches"),
                Child, ColGroup(2),
                    Child, LLabel("Count:"),
                    Child, d->batchCount = LText(),
                    Child, LLabel("Avg Size:"),
                    Child, d->batchAvgSize = LText(),
                    Child, LLabel("Max Size:"),
                    Child, d->batchMaxSize = LText(),
                End,
            End,
        End,

        /* Controls */
        Child, HGroup, GroupFrameT("Controls"),
            Child, d->startStopBtn = SimpleButton("Start/Stop"),
            Child, d->resetStatsBtn = SimpleButton("Reset Stats"),
            Child, d->irqToggleBtn = SimpleButton("Toggle IRQ"),
        End,

        /* Threshold override */
        Child, HGroup,
            Child, LLabel("Threshold:"),
            Child, d->thresholdStr = StringObject,
                MUIA_String_Accept, "0123456789",
                MUIA_String_Integer, 1024,
                MUIA_CycleChain, TRUE,
            End,
            Child, d->thresholdSetBtn = SimpleButton("Set"),
        End,

        /* Approximation note */
        Child, TextObject,
            MUIA_Text_Contents, "\033iCounters are approximate under contention. Reset for a clean baseline.",
            MUIA_Text_SetMin, FALSE,
        End,
    End;
}

static Object *MakePageList(Object **listPtr, const char *title)
{
    return VGroup,
        Child, ListviewObject,
            MUIA_Listview_List, *listPtr = ListObject,
                MUIA_List_Title, (IPTR)title,
                MUIA_List_Format, "",
                MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
                MUIA_List_DestructHook, MUIV_List_DestructHook_String,
            End,
        End,
    End;
}

int main(void)
{
    struct IEWarpMonData data = {0};
    Object *summaryPage, *opPage, *taskPage, *callerPage, *waiterPage;
    ULONG sigs;
    BOOL running = TRUE;

    static const char *tabTitles[] =
    {
        "Summary", "Operations", "Tasks", "Libraries", "Waiters", NULL
    };

    /* Open libraries */
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase)
    {
        PutStr("Cannot open muimaster.library\n");
        return 20;
    }

    IEWarpBase = OpenLibrary("iewarp.library", 0);
    if (!IEWarpBase)
    {
        PutStr("Cannot open iewarp.library\n");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    data.IEWarpBase = IEWarpBase;

    /* Build pages */
    summaryPage = MakePageSummary(&data);
    opPage = MakePageList(&data.opList, "Op    Accel       Fallback     Accel%  Bytes");
    taskPage = MakePageList(&data.taskList, "Task                  Ops        Bytes");
    callerPage = MakePageList(&data.callerList, "Library        Ops        Bytes");
    waiterPage = MakePageList(&data.waiterList, "Task                Ticket");

    /* Build application */
    data.application = ApplicationObject,
        MUIA_Application_Title, (IPTR)"IEWarp Monitor ~ approximate",
        MUIA_Application_Version, (IPTR)"$VER: IEWarpMon 1.0 (09.03.2026)",
        MUIA_Application_Description, (IPTR)"IE64 Coprocessor Monitor",

        SubWindow, data.mainwindow = WindowObject,
            MUIA_Window_Title, (IPTR)"IEWarp Monitor ~ approximate",
            MUIA_Window_ID, MAKE_ID('I','W','M','N'),

            WindowContents, VGroup,
                Child, data.pages = RegisterGroup(tabTitles),
                    Child, summaryPage,
                    Child, opPage,
                    Child, taskPage,
                    Child, callerPage,
                    Child, waiterPage,
                End,
            End,
        End,
    End;

    if (!data.application)
    {
        PutStr("Cannot create MUI application\n");
        CloseLibrary(IEWarpBase);
        CloseLibrary(MUIMasterBase);
        return 20;
    }

    /* Set up notifications */
    DoMethod(data.mainwindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (IPTR)data.application, 2, MUIM_Application_ReturnID,
             MUIV_Application_ReturnID_Quit);

    DoMethod(data.startStopBtn, MUIM_Notify, MUIA_Pressed, FALSE,
             (IPTR)data.application, 2, MUIM_Application_ReturnID,
             ID_START_STOP);

    DoMethod(data.resetStatsBtn, MUIM_Notify, MUIA_Pressed, FALSE,
             (IPTR)data.application, 2, MUIM_Application_ReturnID,
             ID_RESET_STATS);

    DoMethod(data.irqToggleBtn, MUIM_Notify, MUIA_Pressed, FALSE,
             (IPTR)data.application, 2, MUIM_Application_ReturnID,
             ID_IRQ_TOGGLE);

    DoMethod(data.thresholdSetBtn, MUIM_Notify, MUIA_Pressed, FALSE,
             (IPTR)data.application, 2, MUIM_Application_ReturnID,
             ID_THRESHOLD_SET);

    /* Initialize timer */
    if (!InitTimer())
    {
        PutStr("Cannot initialize timer\n");
        MUI_DisposeObject(data.application);
        CloseLibrary(IEWarpBase);
        CloseLibrary(MUIMasterBase);
        return 20;
    }

    /* Open window */
    set(data.mainwindow, MUIA_Window_Open, TRUE);

    /* Initial update */
    UpdateSummary(&data);

    /* Main event loop */
    SignalMeAfter(250);

    while (running)
    {
        ULONG id = DoMethod(data.application, MUIM_Application_NewInput, (IPTR)&sigs);

        switch (id)
        {
        case MUIV_Application_ReturnID_Quit:
            running = FALSE;
            break;

        case ID_START_STOP:
        {
            ULONG state = ie_read32(IE_COPROC_WORKER_STATE);
            if (state & (1 << 2)) /* IE64 bit — stop */
                IEWarpWorkerStop();
            else
                IEWarpWorkerStart();
            break;
        }

        case ID_RESET_STATS:
            IEWarpResetAllStats();
            data.prevOps = 0;
            data.prevBytes = 0;
            break;

        case ID_IRQ_TOGGLE:
        {
            ULONG irq = ie_read32(IE_COPROC_IRQ_CTRL);
            ie_write32(IE_COPROC_IRQ_CTRL, irq ? 0 : 1);
            break;
        }

        case ID_THRESHOLD_SET:
        {
            ULONG val = 0;
            get(data.thresholdStr, MUIA_String_Integer, &val);
            if (val > 0)
                IEWarpSetThreshold(val);
            break;
        }
        }

        if (running && sigs)
        {
            sigs = Wait(sigs | GetSIG_TIMER());

            if (sigs & GetSIG_TIMER())
            {
                ULONG activeTab = 0;

                /* Always update summary */
                UpdateSummary(&data);

                /* Update active tab */
                get(data.pages, MUIA_Group_ActivePage, &activeTab);
                switch (activeTab)
                {
                case 1: UpdateOpList(&data); break;
                case 2: UpdateTaskList(&data); break;
                case 3: UpdateCallerList(&data); break;
                case 4: UpdateWaiterList(&data); break;
                }

                /* Schedule next update */
                SignalMeAfter(250);
            }
        }
    }

    /* Cleanup */
    set(data.mainwindow, MUIA_Window_Open, FALSE);
    DeInitTimer();
    MUI_DisposeObject(data.application);
    CloseLibrary(IEWarpBase);
    CloseLibrary(MUIMasterBase);

    return 0;
}

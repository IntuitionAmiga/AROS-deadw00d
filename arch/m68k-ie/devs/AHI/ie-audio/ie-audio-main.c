/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE AHI audio driver — main entry points.
          Based on the Void driver pattern.  Writes mixed AHI output
          to IE flex channel DAC registers for real audio playback.
*/

#include <aros/libcall.h>
#include <devices/ahi.h>
#include <dos/dostags.h>
#include <exec/memory.h>
#include <libraries/ahi_sub.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#include <stddef.h>

#include "DriverData.h"

#define dd ((struct IEAudioData*) AudioCtrl->ahiac_DriverData)

extern const char LibName[];
extern const char LibIDString[];

void SlaveEntry(void);

/* IE SoundChip fixed at 44100 Hz */
static const LONG frequencies[] = {
    8000,
    11025,
    16000,
    22050,
    32000,
    44100,
    48000,
};

#define FREQUENCIES (sizeof frequencies / sizeof frequencies[0])

/******************************************************************************
** AHIsub_AllocAudio **********************************************************
******************************************************************************/

AROS_LH2(ULONG, AHIsub_AllocAudio,
    AROS_LHA(struct TagItem *,         tagList,   A1),
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    struct IEAudioBase *, IEAudioBase, 5, IEAudio)
{
    AROS_LIBFUNC_INIT

    AudioCtrl->ahiac_DriverData = AllocVec(sizeof(struct IEAudioData),
                                           MEMF_CLEAR | MEMF_PUBLIC);

    if (dd != NULL) {
        dd->slavesignal  = -1;
        dd->mastersignal = AllocSignal(-1);
        dd->mastertask   = (struct Process *)FindTask(NULL);
        dd->ahisubbase   = IEAudioBase;
    } else {
        return AHISF_ERROR;
    }

    if (dd->mastersignal == -1) {
        return AHISF_ERROR;
    }

    return (AHISF_KNOWHIFI |
            AHISF_KNOWSTEREO |
            AHISF_MIXING |
            AHISF_TIMING);

    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_FreeAudio ***********************************************************
******************************************************************************/

AROS_LH1(void, AHIsub_FreeAudio,
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    struct IEAudioBase *, IEAudioBase, 6, IEAudio)
{
    AROS_LIBFUNC_INIT

    if (AudioCtrl->ahiac_DriverData != NULL) {
        FreeSignal(dd->mastersignal);
        FreeVec(AudioCtrl->ahiac_DriverData);
        AudioCtrl->ahiac_DriverData = NULL;
    }

    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_Disable *************************************************************
******************************************************************************/

AROS_LH1(void, AHIsub_Disable,
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    struct IEAudioBase *, IEAudioBase, 7, IEAudio)
{
    AROS_LIBFUNC_INIT
    Forbid();
    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_Enable **************************************************************
******************************************************************************/

AROS_LH1(void, AHIsub_Enable,
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    struct IEAudioBase *, IEAudioBase, 8, IEAudio)
{
    AROS_LIBFUNC_INIT
    Permit();
    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_Start ***************************************************************
******************************************************************************/

static void
StopPlayback(struct AHIAudioCtrlDrv *AudioCtrl, struct IEAudioBase *IEAudioBase)
{
    if (dd->slavetask != NULL) {
        if (dd->slavesignal != -1) {
            Signal((struct Task *)dd->slavetask,
                   1L << dd->slavesignal);
        }
        Wait(1L << dd->mastersignal);
    }
    FreeVec(dd->mixbuffer);
    dd->mixbuffer = NULL;
}

AROS_LH2(ULONG, AHIsub_Start,
    AROS_LHA(ULONG,                    flags,     D0),
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    struct IEAudioBase *, IEAudioBase, 9, IEAudio)
{
    AROS_LIBFUNC_INIT

    /* Stop any existing playback first */
    if (flags & AHISF_PLAY) {
        StopPlayback(AudioCtrl, IEAudioBase);
    }

    if (flags & AHISF_PLAY) {
        struct TagItem proctags[] = {
            { NP_Entry,    (IPTR)&SlaveEntry },
            { NP_Name,     (IPTR)LibName     },
            { NP_Priority, -1                },
            { TAG_DONE,    0                 }
        };

        dd->mixbuffer = AllocVec(AudioCtrl->ahiac_BuffSize,
                                 MEMF_ANY | MEMF_PUBLIC);

        if (dd->mixbuffer == NULL) return AHIE_NOMEM;

        Forbid();

        dd->slavetask = CreateNewProc(proctags);

        if (dd->slavetask != NULL) {
            dd->slavetask->pr_Task.tc_UserData = AudioCtrl;
        }

        Permit();

        if (dd->slavetask != NULL) {
            Wait(1L << dd->mastersignal);

            if (dd->slavetask == NULL) {
                return AHIE_UNKNOWN;
            }
        } else {
            return AHIE_NOMEM;
        }
    }

    if (flags & AHISF_RECORD) {
        return AHIE_UNKNOWN;
    }

    return AHIE_OK;

    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_Update **************************************************************
******************************************************************************/

AROS_LH2(ULONG, AHIsub_Update,
    AROS_LHA(ULONG,                    flags,     D0),
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    struct IEAudioBase *, IEAudioBase, 10, IEAudio)
{
    AROS_LIBFUNC_INIT
    /* Empty — no hardware state to update between mixes */
    return 0;
    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_Stop ****************************************************************
******************************************************************************/

AROS_LH2(void, AHIsub_Stop,
    AROS_LHA(ULONG,                    flags,     D0),
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    struct IEAudioBase *, IEAudioBase, 11, IEAudio)
{
    AROS_LIBFUNC_INIT

    if (flags & AHISF_PLAY) {
        StopPlayback(AudioCtrl, IEAudioBase);
    }

    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_GetAttr *************************************************************
******************************************************************************/

AROS_LH5(IPTR, AHIsub_GetAttr,
    AROS_LHA(ULONG,                    attribute, D0),
    AROS_LHA(LONG,                     argument,  D1),
    AROS_LHA(IPTR,                     def,       D2),
    AROS_LHA(struct TagItem *,         tagList,   A1),
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    struct IEAudioBase *, IEAudioBase, 18, IEAudio)
{
    AROS_LIBFUNC_INIT

    size_t i;

    switch (attribute) {
    case AHIDB_Bits:
        return 16;

    case AHIDB_Frequencies:
        return FREQUENCIES;

    case AHIDB_Frequency:
        return (LONG)frequencies[argument];

    case AHIDB_Index:
        if (argument <= frequencies[0])
            return 0;
        if (argument >= frequencies[FREQUENCIES - 1])
            return FREQUENCIES - 1;
        for (i = 1; i < FREQUENCIES; i++) {
            if (frequencies[i] > argument) {
                if ((argument - frequencies[i - 1]) <
                        (frequencies[i] - argument))
                    return i - 1;
                else
                    return i;
            }
        }
        return 0;

    case AHIDB_Author:
        return (IPTR)"Intuition Engine Team";

    case AHIDB_Copyright:
        return (IPTR)"Public Domain";

    case AHIDB_Version:
        return (IPTR)LibIDString;

    case AHIDB_Record:
        return FALSE;

    case AHIDB_Realtime:
        return TRUE;

    case AHIDB_Outputs:
        return 1;

    case AHIDB_Output:
        return (IPTR)"IE SoundChip";

    default:
        return def;
    }

    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_HardwareControl *****************************************************
******************************************************************************/

AROS_LH3(LONG, AHIsub_HardwareControl,
    AROS_LHA(ULONG,                    attribute, D0),
    AROS_LHA(LONG,                     argument,  D1),
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    struct IEAudioBase *, IEAudioBase, 19, IEAudio)
{
    AROS_LIBFUNC_INIT
    return 0;
    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_SetVol (stub — software mixing only, no hardware acceleration) *****
******************************************************************************/

AROS_LH5(ULONG, AHIsub_SetVol,
    AROS_LHA(UWORD,                    channel,   D0),
    AROS_LHA(LONG,                     volume,    D1),
    AROS_LHA(LONG,                     pan,       D2),
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    AROS_LHA(ULONG,                    flags,     D3),
    struct IEAudioBase *, IEAudioBase, 12, IEAudio)
{
    AROS_LIBFUNC_INIT
    return AHIS_UNKNOWN;
    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_SetFreq (stub) *****************************************************
******************************************************************************/

AROS_LH4(ULONG, AHIsub_SetFreq,
    AROS_LHA(UWORD,                    channel,   D0),
    AROS_LHA(ULONG,                    freq,      D1),
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    AROS_LHA(ULONG,                    flags,     D2),
    struct IEAudioBase *, IEAudioBase, 13, IEAudio)
{
    AROS_LIBFUNC_INIT
    return AHIS_UNKNOWN;
    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_SetSound (stub) ****************************************************
******************************************************************************/

AROS_LH6(ULONG, AHIsub_SetSound,
    AROS_LHA(UWORD,                    channel,   D0),
    AROS_LHA(UWORD,                    sound,     D1),
    AROS_LHA(ULONG,                    offset,    D2),
    AROS_LHA(LONG,                     length,    D3),
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    AROS_LHA(ULONG,                    flags,     D4),
    struct IEAudioBase *, IEAudioBase, 14, IEAudio)
{
    AROS_LIBFUNC_INIT
    return AHIS_UNKNOWN;
    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_SetEffect (stub) ***************************************************
******************************************************************************/

AROS_LH2(ULONG, AHIsub_SetEffect,
    AROS_LHA(APTR,                     effect,    A0),
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    struct IEAudioBase *, IEAudioBase, 15, IEAudio)
{
    AROS_LIBFUNC_INIT
    return AHIS_UNKNOWN;
    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_LoadSound (stub) ***************************************************
******************************************************************************/

AROS_LH4(ULONG, AHIsub_LoadSound,
    AROS_LHA(UWORD,                    sound,     D0),
    AROS_LHA(ULONG,                    type,      D1),
    AROS_LHA(APTR,                     info,      A0),
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    struct IEAudioBase *, IEAudioBase, 16, IEAudio)
{
    AROS_LIBFUNC_INIT
    return AHIS_UNKNOWN;
    AROS_LIBFUNC_EXIT
}


/******************************************************************************
** AHIsub_UnloadSound (stub) *************************************************
******************************************************************************/

AROS_LH2(void, AHIsub_UnloadSound,
    AROS_LHA(UWORD,                    sound,     D0),
    AROS_LHA(struct AHIAudioCtrlDrv *, AudioCtrl, A2),
    struct IEAudioBase *, IEAudioBase, 17, IEAudio)
{
    AROS_LIBFUNC_INIT
    /* Nothing to unload */
    AROS_LIBFUNC_EXIT
}

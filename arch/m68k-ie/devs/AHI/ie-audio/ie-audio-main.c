/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE AHI audio driver — main entry points.
          Based on the Void driver pattern.  Writes mixed AHI output
          to IE flex channel DAC registers for real audio playback.
*/

#include <devices/ahi.h>
#include <dos/dostags.h>
#include <exec/memory.h>
#include <libraries/ahi_sub.h>
#include <proto/ahi_sub.h>
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

ULONG
_AHIsub_AllocAudio(struct TagItem         *taglist,
                   struct AHIAudioCtrlDrv *AudioCtrl,
                   struct DriverBase      *AHIsubBase)
{
    AudioCtrl->ahiac_DriverData = AllocVec(sizeof(struct IEAudioData),
                                           MEMF_CLEAR | MEMF_PUBLIC);

    if (dd != NULL) {
        dd->slavesignal  = -1;
        dd->mastersignal = AllocSignal(-1);
        dd->mastertask   = (struct Process *)FindTask(NULL);
        dd->ahisubbase   = (struct IEAudioBase *)AHIsubBase;
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
}


/******************************************************************************
** AHIsub_FreeAudio ***********************************************************
******************************************************************************/

void
_AHIsub_FreeAudio(struct AHIAudioCtrlDrv *AudioCtrl,
                  struct DriverBase      *AHIsubBase)
{
    if (AudioCtrl->ahiac_DriverData != NULL) {
        FreeSignal(dd->mastersignal);
        FreeVec(AudioCtrl->ahiac_DriverData);
        AudioCtrl->ahiac_DriverData = NULL;
    }
}


/******************************************************************************
** AHIsub_Disable *************************************************************
******************************************************************************/

void
_AHIsub_Disable(struct AHIAudioCtrlDrv *AudioCtrl,
                struct DriverBase      *AHIsubBase)
{
    Forbid();
}


/******************************************************************************
** AHIsub_Enable **************************************************************
******************************************************************************/

void
_AHIsub_Enable(struct AHIAudioCtrlDrv *AudioCtrl,
               struct DriverBase      *AHIsubBase)
{
    Permit();
}


/******************************************************************************
** AHIsub_Start ***************************************************************
******************************************************************************/

ULONG
_AHIsub_Start(ULONG                   flags,
              struct AHIAudioCtrlDrv *AudioCtrl,
              struct DriverBase      *AHIsubBase)
{
    AHIsub_Stop(flags, AudioCtrl);

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
}


/******************************************************************************
** AHIsub_Update **************************************************************
******************************************************************************/

void
_AHIsub_Update(ULONG                   flags,
               struct AHIAudioCtrlDrv *AudioCtrl,
               struct DriverBase      *AHIsubBase)
{
    /* Empty — no hardware state to update between mixes */
}


/******************************************************************************
** AHIsub_Stop ****************************************************************
******************************************************************************/

void
_AHIsub_Stop(ULONG                   flags,
             struct AHIAudioCtrlDrv *AudioCtrl,
             struct DriverBase      *AHIsubBase)
{
    if (flags & AHISF_PLAY) {
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
}


/******************************************************************************
** AHIsub_GetAttr *************************************************************
******************************************************************************/

IPTR
_AHIsub_GetAttr(ULONG                   attribute,
                LONG                    argument,
                IPTR                    def,
                struct TagItem         *taglist,
                struct AHIAudioCtrlDrv *AudioCtrl,
                struct DriverBase      *AHIsubBase)
{
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
}


/******************************************************************************
** AHIsub_HardwareControl *****************************************************
******************************************************************************/

ULONG
_AHIsub_HardwareControl(ULONG                   attribute,
                        LONG                    argument,
                        struct AHIAudioCtrlDrv *AudioCtrl,
                        struct DriverBase      *AHIsubBase)
{
    return 0;
}

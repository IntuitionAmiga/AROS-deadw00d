/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE AHI audio driver — slave process.
          The slave process calls the AHI mixer at the system sample rate,
          then writes the mixed output to IE flex channel DAC registers.
*/

#include <devices/ahi.h>
#include <exec/execbase.h>
#include <libraries/ahi_sub.h>

#include "DriverData.h"

#include <aros/asmcall.h>
#include <proto/exec.h>

#define dd ((struct IEAudioData*) AudioCtrl->ahiac_DriverData)

/* IE flex channel DAC registers */
#define IE_FLEX_CH0_DAC   (0xF0A80 + 0x3C)
#define IE_FLEX_CH1_DAC   (0xF0AC0 + 0x3C)
#define IE_FLEX_CH0_VOL   (0xF0A80 + 0x04)
#define IE_FLEX_CH1_VOL   (0xF0AC0 + 0x04)
#define IE_FLEX_CH0_CTRL  (0xF0A80 + 0x08)
#define IE_FLEX_CH1_CTRL  (0xF0AC0 + 0x08)

static inline void ie_write32(unsigned long reg, unsigned long val)
{
    *(volatile unsigned long *)reg = val;
}

#undef SysBase

static void Slave(struct ExecBase *SysBase);

AROS_UFH3(void, SlaveEntry,
          AROS_UFHA(STRPTR, argPtr, A0),
          AROS_UFHA(ULONG, argSize, D0),
          AROS_UFHA(struct ExecBase *, SysBase, A6))
{
    AROS_USERFUNC_INIT
    Slave(SysBase);
    AROS_USERFUNC_EXIT
}

static void
Slave(struct ExecBase *SysBase)
{
    struct AHIAudioCtrlDrv *AudioCtrl;
    struct DriverBase      *AHIsubBase;
    BOOL                    running;
    ULONG                   signals;

    AudioCtrl  = (struct AHIAudioCtrlDrv *)FindTask(NULL)->tc_UserData;
    AHIsubBase = (struct DriverBase *)dd->ahisubbase;

    dd->slavesignal = AllocSignal(-1);

    if (dd->slavesignal != -1) {
        /* Enable flex channels for DAC output */
        ie_write32(IE_FLEX_CH0_VOL,  255);
        ie_write32(IE_FLEX_CH0_CTRL, 3);
        ie_write32(IE_FLEX_CH0_DAC,  0);
        ie_write32(IE_FLEX_CH1_VOL,  255);
        ie_write32(IE_FLEX_CH1_CTRL, 3);
        ie_write32(IE_FLEX_CH1_DAC,  0);

        /* Tell master we're alive */
        Signal((struct Task *)dd->mastertask,
               1L << dd->mastersignal);

        running = TRUE;

        while (running) {
            signals = SetSignal(0L, 0L);

            if (signals & (SIGBREAKF_CTRL_C | (1L << dd->slavesignal))) {
                running = FALSE;
            } else {
                /* Call AHI player hook (timing) and mixer (sample generation).
                 * The mixer fills dd->mixbuffer with ahiac_BuffSamples frames.
                 * We then write the first stereo sample pair to the flex DACs.
                 *
                 * This is a simplified output path — it feeds one mixed sample
                 * per AHI callback rather than streaming the full buffer.
                 * A production driver would use the audio DMA engine to stream
                 * the entire mix buffer at the correct rate.
                 */
                CallHookPkt(AudioCtrl->ahiac_PlayerFunc, AudioCtrl, NULL);
                CallHookPkt(AudioCtrl->ahiac_MixerFunc,  AudioCtrl, dd->mixbuffer);

                /* Write first mixed sample to DAC (16-bit signed → 8-bit signed).
                 * Buffer format depends on ahiac_BuffType:
                 *   AHIST_S16S: interleaved signed 16-bit stereo (L,R,L,R,...)
                 *   AHIST_M16S: signed 16-bit mono
                 */
                if (dd->mixbuffer && AudioCtrl->ahiac_BuffSamples > 0) {
                    WORD *buf16 = (WORD *)dd->mixbuffer;
                    BYTE left  = (BYTE)(buf16[0] >> 8);
                    BYTE right = (BYTE)(buf16[1] >> 8);
                    ie_write32(IE_FLEX_CH0_DAC, (ULONG)(UBYTE)left);
                    ie_write32(IE_FLEX_CH1_DAC, (ULONG)(UBYTE)right);
                }
            }
        }

        /* Disable flex channels */
        ie_write32(IE_FLEX_CH0_CTRL, 0);
        ie_write32(IE_FLEX_CH1_CTRL, 0);
    }

    FreeSignal(dd->slavesignal);
    dd->slavesignal = -1;

    Forbid();

    Signal((struct Task *)dd->mastertask,
           1L << dd->mastersignal);

    dd->slavetask = NULL;
}

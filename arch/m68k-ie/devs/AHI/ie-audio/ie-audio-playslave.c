/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE AHI audio driver — slave process.
          The slave process calls the AHI mixer at the system sample rate,
          then streams the mixed output to IE flex channel DAC registers.
          Uses IE64 coprocessor (WARP_OP_AUDIO_RESAMPLE) for sample rate
          conversion when the mixer output rate differs from the DAC rate.
*/

#include <devices/ahi.h>
#include <exec/execbase.h>
#include <libraries/ahi_sub.h>

#include "DriverData.h"

#include <aros/asmcall.h>
#include <proto/exec.h>
#include <proto/utility.h>

#include <ie_hwreg.h>
#include <libraries/iewarp.h>
static struct Library *IEWarpBase = NULL;
#include <iewarp_consumer.h>

#define dd ((struct IEAudioData*) AudioCtrl->ahiac_DriverData)

/* IE flex channel DAC registers */
#define IE_FLEX_CH0_DAC   (0xF0A80 + 0x3C)
#define IE_FLEX_CH1_DAC   (0xF0AC0 + 0x3C)
#define IE_FLEX_CH0_VOL   (0xF0A80 + 0x04)
#define IE_FLEX_CH1_VOL   (0xF0AC0 + 0x04)
#define IE_FLEX_CH0_CTRL  (0xF0A80 + 0x08)
#define IE_FLEX_CH1_CTRL  (0xF0AC0 + 0x08)

/* IE DAC native output rate */
#define IE_DAC_RATE 44100

/* Coprocessor dispatch threshold (samples) */
#define RESAMPLE_THRESHOLD 256

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

/*
 * Resample stereo int16 buffer from srcRate to dstRate using IE64 coprocessor.
 * Deinterleaves stereo (L,R,L,R...) into separate L/R mono buffers,
 * dispatches WARP_OP_AUDIO_RESAMPLE for each channel, then re-interleaves.
 * Returns the number of output samples, or 0 on failure (caller should fallback).
 */
static ULONG IE_ResampleStereo(struct ExecBase *SysBase,
                                WORD *src, ULONG srcSamples,
                                WORD *dst, ULONG dstSamples,
                                ULONG srcRate, ULONG dstRate)
{
    ULONG srcBytes = srcSamples * sizeof(WORD);
    ULONG dstBytes = dstSamples * sizeof(WORD);
    WORD *srcL, *srcR, *dstL, *dstR;
    ULONG i;

    srcL = (WORD *)AllocMem(srcBytes * 2 + dstBytes * 2, MEMF_ANY);
    if (!srcL)
        return 0;

    srcR = srcL + srcSamples;
    dstL = srcR + srcSamples;
    dstR = dstL + dstSamples;

    /* Deinterleave stereo → mono L/R */
    for (i = 0; i < srcSamples; i++)
    {
        srcL[i] = src[i * 2];
        srcR[i] = src[i * 2 + 1];
    }

    /* Dispatch audio resample via iewarp.library */
    if (!IEWARP_OPEN())
    {
        FreeMem(srcL, srcBytes * 2 + dstBytes * 2);
        return 0;
    }

    IEWarpSetCaller(IEWARP_CALLER_AHI);
    {
        ULONG ticketL = IEWarpAudioResample(
            (APTR)srcL, (APTR)dstL, srcSamples, srcRate, dstRate);
        ULONG ticketR;

        if (!ticketL)
        {
            FreeMem(srcL, srcBytes * 2 + dstBytes * 2);
            return 0;
        }

        ticketR = IEWarpAudioResample(
            (APTR)srcR, (APTR)dstR, srcSamples, srcRate, dstRate);

        if (!ticketR)
        {
            IEWarpWait(ticketL);
            FreeMem(srcL, srcBytes * 2 + dstBytes * 2);
            return 0;
        }

        /* Wait for both channels */
        IEWarpWait(ticketL);
        IEWarpWait(ticketR);

        /* Re-interleave mono L/R → stereo */
        for (i = 0; i < dstSamples; i++)
        {
            dst[i * 2]     = dstL[i];
            dst[i * 2 + 1] = dstR[i];
        }
    }

    FreeMem(srcL, srcBytes * 2 + dstBytes * 2);
    return dstSamples;
}

static void
Slave(struct ExecBase *SysBase)
{
    struct AHIAudioCtrlDrv *AudioCtrl;
    struct DriverBase      *AHIsubBase;
    BOOL                    running;
    ULONG                   signals;
    WORD                   *resampleBuf = NULL;
    ULONG                   resampleBufSamples = 0;

    AudioCtrl  = (struct AHIAudioCtrlDrv *)FindTask(NULL)->tc_UserData;
    AHIsubBase = (struct DriverBase *)dd->ahisubbase;

    dd->slavesignal = AllocSignal(-1);

    if (dd->slavesignal != -1) {
        ULONG mixRate = AudioCtrl->ahiac_MixFreq;
        BOOL needsResample = (mixRate != IE_DAC_RATE && mixRate > 0);

        /* Pre-allocate resample buffer if rate conversion needed */
        if (needsResample && AudioCtrl->ahiac_BuffSamples > 0)
        {
            /* Output samples = input samples * dstRate / srcRate + 1 */
            resampleBufSamples = (ULONG)AudioCtrl->ahiac_BuffSamples *
                                 IE_DAC_RATE / mixRate + 1;
            resampleBuf = (WORD *)AllocMem(
                resampleBufSamples * 2 * sizeof(WORD), MEMF_ANY);
            if (!resampleBuf)
                needsResample = FALSE;  /* fallback to direct output */
        }

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
                CallHookPkt(AudioCtrl->ahiac_PlayerFunc, AudioCtrl, NULL);
                CallHookPkt(AudioCtrl->ahiac_MixerFunc,  AudioCtrl, dd->mixbuffer);

                if (dd->mixbuffer && AudioCtrl->ahiac_BuffSamples > 0) {
                    WORD *outBuf = (WORD *)dd->mixbuffer;
                    ULONG outSamples = AudioCtrl->ahiac_BuffSamples;

                    /* Resample via IE64 coprocessor if rates differ */
                    if (needsResample &&
                        outSamples >= RESAMPLE_THRESHOLD &&
                        resampleBuf)
                    {
                        ULONG resampled = IE_ResampleStereo(
                            SysBase, (WORD *)dd->mixbuffer,
                            AudioCtrl->ahiac_BuffSamples,
                            resampleBuf, resampleBufSamples,
                            mixRate, IE_DAC_RATE);
                        if (resampled > 0)
                        {
                            outBuf = resampleBuf;
                            outSamples = resampled;
                        }
                    }

                    /* Stream all samples to DAC (16-bit signed → 8-bit) */
                    {
                        ULONG s;
                        for (s = 0; s < outSamples; s++)
                        {
                            BYTE left  = (BYTE)(outBuf[s * 2] >> 8);
                            BYTE right = (BYTE)(outBuf[s * 2 + 1] >> 8);
                            ie_write32(IE_FLEX_CH0_DAC, (ULONG)(UBYTE)left);
                            ie_write32(IE_FLEX_CH1_DAC, (ULONG)(UBYTE)right);
                        }
                    }
                }
            }
        }

        /* Disable flex channels */
        ie_write32(IE_FLEX_CH0_CTRL, 0);
        ie_write32(IE_FLEX_CH1_CTRL, 0);

        if (resampleBuf)
            FreeMem(resampleBuf, resampleBufSamples * 2 * sizeof(WORD));
    }

    FreeSignal(dd->slavesignal);
    dd->slavesignal = -1;

    Forbid();

    Signal((struct Task *)dd->mastertask,
           1L << dd->mastersignal);

    dd->slavetask = NULL;
}

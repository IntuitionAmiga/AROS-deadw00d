/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE AHI audio driver — slave process.
          The slave mixes AHI output and streams it through audio.device
          with double-buffered CMD_WRITE requests. audio.device owns
          the DMA channels, the interrupt vectors and the pacing, so the
          AHI driver touches no audio hardware registers and cannot
          conflict with other audio.device clients.
*/

#include <devices/ahi.h>
#include <devices/audio.h>
#include <exec/execbase.h>
#include <clib/alib_protos.h>
#include <libraries/ahi_sub.h>

#include "DriverData.h"

#include <aros/asmcall.h>
#include <proto/exec.h>
#include <proto/utility.h>

#define dd ((struct IEAudioData*) AudioCtrl->ahiac_DriverData)

/* Paula period for the fixed 44100 Hz driver rate */
#define PAULA_CLOCK_PAL   3546895
#define IE_DAC_RATE       44100
#define IE_AHI_PERIOD     (PAULA_CLOCK_PAL / IE_DAC_RATE)

/* Channel side masks: Paula channels 0 and 3 are left, 1 and 2 right */
#define LEFT_MASK         0x09
#define RIGHT_MASK        0x06

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

/* Deinterleave one mixed 16-bit stereo block into two 8-bit mono
 * channel buffers (Paula sample format). */
static void convert_block(WORD *buf16, ULONG samples, BYTE *left, BYTE *right)
{
    ULONG s;
    for (s = 0; s < samples; s++)
    {
        left[s]  = (BYTE)(buf16[s * 2] >> 8);
        right[s] = (BYTE)(buf16[s * 2 + 1] >> 8);
    }
}

static void setup_write(struct IOAudio *io, struct IOAudio *tmpl,
                        ULONG unitmask, struct MsgPort *port,
                        BYTE *buf, ULONG bytes)
{
    *io = *tmpl;
    io->ioa_Request.io_Message.mn_ReplyPort = port;
    io->ioa_Request.io_Command = CMD_WRITE;
    io->ioa_Request.io_Flags   = ADIOF_PERVOL;
    io->ioa_Request.io_Unit    = (struct Unit *)unitmask;
    io->ioa_Data               = (UBYTE *)buf;
    io->ioa_Length             = bytes & ~1UL;
    io->ioa_Period             = IE_AHI_PERIOD;
    io->ioa_Volume             = 64;
    io->ioa_Cycles             = 1;
}

static void
Slave(struct ExecBase *SysBase)
{
    struct AHIAudioCtrlDrv *AudioCtrl;
    struct DriverBase      *AHIsubBase;
    BOOL                    running;
    ULONG                   signals;
    struct MsgPort         *port = NULL;
    struct IOAudio          ioa;          /* OpenDevice + allocation */
    struct IOAudio          req[2][2];    /* [side L=0,R=1][index]   */
    BYTE                   *chanbuf = NULL;
    ULONG                   chanbufsamples = 0;
    BOOL                    devopen = FALSE;
    BOOL                    startupfailed = FALSE;
    static UBYTE            allocmasks[] = { 0x03, 0x05, 0x0A, 0x0C };

    AudioCtrl  = (struct AHIAudioCtrlDrv *)FindTask(NULL)->tc_UserData;
    AHIsubBase = (struct DriverBase *)dd->ahisubbase;

    dd->slavesignal = AllocSignal(-1);

    chanbufsamples = AudioCtrl->ahiac_MaxBuffSamples;
    if (chanbufsamples == 0)
        chanbufsamples = AudioCtrl->ahiac_BuffSamples;
    if (chanbufsamples > 0)
        chanbuf = AllocMem(chanbufsamples * 4, MEMF_PUBLIC | MEMF_CLEAR);

    port = CreateMsgPort();

    if (port && chanbuf && dd->slavesignal != -1)
    {
        /* Open audio.device, allocating one stereo pair (one left and
         * one right channel) at open time. */
        ioa.ioa_Request.io_Message.mn_ReplyPort   = port;
        ioa.ioa_Request.io_Message.mn_Node.ln_Pri = 64;
        ioa.ioa_Request.io_Flags                  = 0;
        ioa.ioa_AllocKey                          = 0;
        ioa.ioa_Data                              = allocmasks;
        ioa.ioa_Length                            = sizeof(allocmasks);

        if (OpenDevice("audio.device", 0,
                       (struct IORequest *)&ioa, 0) == 0)
            devopen = TRUE;
    }

    if (devopen)
    {
        ULONG unitmask  = (ULONG)ioa.ioa_Request.io_Unit;
        ULONG leftmask  = unitmask & LEFT_MASK;
        ULONG rightmask = unitmask & RIGHT_MASK;
        BYTE *bufL[2], *bufR[2];
        BOOL  back[2][2];   /* request returned and awaiting refill */
        ULONG portsig = 1UL << port->mp_SigBit;
        ULONG idx;

        bufL[0] = chanbuf;
        bufL[1] = chanbuf + chanbufsamples;
        bufR[0] = chanbuf + chanbufsamples * 2;
        bufR[1] = chanbuf + chanbufsamples * 3;

        /* Tell master we're alive */
        dd->slavefailed = FALSE;
        Signal((struct Task *)dd->mastertask,
               1L << dd->mastersignal);

        /* Prime both buffer pairs: audio.device starts DMA on the
         * first queued write and latches the second for a seamless
         * double-buffered loop. */
        for (idx = 0; idx < 2; idx++)
        {
            ULONG samples;

            CallHookPkt(AudioCtrl->ahiac_PlayerFunc, AudioCtrl, NULL);
            CallHookPkt(AudioCtrl->ahiac_MixerFunc,  AudioCtrl, dd->mixbuffer);

            samples = AudioCtrl->ahiac_BuffSamples;
            if (samples > chanbufsamples)
                samples = chanbufsamples;

            convert_block((WORD *)dd->mixbuffer, samples,
                          bufL[idx], bufR[idx]);

            setup_write(&req[0][idx], &ioa, leftmask, port,
                        bufL[idx], samples);
            setup_write(&req[1][idx], &ioa, rightmask, port,
                        bufR[idx], samples);
            BeginIO((struct IORequest *)&req[0][idx]);
            BeginIO((struct IORequest *)&req[1][idx]);
            back[0][idx] = back[1][idx] = FALSE;
        }

        running = TRUE;

        while (running)
        {
            struct Message *m;

            signals = Wait(portsig |
                           (1UL << dd->slavesignal) |
                           SIGBREAKF_CTRL_C);

            if (signals & (SIGBREAKF_CTRL_C | (1UL << dd->slavesignal)))
            {
                running = FALSE;
                break;
            }

            while ((m = GetMsg(port)) != NULL)
            {
                ULONG side, i;
                for (side = 0; side < 2; side++)
                    for (i = 0; i < 2; i++)
                        if (m == &req[side][i].ioa_Request.io_Message)
                            back[side][i] = TRUE;
            }

            /* When both sides of a block index are back, mix the next
             * block into it and requeue. audio.device's second queued
             * write keeps playing meanwhile. */
            for (idx = 0; idx < 2; idx++)
            {
                if (back[0][idx] && back[1][idx])
                {
                    ULONG samples;

                    CallHookPkt(AudioCtrl->ahiac_PlayerFunc, AudioCtrl, NULL);
                    CallHookPkt(AudioCtrl->ahiac_MixerFunc,  AudioCtrl,
                                dd->mixbuffer);

                    samples = AudioCtrl->ahiac_BuffSamples;
                    if (samples > chanbufsamples)
                        samples = chanbufsamples;

                    convert_block((WORD *)dd->mixbuffer, samples,
                                  bufL[idx], bufR[idx]);

                    req[0][idx].ioa_Length = samples & ~1UL;
                    req[1][idx].ioa_Length = samples & ~1UL;
                    BeginIO((struct IORequest *)&req[0][idx]);
                    BeginIO((struct IORequest *)&req[1][idx]);
                    back[0][idx] = back[1][idx] = FALSE;
                }
            }
        }

        /* Drain in-flight requests before closing */
        {
            ULONG side, i;
            for (side = 0; side < 2; side++)
            {
                for (i = 0; i < 2; i++)
                {
                    if (!back[side][i])
                    {
                        AbortIO((struct IORequest *)&req[side][i]);
                        WaitIO((struct IORequest *)&req[side][i]);
                    }
                }
            }
        }

        CloseDevice((struct IORequest *)&ioa);
    }
    else
    {
        /* Report startup failure only after cleanup has completed. */
        startupfailed = TRUE;
    }

    if (port)
        DeleteMsgPort(port);
    if (chanbuf)
        FreeMem(chanbuf, chanbufsamples * 4);
    if (dd->slavesignal != -1)
        FreeSignal(dd->slavesignal);
    dd->slavesignal = -1;

    Forbid();
    if (startupfailed)
        dd->slavefailed = TRUE;
    dd->slavetask = NULL;
    Signal((struct Task *)dd->mastertask,
           1L << dd->mastersignal);
    Permit();
}

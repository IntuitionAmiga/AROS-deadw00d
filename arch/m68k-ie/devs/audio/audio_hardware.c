/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE audio.device — hardware interface layer.
          Maps Paula audio DMA operations to IE MMIO registers.
          Instead of writing to Paula custom chip at 0xDFF000, we write
          to the IE Audio DMA registers at 0xF2260–0xF22AF.

          The Go-side ArosAudioDMA engine reads sample bytes from guest
          memory at 44100 Hz and writes them to flex channel DAC registers.
          When a buffer is exhausted it sets a completion flag and triggers
          a level-3 M68K interrupt.
*/

#define DEBUG 0
#include <aros/debug.h>

#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/alerts.h>
#include <exec/tasks.h>
#include <exec/interrupts.h>
#include <devices/audio.h>
#include <hardware/intbits.h>

#include <proto/exec.h>

#include "audio_intern.h"

/* IE Audio DMA MMIO register addresses */
#define IE_AUD_BASE       0xF2260
#define IE_AUD_CH_STRIDE  16

#define IE_AUD_OFF_PTR    0x00
#define IE_AUD_OFF_LEN    0x04
#define IE_AUD_OFF_PER    0x08
#define IE_AUD_OFF_VOL    0x0C

#define IE_AUD_DMACON     0xF22A0
#define IE_AUD_STATUS     0xF22A4
#define IE_AUD_INTENA     0xF22A8

/* Amiga-style set/clear bit for DMACON/INTENA writes */
#define SETCLR            0x8000

static inline void ie_aud_write(ULONG reg, ULONG val)
{
    *(volatile ULONG *)reg = val;
}

static inline ULONG ie_aud_read(ULONG reg)
{
    return *(volatile ULONG *)reg;
}

static inline ULONG ie_aud_ch_reg(UBYTE ch, ULONG offset)
{
    return IE_AUD_BASE + (ULONG)ch * IE_AUD_CH_STRIDE + offset;
}

void audiohw_stop(struct AudioBase *ab, UWORD mask)
{
    if (!mask)
        return;

    /* Clear DMA enable bits */
    ie_aud_write(IE_AUD_DMACON, mask & CH_MASK);

    /* Disable interrupts for these channels */
    ie_aud_write(IE_AUD_INTENA, mask & CH_MASK);
}

void audiohw_prepareptlen(struct AudioBase *ab, struct IOAudio *io, UBYTE ch)
{
    if (ab->cycles[ch] != 1)
        return;

    if (io) {
        ie_aud_write(ie_aud_ch_reg(ch, IE_AUD_OFF_PTR), (ULONG)io->ioa_Data);
        ie_aud_write(ie_aud_ch_reg(ch, IE_AUD_OFF_LEN), io->ioa_Length / 2);
        ab->cycles[ch] = io->ioa_Cycles;
        D(bug("ie_aud: ch%d pt=%08lx len=%ld cyc=%d\n",
              ch, (ULONG)io->ioa_Data, io->ioa_Length / 2, io->ioa_Cycles));
    } else {
        /* Point to silence buffer */
        ie_aud_write(ie_aud_ch_reg(ch, IE_AUD_OFF_PTR), (ULONG)ab->zerosample);
        ie_aud_write(ie_aud_ch_reg(ch, IE_AUD_OFF_LEN), 1);
        D(bug("ie_aud: ch%d null\n", ch));
    }
    ab->initialcyclemask &= ~(1 << ch);
}

void audiohw_preparepervol(struct AudioBase *ab, struct IOAudio *io, UBYTE ch)
{
    if (io && (io->ioa_Request.io_Flags & ADIOF_PERVOL)) {
        ie_aud_write(ie_aud_ch_reg(ch, IE_AUD_OFF_PER), io->ioa_Period);
        ie_aud_write(ie_aud_ch_reg(ch, IE_AUD_OFF_VOL), io->ioa_Volume);
        D(bug("ie_aud: ch%d per=%d vol=%d\n", ch, io->ioa_Period, io->ioa_Volume));
    }
}

/*
 * Audio interrupt handler — called by the level-3 autovector dispatcher
 * when the Go-side DMA engine signals buffer completion.
 */
static void audioirq(struct AudioBase *ab, UBYTE ch)
{
    struct IOAudio *io = getnextwrite(ab, ch, FALSE);
    UBYTE mask = 1 << ch;

    D(bug("ie_aud: ch%d irq io=%p init=%04x dma=%04x stop=%04x cyc=%d\n",
          ch, io, ab->initialcyclemask, ab->initialdmamask,
          ab->stopmask, ab->cycles[ch]));

    if (!io || (ab->stopmask & mask)) {
        audiohw_stop(ab, mask);
        D(bug("ie_aud: ch%d finished\n", ch));
        return;
    }

    if (!(ab->initialdmamask & mask)) {
        /* Startup interrupt — first buffer loaded into DMA */
        D(bug("ie_aud: ch%d startup\n", ch));
        ab->initialdmamask |= mask;
        if (io->ioa_Request.io_Flags & ADIOF_WRITEMESSAGE)
            ReplyMsg(&io->ioa_WriteMsg);
        io = getnextwrite(ab, ch, TRUE);
        audiohw_prepareptlen(ab, io, ch);
    } else {
        struct IOAudio *wio, *next;
        struct IOAudio *io2 = getnextwrite(ab, ch, TRUE);

        if (!(ab->initialcyclemask & mask)) {
            ab->initialcyclemask |= mask;
            audiohw_preparepervol(ab, io2, ch);
            if (io2 && (io2->ioa_Request.io_Flags & ADIOF_WRITEMESSAGE))
                ReplyMsg(&io2->ioa_WriteMsg);
        }

        if (ab->cycles[ch] == 1) {
            REMOVE(io);
            ReplyMsg(&io->ioa_Request.io_Message);
            io = getnextwrite(ab, ch, TRUE);
            D(bug("ie_aud: ch%d next=%p\n", ch, io));
            audiohw_prepareptlen(ab, io, ch);
        } else if (ab->cycles[ch]) {
            ab->cycles[ch]--;
        }

        /* Handle SYNCCYCLE requests */
        ForeachNodeSafe(&ab->misclist, wio, next) {
            UWORD cmd = wio->ioa_Request.io_Command;
            UBYTE cmask = (UBYTE)(ULONG)wio->ioa_Request.io_Unit;
            if (cmd != ADCMD_PERVOL && cmd != ADCMD_FINISH && cmd != ADCMD_WAITCYCLE)
                continue;
            if (!(cmask & mask))
                continue;
            if (cmask & (mask << NR_CH)) {
                if (cmd == ADCMD_PERVOL)
                    audiohw_preparepervol(ab, wio, ch);
            }
            cmask &= ~(mask << NR_CH);
            if ((cmask >> NR_CH) == 0) {
                D(bug("ie_aud: ch%d SYNCCYCLE woken, io=%p\n", ch, wio));
                REMOVE(wio);
                ReplyMsg(&wio->ioa_Request.io_Message);
            }
        }
    }
}

/*
 * Interrupt server — installed with SetIntVector(INTB_AUD0+ch).
 * On the IE, the level-3 kernel handler checks IE_AUD_STATUS and
 * calls core_Cause(INTB_AUD0+ch) for each completed channel.
 */
AROS_INTH1(audio_int, struct AudioInterrupt *, ai)
{
    AROS_INTFUNC_INIT

    audioirq(ai->ab, ai->ch);

    return 0;

    AROS_INTFUNC_EXIT
}

static void preparech_initial(struct AudioBase *ab, UBYTE ch)
{
    struct IOAudio *io = getnextwrite(ab, ch, FALSE);
    ab->cycles[ch] = 1;
    audiohw_prepareptlen(ab, io, ch);
    audiohw_preparepervol(ab, io, ch);
    ab->initialdmamask &= ~(1 << ch);
    ab->initialcyclemask |= 1 << ch;
}

void audiohw_start(struct AudioBase *ab, UWORD mask)
{
    UWORD hwmask;
    UBYTE ch;

    if (!mask)
        return;

    hwmask = 0;
    for (ch = 0; ch < NR_CH; ch++) {
        if ((mask & (1 << ch)) && getnextwrite(ab, ch, FALSE)) {
            hwmask |= 1 << ch;
            preparech_initial(ab, ch);
        }
    }
    D(bug("ie_aud: hw_start %02x\n", hwmask));
    if (hwmask) {
        /* Clear any pending status, enable interrupts, start DMA */
        ie_aud_write(IE_AUD_STATUS, hwmask);
        ie_aud_write(IE_AUD_INTENA, SETCLR | hwmask);
        ie_aud_write(IE_AUD_DMACON, SETCLR | hwmask);
    }
}

void audiohw_reset(struct AudioBase *ab, UWORD mask)
{
    UBYTE ch;

    /* Stop DMA and disable interrupts */
    ie_aud_write(IE_AUD_DMACON, mask & CH_MASK);
    ie_aud_write(IE_AUD_INTENA, mask & CH_MASK);
    ie_aud_write(IE_AUD_STATUS, mask & CH_MASK);

    for (ch = 0; ch < NR_CH; ch++) {
        if ((1 << ch) & mask) {
            ie_aud_write(ie_aud_ch_reg(ch, IE_AUD_OFF_VOL), 0);
            ie_aud_write(ie_aud_ch_reg(ch, IE_AUD_OFF_PER), 100);
        }
    }
}

void audiohw_init(struct AudioBase *ab)
{
    UBYTE ch;

    audiohw_reset(ab, CH_MASK);

    for (ch = 0; ch < NR_CH; ch++) {
        struct AudioInterrupt *inter = &ab->audint[ch];
        inter->audint.is_Code = (APTR)audio_int;
        inter->audint.is_Data = inter;
        inter->audint.is_Node.ln_Name = "audio";
        inter->audint.is_Node.ln_Type = NT_INTERRUPT;
        inter->ch = ch;
        inter->ab = ab;
        SetIntVector(INTB_AUD0 + ch, &inter->audint);
    }
}

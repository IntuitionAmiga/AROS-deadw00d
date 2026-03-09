/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: iewarp.library per-operation wrappers with threshold-based fallback
*/

#include <proto/exec.h>
#include "iewarp_intern.h"

/* ── Memory operations ─────────────────────────────────────────── */

AROS_LH3(ULONG, IEWarpMemCpy,
    AROS_LHA(APTR,  dst, A0),
    AROS_LHA(APTR,  src, A1),
    AROS_LHA(ULONG, len, D0),
    struct IEWarpBase *, IEWarpBase, 6, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (!IEWarpBase->workerRunning || len < IEWarpBase->threshold)
    {
        CopyMem(src, dst, len);
        ie_warp_track_fallback(IEWarpBase, WARP_OP_MEMCPY, len);
        return 0;
    }

    return ie_warp_dispatch(IEWarpBase, WARP_OP_MEMCPY,
                            src, len, dst, len);

    AROS_LIBFUNC_EXIT
}

AROS_LH3(ULONG, IEWarpMemCpyQuick,
    AROS_LHA(APTR,  dst, A0),
    AROS_LHA(APTR,  src, A1),
    AROS_LHA(ULONG, len, D0),
    struct IEWarpBase *, IEWarpBase, 7, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (!IEWarpBase->workerRunning || len < IEWarpBase->threshold)
    {
        CopyMemQuick(src, dst, len);
        ie_warp_track_fallback(IEWarpBase, WARP_OP_MEMCPY_QUICK, len);
        return 0;
    }

    return ie_warp_dispatch(IEWarpBase, WARP_OP_MEMCPY_QUICK,
                            src, len, dst, len);

    AROS_LIBFUNC_EXIT
}

AROS_LH3(ULONG, IEWarpMemSet,
    AROS_LHA(APTR,  dst, A0),
    AROS_LHA(ULONG, val, D0),
    AROS_LHA(ULONG, len, D1),
    struct IEWarpBase *, IEWarpBase, 8, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (!IEWarpBase->workerRunning || len < IEWarpBase->threshold)
    {
        /* M68K fallback — byte fill */
        UBYTE *p = (UBYTE *)dst;
        UBYTE v = (UBYTE)val;
        ULONG i;
        for (i = 0; i < len; i++)
            p[i] = v;
        ie_warp_track_fallback(IEWarpBase, WARP_OP_MEMSET, len);
        return 0;
    }

    /* Pack val into reqBuf pointer field (op reads it from MMIO) */
    return ie_warp_dispatch(IEWarpBase, WARP_OP_MEMSET,
                            (APTR)(IPTR)val, len, dst, len);

    AROS_LIBFUNC_EXIT
}

AROS_LH3(ULONG, IEWarpMemMove,
    AROS_LHA(APTR,  dst, A0),
    AROS_LHA(APTR,  src, A1),
    AROS_LHA(ULONG, len, D0),
    struct IEWarpBase *, IEWarpBase, 9, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (!IEWarpBase->workerRunning || len < IEWarpBase->threshold)
    {
        /* M68K fallback — handle overlapping regions */
        UBYTE *d = (UBYTE *)dst;
        UBYTE *s = (UBYTE *)src;
        ULONG i;

        if (d < s || d >= s + len)
        {
            for (i = 0; i < len; i++)
                d[i] = s[i];
        }
        else
        {
            for (i = len; i > 0; i--)
                d[i - 1] = s[i - 1];
        }
        ie_warp_track_fallback(IEWarpBase, WARP_OP_MEMMOVE, len);
        return 0;
    }

    return ie_warp_dispatch(IEWarpBase, WARP_OP_MEMMOVE,
                            src, len, dst, len);

    AROS_LIBFUNC_EXIT
}

/* ── Graphics operations ───────────────────────────────────────── */

AROS_LH7(ULONG, IEWarpBlitCopy,
    AROS_LHA(APTR,  src,       A0),
    AROS_LHA(APTR,  dst,       A1),
    AROS_LHA(UWORD, width,     D0),
    AROS_LHA(UWORD, height,    D1),
    AROS_LHA(UWORD, srcStride, D2),
    AROS_LHA(UWORD, dstStride, D3),
    AROS_LHA(UBYTE, minterm,   D4),
    struct IEWarpBase *, IEWarpBase, 10, IEWarp)
{
    AROS_LIBFUNC_INIT

    ULONG totalBytes = (ULONG)height * (ULONG)srcStride;

    /* Only straight copy (minterm 0xC0) is supported.
     * Other minterms (AND, OR, XOR, etc.) are not implemented
     * in the worker — return 0 so the caller uses its own path. */
    if (minterm != 0xC0)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_BLIT_COPY, (ULONG)height * (ULONG)srcStride);
        return 0;
    }

    if (!IEWarpBase->workerRunning || totalBytes < IEWarpBase->threshold)
    {
        /* M68K fallback — straight copy */
        UBYTE *s = (UBYTE *)src;
        UBYTE *d = (UBYTE *)dst;
        UWORD y;
        for (y = 0; y < height; y++)
        {
            CopyMem(s, d, width);
            s += srcStride;
            d += dstStride;
        }
        ie_warp_track_fallback(IEWarpBase, WARP_OP_BLIT_COPY, totalBytes);
        return 0;
    }

    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_OP, WARP_OP_BLIT_COPY);
    ie_write32(IE_COPROC_REQ_PTR, (ULONG)src);
    ie_write32(IE_COPROC_REQ_LEN,
               ((ULONG)width) | ((ULONG)height << 16));
    ie_write32(IE_COPROC_RESP_PTR, (ULONG)dst);
    ie_write32(IE_COPROC_RESP_CAP,
               ((ULONG)srcStride) | ((ULONG)dstStride << 16));
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

    if (ie_read32(IE_COPROC_CMD_STATUS) != 0)
    {
        ie_warp_track_error(IEWarpBase, 0);
        return 0;
    }

    {
        ULONG ticket = ie_read32(IE_COPROC_TICKET);
        ie_warp_track_accel(IEWarpBase, WARP_OP_BLIT_COPY, totalBytes);
        if (IEWarpBase->batchMode)
            IEWarpBase->lastTicket = ticket;
        return ticket;
    }

    AROS_LIBFUNC_EXIT
}

AROS_LH7(ULONG, IEWarpBlitMask,
    AROS_LHA(APTR,  src,       A0),
    AROS_LHA(APTR,  dst,       A1),
    AROS_LHA(APTR,  mask,      A2),
    AROS_LHA(UWORD, width,     D0),
    AROS_LHA(UWORD, height,    D1),
    AROS_LHA(UWORD, srcStride, D2),
    AROS_LHA(UWORD, dstStride, D3),
    struct IEWarpBase *, IEWarpBase, 11, IEWarp)
{
    AROS_LIBFUNC_INIT

    ULONG totalBytes = (ULONG)height * (ULONG)srcStride;

    if (!IEWarpBase->workerRunning || totalBytes < IEWarpBase->threshold)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_BLIT_MASK, totalBytes);
        return 0; /* No simple M68K fallback for masked blit */
    }

    /* Manual MMIO: mask pointer goes to TIMEOUT register (extra param slot) */
    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_OP, WARP_OP_BLIT_MASK);
    ie_write32(IE_COPROC_REQ_PTR, (ULONG)src);
    ie_write32(IE_COPROC_REQ_LEN,
               ((ULONG)width) | ((ULONG)height << 16));
    ie_write32(IE_COPROC_RESP_PTR, (ULONG)dst);
    ie_write32(IE_COPROC_RESP_CAP,
               ((ULONG)srcStride) | ((ULONG)dstStride << 16));
    ie_write32(IE_COPROC_TIMEOUT, (ULONG)mask);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

    if (ie_read32(IE_COPROC_CMD_STATUS) != 0)
    {
        ie_warp_track_error(IEWarpBase, 0);
        return 0;
    }

    {
        ULONG ticket = ie_read32(IE_COPROC_TICKET);
        ie_warp_track_accel(IEWarpBase, WARP_OP_BLIT_MASK, totalBytes);
        if (IEWarpBase->batchMode)
            IEWarpBase->lastTicket = ticket;
        return ticket;
    }

    AROS_LIBFUNC_EXIT
}

AROS_LH8(ULONG, IEWarpBlitScale,
    AROS_LHA(APTR,  src,       A0),
    AROS_LHA(APTR,  dst,       A1),
    AROS_LHA(UWORD, srcW,      D0),
    AROS_LHA(UWORD, srcH,      D1),
    AROS_LHA(UWORD, dstW,      D2),
    AROS_LHA(UWORD, dstH,      D3),
    AROS_LHA(UWORD, srcStride, D4),
    AROS_LHA(UWORD, dstStride, D5),
    struct IEWarpBase *, IEWarpBase, 12, IEWarp)
{
    AROS_LIBFUNC_INIT

    ULONG totalBytes = (ULONG)srcH * (ULONG)srcStride;

    if (!IEWarpBase->workerRunning || totalBytes < IEWarpBase->threshold)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_BLIT_SCALE, totalBytes);
        return 0;
    }

    /* Manual MMIO: strides go to TIMEOUT register (extra param slot) */
    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_OP, WARP_OP_BLIT_SCALE);
    ie_write32(IE_COPROC_REQ_PTR, (ULONG)src);
    ie_write32(IE_COPROC_REQ_LEN,
               ((ULONG)srcW) | ((ULONG)srcH << 16));
    ie_write32(IE_COPROC_RESP_PTR, (ULONG)dst);
    ie_write32(IE_COPROC_RESP_CAP,
               ((ULONG)dstW) | ((ULONG)dstH << 16));
    ie_write32(IE_COPROC_TIMEOUT,
               ((ULONG)srcStride) | ((ULONG)dstStride << 16));
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

    if (ie_read32(IE_COPROC_CMD_STATUS) != 0)
    {
        ie_warp_track_error(IEWarpBase, 0);
        return 0;
    }

    {
        ULONG ticket = ie_read32(IE_COPROC_TICKET);
        ie_warp_track_accel(IEWarpBase, WARP_OP_BLIT_SCALE, totalBytes);
        if (IEWarpBase->batchMode)
            IEWarpBase->lastTicket = ticket;
        return ticket;
    }

    AROS_LIBFUNC_EXIT
}

AROS_LH6(ULONG, IEWarpBlitConvert,
    AROS_LHA(APTR,  src,    A0),
    AROS_LHA(APTR,  dst,    A1),
    AROS_LHA(UWORD, width,  D0),
    AROS_LHA(UWORD, height, D1),
    AROS_LHA(UWORD, srcFmt, D2),
    AROS_LHA(UWORD, dstFmt, D3),
    struct IEWarpBase *, IEWarpBase, 13, IEWarp)
{
    AROS_LIBFUNC_INIT

    ULONG totalBytes = (ULONG)width * (ULONG)height;

    if (!IEWarpBase->workerRunning || totalBytes < IEWarpBase->threshold)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_BLIT_CONVERT, totalBytes);
        return 0;
    }

    return ie_warp_dispatch(IEWarpBase, WARP_OP_BLIT_CONVERT,
                            src, ((ULONG)width) | ((ULONG)height << 16),
                            dst, ((ULONG)srcFmt) | ((ULONG)dstFmt << 16));

    AROS_LIBFUNC_EXIT
}

AROS_LH6(ULONG, IEWarpBlitAlpha,
    AROS_LHA(APTR,  src,       A0),
    AROS_LHA(APTR,  dst,       A1),
    AROS_LHA(UWORD, width,     D0),
    AROS_LHA(UWORD, height,    D1),
    AROS_LHA(UWORD, srcStride, D2),
    AROS_LHA(UWORD, dstStride, D3),
    struct IEWarpBase *, IEWarpBase, 14, IEWarp)
{
    AROS_LIBFUNC_INIT

    ULONG totalBytes = (ULONG)height * (ULONG)srcStride;

    if (!IEWarpBase->workerRunning || totalBytes < IEWarpBase->threshold)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_BLIT_ALPHA, totalBytes);
        return 0;
    }

    return ie_warp_dispatch(IEWarpBase, WARP_OP_BLIT_ALPHA,
                            src, ((ULONG)width) | ((ULONG)height << 16),
                            dst, ((ULONG)srcStride) | ((ULONG)dstStride << 16));

    AROS_LIBFUNC_EXIT
}

AROS_LH5(ULONG, IEWarpFillRect,
    AROS_LHA(APTR,  dst,    A0),
    AROS_LHA(UWORD, width,  D0),
    AROS_LHA(UWORD, height, D1),
    AROS_LHA(UWORD, stride, D2),
    AROS_LHA(ULONG, color,  D3),
    struct IEWarpBase *, IEWarpBase, 15, IEWarp)
{
    AROS_LIBFUNC_INIT

    ULONG totalBytes = (ULONG)height * (ULONG)stride;

    if (!IEWarpBase->workerRunning || totalBytes < IEWarpBase->threshold)
    {
        /* M68K fallback — fill each row */
        UBYTE *d = (UBYTE *)dst;
        UBYTE colorBytes[4];
        UWORD y, x;
        UWORD wholeWords = width / 4;
        UWORD tailBytes  = width & 3;

        colorBytes[0] = (UBYTE)(color);
        colorBytes[1] = (UBYTE)(color >> 8);
        colorBytes[2] = (UBYTE)(color >> 16);
        colorBytes[3] = (UBYTE)(color >> 24);

        for (y = 0; y < height; y++)
        {
            ULONG *row = (ULONG *)d;
            for (x = 0; x < wholeWords; x++)
                row[x] = color;
            for (x = 0; x < tailBytes; x++)
                d[wholeWords * 4 + x] = colorBytes[x];
            d += stride;
        }
        ie_warp_track_fallback(IEWarpBase, WARP_OP_FILL_RECT, totalBytes);
        return 0;
    }

    return ie_warp_dispatch(IEWarpBase, WARP_OP_FILL_RECT,
                            (APTR)(IPTR)color,
                            ((ULONG)width) | ((ULONG)height << 16),
                            dst, (ULONG)stride);

    AROS_LIBFUNC_EXIT
}

AROS_LH6(ULONG, IEWarpPixelProcess,
    AROS_LHA(APTR,  data,      A0),
    AROS_LHA(UWORD, width,     D0),
    AROS_LHA(UWORD, height,    D1),
    AROS_LHA(UWORD, stride,    D2),
    AROS_LHA(ULONG, operation, D3),
    AROS_LHA(ULONG, param,     D4),
    struct IEWarpBase *, IEWarpBase, 16, IEWarp)
{
    AROS_LIBFUNC_INIT

    ULONG totalBytes = (ULONG)height * (ULONG)stride;

    if (!IEWarpBase->workerRunning || totalBytes < IEWarpBase->threshold)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_PIXEL_PROCESS, totalBytes);
        return 0;
    }

    return ie_warp_dispatch(IEWarpBase, WARP_OP_PIXEL_PROCESS,
                            data,
                            ((ULONG)width) | ((ULONG)height << 16),
                            (APTR)(IPTR)operation,
                            ((ULONG)stride) | (param << 16));

    AROS_LIBFUNC_EXIT
}

/* ── Audio operations ──────────────────────────────────────────── */

AROS_LH5(ULONG, IEWarpAudioMix,
    AROS_LHA(APTR,  channels,    A0),
    AROS_LHA(UWORD, numChannels, D0),
    AROS_LHA(APTR,  dst,         A1),
    AROS_LHA(ULONG, numSamples,  D1),
    AROS_LHA(APTR,  volumes,     A2),
    struct IEWarpBase *, IEWarpBase, 17, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (!IEWarpBase->workerRunning || numSamples < IEWarpBase->threshold / 4)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_AUDIO_MIX, numSamples * 4);
        return 0;
    }

    /* Manual MMIO: volumes pointer goes to TIMEOUT register (extra param slot) */
    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_OP, WARP_OP_AUDIO_MIX);
    ie_write32(IE_COPROC_REQ_PTR, (ULONG)channels);
    ie_write32(IE_COPROC_REQ_LEN,
               ((ULONG)numChannels) | (numSamples << 16));
    ie_write32(IE_COPROC_RESP_PTR, (ULONG)dst);
    ie_write32(IE_COPROC_RESP_CAP, 0);
    ie_write32(IE_COPROC_TIMEOUT, (ULONG)volumes);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

    if (ie_read32(IE_COPROC_CMD_STATUS) != 0)
    {
        ie_warp_track_error(IEWarpBase, 0);
        return 0;
    }

    {
        ULONG ticket = ie_read32(IE_COPROC_TICKET);
        ie_warp_track_accel(IEWarpBase, WARP_OP_AUDIO_MIX, numSamples * 4);
        if (IEWarpBase->batchMode)
            IEWarpBase->lastTicket = ticket;
        return ticket;
    }

    AROS_LIBFUNC_EXIT
}

AROS_LH5(ULONG, IEWarpAudioResample,
    AROS_LHA(APTR,  src,        A0),
    AROS_LHA(APTR,  dst,        A1),
    AROS_LHA(ULONG, srcRate,    D0),
    AROS_LHA(ULONG, dstRate,    D1),
    AROS_LHA(ULONG, numSamples, D2),
    struct IEWarpBase *, IEWarpBase, 18, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (!IEWarpBase->workerRunning || numSamples < IEWarpBase->threshold / 4)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_AUDIO_RESAMPLE, numSamples * 4);
        return 0;
    }

    return ie_warp_dispatch(IEWarpBase, WARP_OP_AUDIO_RESAMPLE,
                            src, numSamples,
                            dst, (srcRate & 0xFFFF) | (dstRate << 16));

    AROS_LIBFUNC_EXIT
}

AROS_LH4(ULONG, IEWarpAudioDecode,
    AROS_LHA(APTR,  src,    A0),
    AROS_LHA(APTR,  dst,    A1),
    AROS_LHA(ULONG, srcLen, D0),
    AROS_LHA(ULONG, codec,  D1),
    struct IEWarpBase *, IEWarpBase, 19, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (!IEWarpBase->workerRunning || srcLen < IEWarpBase->threshold)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_AUDIO_DECODE, srcLen);
        return 0;
    }

    return ie_warp_dispatch(IEWarpBase, WARP_OP_AUDIO_DECODE,
                            src, srcLen, dst, codec);

    AROS_LIBFUNC_EXIT
}

/* ── Math operations ──────────────────────────────────────────── */

AROS_LH3(ULONG, IEWarpFPBatch,
    AROS_LHA(APTR,  descriptors, A0),
    AROS_LHA(APTR,  results,     A1),
    AROS_LHA(ULONG, count,       D0),
    struct IEWarpBase *, IEWarpBase, 20, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (!IEWarpBase->workerRunning || count == 0)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_FP_BATCH, count * 12);
        return 0;
    }

    /* FP_BATCH: reqPtr = descriptors (12 bytes each: opcode:u32, a:f32, b:f32)
     * respPtr = results (packed f32 array, 4 bytes per result)
     * reqLen = count of operations */
    return ie_warp_dispatch(IEWarpBase, WARP_OP_FP_BATCH,
                            descriptors, count, results, count * 4);

    AROS_LIBFUNC_EXIT
}

AROS_LH3(ULONG, IEWarpMatrixMul,
    AROS_LHA(APTR, matA,   A0),
    AROS_LHA(APTR, matB,   A1),
    AROS_LHA(APTR, matOut, A2),
    struct IEWarpBase *, IEWarpBase, 21, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (!IEWarpBase->workerRunning)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_MATRIX_MUL, 192);
        return 0;
    }

    /* MATRIX_MUL: Worker expects r12=matA, r14=matB, r29=dstC.
     * RESP_PTR → r14 = matB, TIMEOUT → r29 = matOut (destination). */
    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_OP, WARP_OP_MATRIX_MUL);
    ie_write32(IE_COPROC_REQ_PTR, (ULONG)matA);
    ie_write32(IE_COPROC_REQ_LEN, 64);
    ie_write32(IE_COPROC_RESP_PTR, (ULONG)matB);
    ie_write32(IE_COPROC_RESP_CAP, 64);
    ie_write32(IE_COPROC_TIMEOUT, (ULONG)matOut);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

    if (ie_read32(IE_COPROC_CMD_STATUS) != 0)
    {
        ie_warp_track_error(IEWarpBase, 0);
        return 0;
    }

    {
        ULONG ticket = ie_read32(IE_COPROC_TICKET);
        ie_warp_track_accel(IEWarpBase, WARP_OP_MATRIX_MUL, 192);
        if (IEWarpBase->batchMode)
            IEWarpBase->lastTicket = ticket;
        return ticket;
    }

    AROS_LIBFUNC_EXIT
}

AROS_LH4(ULONG, IEWarpCRC32,
    AROS_LHA(APTR,   data,    A0),
    AROS_LHA(ULONG,  len,     D0),
    AROS_LHA(ULONG,  initial, D1),
    AROS_LHA(ULONG *, result, A1),
    struct IEWarpBase *, IEWarpBase, 22, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (!IEWarpBase->workerRunning || len < IEWarpBase->threshold)
    {
        /* M68K fallback — bit-by-bit CRC32 */
        ULONG crc = initial;
        UBYTE *p = (UBYTE *)data;
        ULONG i, j;
        for (i = 0; i < len; i++)
        {
            crc ^= p[i];
            for (j = 0; j < 8; j++)
            {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xEDB88320;
                else
                    crc >>= 1;
            }
        }
        crc ^= 0xFFFFFFFF;
        if (result)
            *result = crc;
        ie_warp_track_fallback(IEWarpBase, WARP_OP_CRC32, len);
        return 0;
    }

    /* CRC32: data=input buffer, len=byte count, initial=starting CRC (via TIMEOUT→r29).
     * Worker writes the computed CRC to respPtr (r14) and to the response
     * descriptor's resultCode field. We pass `result` as respPtr so the
     * worker stores the CRC directly into the caller's ULONG. */
    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_OP, WARP_OP_CRC32);
    ie_write32(IE_COPROC_REQ_PTR, (ULONG)data);
    ie_write32(IE_COPROC_REQ_LEN, len);
    ie_write32(IE_COPROC_RESP_PTR, (ULONG)result);
    ie_write32(IE_COPROC_RESP_CAP, 4);
    ie_write32(IE_COPROC_TIMEOUT, initial);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

    if (ie_read32(IE_COPROC_CMD_STATUS) != 0)
    {
        ie_warp_track_error(IEWarpBase, 0);
        return 0;
    }

    {
        ULONG ticket = ie_read32(IE_COPROC_TICKET);
        ie_warp_track_accel(IEWarpBase, WARP_OP_CRC32, len);
        if (IEWarpBase->batchMode)
            IEWarpBase->lastTicket = ticket;
        return ticket;
    }

    AROS_LIBFUNC_EXIT
}

/* ── Rendering operations ─────────────────────────────────────── */

AROS_LH7(ULONG, IEWarpGradientFill,
    AROS_LHA(APTR,  dst,        A0),
    AROS_LHA(UWORD, width,      D0),
    AROS_LHA(UWORD, height,     D1),
    AROS_LHA(UWORD, stride,     D2),
    AROS_LHA(ULONG, startColor, D3),
    AROS_LHA(ULONG, endColor,   D4),
    AROS_LHA(UWORD, direction,  D5),
    struct IEWarpBase *, IEWarpBase, 23, IEWarp)
{
    AROS_LIBFUNC_INIT

    ULONG totalBytes = (ULONG)height * (ULONG)stride;

    if (!IEWarpBase->workerRunning || totalBytes < IEWarpBase->threshold)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_GRADIENT_FILL, totalBytes);
        return 0;
    }

    /* GRADIENT_FILL: startColor in reqPtr, endColor in reqLen (packed),
     * dst in respPtr, stride|(direction<<16) in respCap */
    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_OP, WARP_OP_GRADIENT_FILL);
    ie_write32(IE_COPROC_REQ_PTR, startColor);
    ie_write32(IE_COPROC_REQ_LEN,
               ((ULONG)width) | ((ULONG)height << 16));
    ie_write32(IE_COPROC_RESP_PTR, (ULONG)dst);
    ie_write32(IE_COPROC_RESP_CAP,
               ((ULONG)stride) | ((ULONG)direction << 16));
    ie_write32(IE_COPROC_TIMEOUT, endColor);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

    if (ie_read32(IE_COPROC_CMD_STATUS) != 0)
    {
        ie_warp_track_error(IEWarpBase, 0);
        return 0;
    }

    {
        ULONG ticket = ie_read32(IE_COPROC_TICKET);
        ie_warp_track_accel(IEWarpBase, WARP_OP_GRADIENT_FILL, totalBytes);
        if (IEWarpBase->batchMode)
            IEWarpBase->lastTicket = ticket;
        return ticket;
    }

    AROS_LIBFUNC_EXIT
}

/* ── Scroll operation ─────────────────────────────────────────── */

AROS_LH8(ULONG, IEWarpScroll,
    AROS_LHA(APTR,  fb,     A0),
    AROS_LHA(UWORD, width,  D0),
    AROS_LHA(UWORD, height, D1),
    AROS_LHA(UWORD, xMin,   D2),
    AROS_LHA(UWORD, yMin,   D3),
    AROS_LHA(WORD,  dx,     D4),
    AROS_LHA(WORD,  dy,     D5),
    AROS_LHA(UWORD, stride, D6),
    struct IEWarpBase *, IEWarpBase, 39, IEWarp)
{
    AROS_LIBFUNC_INIT

    ULONG totalBytes = (ULONG)height * (ULONG)stride;

    if (!IEWarpBase->workerRunning || totalBytes < IEWarpBase->threshold)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_SCROLL, totalBytes);
        return 0;
    }

    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_OP, WARP_OP_SCROLL);
    ie_write32(IE_COPROC_REQ_PTR, (ULONG)fb);
    ie_write32(IE_COPROC_REQ_LEN,
               (ULONG)width | ((ULONG)height << 16));
    ie_write32(IE_COPROC_RESP_PTR,
               (ULONG)xMin | ((ULONG)yMin << 16));
    ie_write32(IE_COPROC_RESP_CAP,
               (ULONG)(UWORD)dx | ((ULONG)(UWORD)dy << 16));
    ie_write32(IE_COPROC_TIMEOUT, (ULONG)stride);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

    if (ie_read32(IE_COPROC_CMD_STATUS) != 0)
    {
        ie_warp_track_error(IEWarpBase, 0);
        return 0;
    }

    {
        ULONG ticket = ie_read32(IE_COPROC_TICKET);
        ie_warp_track_accel(IEWarpBase, WARP_OP_SCROLL, totalBytes);
        if (IEWarpBase->batchMode)
            IEWarpBase->lastTicket = ticket;
        return ticket;
    }

    AROS_LIBFUNC_EXIT
}

/* ── Glyph rendering operation ────────────────────────────────── */

AROS_LH5(ULONG, IEWarpGlyphRender,
    AROS_LHA(APTR,  descs,     A0),
    AROS_LHA(ULONG, count,     D0),
    AROS_LHA(APTR,  dst,       A1),
    AROS_LHA(ULONG, dstStride, D1),
    AROS_LHA(APTR,  alphaData, A2),
    struct IEWarpBase *, IEWarpBase, 40, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (!IEWarpBase->workerRunning || count == 0)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_GLYPH_RENDER, count * 16);
        return 0;
    }

    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_OP, WARP_OP_GLYPH_RENDER);
    ie_write32(IE_COPROC_REQ_PTR, (ULONG)descs);
    ie_write32(IE_COPROC_REQ_LEN, count);
    ie_write32(IE_COPROC_RESP_PTR, (ULONG)dst);
    ie_write32(IE_COPROC_RESP_CAP, dstStride);
    ie_write32(IE_COPROC_TIMEOUT, (ULONG)alphaData);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

    if (ie_read32(IE_COPROC_CMD_STATUS) != 0)
    {
        ie_warp_track_error(IEWarpBase, 0);
        return 0;
    }

    {
        ULONG ticket = ie_read32(IE_COPROC_TICKET);
        ie_warp_track_accel(IEWarpBase, WARP_OP_GLYPH_RENDER, count * 16);
        if (IEWarpBase->batchMode)
            IEWarpBase->lastTicket = ticket;
        return ticket;
    }

    AROS_LIBFUNC_EXIT
}

/* ── Area fill operation ──────────────────────────────────────── */

AROS_LH4(ULONG, IEWarpAreaFill,
    AROS_LHA(APTR,  vertices, A0),
    AROS_LHA(ULONG, count,    D0),
    AROS_LHA(APTR,  dstBase,  A1),
    AROS_LHA(ULONG, stride,   D1),
    struct IEWarpBase *, IEWarpBase, 41, IEWarp)
{
    AROS_LIBFUNC_INIT

    if (!IEWarpBase->workerRunning || count < 3)
    {
        ie_warp_track_fallback(IEWarpBase, WARP_OP_AREA_FILL, 0);
        return 0;
    }

    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_OP, WARP_OP_AREA_FILL);
    ie_write32(IE_COPROC_REQ_PTR, (ULONG)vertices);
    ie_write32(IE_COPROC_REQ_LEN, count);
    ie_write32(IE_COPROC_RESP_PTR, (ULONG)dstBase);
    ie_write32(IE_COPROC_RESP_CAP, stride);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

    if (ie_read32(IE_COPROC_CMD_STATUS) != 0)
    {
        ie_warp_track_error(IEWarpBase, 0);
        return 0;
    }

    {
        ULONG ticket = ie_read32(IE_COPROC_TICKET);
        ie_warp_track_accel(IEWarpBase, WARP_OP_AREA_FILL, count * 4);
        if (IEWarpBase->batchMode)
            IEWarpBase->lastTicket = ticket;
        return ticket;
    }

    AROS_LIBFUNC_EXIT
}

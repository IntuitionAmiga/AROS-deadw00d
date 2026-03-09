/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE-accelerated BitMapScale via IE64 coprocessor
*/

#include <aros/libcall.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <graphics/rastport.h>
#include <graphics/scale.h>

#include <libraries/iewarp.h>
#include <ie_hwreg.h>

static struct Library *IEWarpBase = NULL;
#include <iewarp_consumer.h>

AROS_LH1(void, BitMapScale,
    AROS_LHA(struct BitScaleArgs *, bsa, A0),
    struct GfxBase *, GfxBase,
    113, Graphics)
{
    AROS_LIBFUNC_INIT

    ULONG totalBytes;

    if (!bsa || !bsa->bsa_SrcBitMap || !bsa->bsa_DestBitMap)
        return;

    totalBytes = (ULONG)bsa->bsa_SrcWidth * (ULONG)bsa->bsa_SrcHeight;

    if (totalBytes >= 4096 && IEWARP_OPEN())
    {
        APTR src = bsa->bsa_SrcBitMap->Planes[0];
        APTR dst = bsa->bsa_DestBitMap->Planes[0];

        if (src && dst)
        {
            IEWarpSetCaller(IEWARP_CALLER_GRAPHICS);
            {
                ULONG ticket = IEWarpBlitScale(
                    src, dst,
                    bsa->bsa_SrcWidth, bsa->bsa_SrcHeight,
                    (bsa->bsa_SrcWidth * bsa->bsa_XDestFactor) / bsa->bsa_XSrcFactor,
                    (bsa->bsa_SrcHeight * bsa->bsa_YDestFactor) / bsa->bsa_YSrcFactor,
                    bsa->bsa_SrcBitMap->BytesPerRow,
                    bsa->bsa_DestBitMap->BytesPerRow);
                if (ticket)
                {
                    IEWarpWait(ticket);
                    bsa->bsa_DestWidth = (bsa->bsa_SrcWidth * bsa->bsa_XDestFactor) / bsa->bsa_XSrcFactor;
                    bsa->bsa_DestHeight = (bsa->bsa_SrcHeight * bsa->bsa_YDestFactor) / bsa->bsa_YSrcFactor;
                    return;
                }
            }
        }
    }

    /* M68K fallback — simple nearest-neighbor scale */
    {
        UBYTE *src = (UBYTE *)bsa->bsa_SrcBitMap->Planes[0];
        UBYTE *dst = (UBYTE *)bsa->bsa_DestBitMap->Planes[0];
        UWORD srcW = bsa->bsa_SrcWidth;
        UWORD srcH = bsa->bsa_SrcHeight;
        UWORD dstW, dstH;
        UWORD srcBPR, dstBPR;
        UWORD bpp;
        UWORD x, y;

        if (!src || !dst) return;

        dstW = (srcW * bsa->bsa_XDestFactor) / bsa->bsa_XSrcFactor;
        dstH = (srcH * bsa->bsa_YDestFactor) / bsa->bsa_YSrcFactor;
        srcBPR = bsa->bsa_SrcBitMap->BytesPerRow;
        dstBPR = bsa->bsa_DestBitMap->BytesPerRow;
        bpp = (srcW > 0) ? (srcBPR / srcW) : 1;
        if (bpp < 1) bpp = 1;

        for (y = 0; y < dstH; y++)
        {
            UWORD srcY = (y * srcH) / dstH;
            UBYTE *srcRow = src + srcY * srcBPR;
            UBYTE *dstRow = dst + y * dstBPR;
            for (x = 0; x < dstW; x++)
            {
                UWORD srcX = (x * srcW) / dstW;
                UWORD i;
                for (i = 0; i < bpp; i++)
                    dstRow[x * bpp + i] = srcRow[srcX * bpp + i];
            }
        }

        bsa->bsa_DestWidth = dstW;
        bsa->bsa_DestHeight = dstH;
    }

    AROS_LIBFUNC_EXIT
}

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE-accelerated Text rendering via IE64 coprocessor
*/

#include <aros/libcall.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <graphics/gfx.h>

#include <ie_hwreg.h>
#include <libraries/iewarp.h>

/* Coprocessor warp dispatch threshold (total glyph pixels) */
#define GLYPH_WARP_THRESHOLD 2048

/* Glyph descriptor for WARP_OP_GLYPH_RENDER (16 bytes per glyph) */
struct GlyphDesc
{
    UWORD dstX;
    UWORD dstY;
    ULONG srcOffset;
    UWORD width;
    UWORD height;
    ULONG fgColor;
};

/*
 * Try to batch-render text via WARP_OP_GLYPH_RENDER.
 * Returns TRUE if dispatch succeeded, FALSE for fallback.
 *
 * Expands 1-bit font templates to 8-bit alpha data, builds glyph
 * descriptors, and dispatches a single IE64 coprocessor call.
 */
static BOOL IE_WarpTextBatch(struct RastPort *rp, struct TextFont *tf,
                              CONST_STRPTR string, ULONG count,
                              ULONG dstBase, ULONG dstStride)
{
    UWORD lochar  = tf->tf_LoChar;
    UWORD hichar  = tf->tf_HiChar;
    UWORD modulo  = tf->tf_Modulo;
    UWORD ysize   = tf->tf_YSize;
    UWORD *charloc  = (UWORD *)tf->tf_CharLoc;
    UBYTE *chardata = (UBYTE *)tf->tf_CharData;
    ULONG fgColor = (ULONG)rp->FgPen;

    /* First pass: compute total alpha data size */
    ULONG totalPixels = 0;
    ULONG i;

    for (i = 0; i < count; i++)
    {
        UBYTE ch = (UBYTE)string[i];
        UWORD idx = (ch < lochar || ch > hichar)
                    ? (hichar - lochar + 1) : (ch - lochar);
        UWORD charwidth = charloc[idx * 2 + 1];
        totalPixels += (ULONG)charwidth * (ULONG)ysize;
    }

    if (totalPixels < GLYPH_WARP_THRESHOLD)
        return FALSE;

    {
        /* Allocate descriptor array + alpha data buffer */
        ULONG descSize = count * sizeof(struct GlyphDesc);
        ULONG totalSize = descSize + totalPixels;
        UBYTE *buf = AllocMem(totalSize, MEMF_ANY);

        if (!buf)
            return FALSE;

        {
            struct GlyphDesc *descs = (struct GlyphDesc *)buf;
            UBYTE *alphaData = buf + descSize;
            ULONG alphaOff = 0;
            WORD x = rp->cp_x;
            WORD y = rp->cp_y - tf->tf_Baseline;

            /* Second pass: build descriptors and expand 1-bit→8-bit */
            for (i = 0; i < count; i++)
            {
                UBYTE ch = (UBYTE)string[i];
                UWORD idx = (ch < lochar || ch > hichar)
                            ? (hichar - lochar + 1) : (ch - lochar);
                UWORD bitoff   = charloc[idx * 2];
                UWORD charwidth = charloc[idx * 2 + 1];

                descs[i].dstX = (UWORD)x;
                descs[i].dstY = (UWORD)y;
                descs[i].srcOffset = alphaOff;
                descs[i].width = charwidth;
                descs[i].height = ysize;
                descs[i].fgColor = fgColor;

                if (charwidth > 0)
                {
                    /* Expand 1-bit template to 8-bit alpha */
                    UWORD row, col;
                    UBYTE *ap = alphaData + alphaOff;
                    UWORD startByte = bitoff >> 3;
                    UWORD startBit  = bitoff & 7;

                    for (row = 0; row < ysize; row++)
                    {
                        UBYTE *rowData = chardata + row * modulo + startByte;
                        for (col = 0; col < charwidth; col++)
                        {
                            UWORD bit = startBit + col;
                            *ap++ = (rowData[bit >> 3] &
                                     (0x80 >> (bit & 7)))
                                    ? 0xFF : 0x00;
                        }
                    }
                    alphaOff += (ULONG)charwidth * (ULONG)ysize;
                }

                if (tf->tf_CharSpace)
                    x += ((WORD *)tf->tf_CharSpace)[idx];
                else
                    x += charwidth;

                if (tf->tf_CharKern)
                    x += ((WORD *)tf->tf_CharKern)[idx];
            }

            /* Dispatch WARP_OP_GLYPH_RENDER */
            ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
            ie_write32(IE_COPROC_OP, WARP_OP_GLYPH_RENDER);
            ie_write32(IE_COPROC_REQ_PTR, (ULONG)descs);
            ie_write32(IE_COPROC_REQ_LEN, count);
            ie_write32(IE_COPROC_RESP_PTR, dstBase);
            ie_write32(IE_COPROC_RESP_CAP, dstStride);
            ie_write32(IE_COPROC_TIMEOUT, (ULONG)alphaData);
            ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

            if (ie_read32(IE_COPROC_CMD_STATUS) == 0)
            {
                ULONG ticket = ie_read32(IE_COPROC_TICKET);
                ie_write32(IE_COPROC_TICKET, ticket);
                ie_write32(IE_COPROC_TIMEOUT, 5000);
                ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_WAIT_CMD);

                if (ie_read32(IE_COPROC_TICKET_STATUS) == IE_COPROC_ST_OK)
                {
                    rp->cp_x = x;
                    FreeMem(buf, totalSize);
                    return TRUE;
                }
            }

            FreeMem(buf, totalSize);
        }
    }

    return FALSE;
}

AROS_LH3(LONG, Text,
    AROS_LHA(struct RastPort *, rp,     A1),
    AROS_LHA(CONST_STRPTR,      string, A0),
    AROS_LHA(ULONG,             count,  D0),
    struct GfxBase *, GfxBase,
    10, Graphics)
{
    AROS_LIBFUNC_INIT

    struct TextFont *tf = rp->Font;

    if (!tf || count == 0)
        return 0;

    /*
     * Try IE64 batch glyph rendering for large text operations.
     * Expands 1-bit font templates to 8-bit alpha, builds batch
     * descriptors, and dispatches a single WARP_OP_GLYPH_RENDER.
     * Falls back to per-character BltTemplate on small strings or
     * if the RastPort has no VRAM-backed bitmap.
     */
    if (count >= 4 && rp->BitMap)
    {
        ULONG dstBase = (ULONG)rp->BitMap->Planes[0];
        ULONG dstStride = rp->BitMap->BytesPerRow;

        if (dstBase >= 0x100000 && dstBase < 0x600000)
        {
            if (IE_WarpTextBatch(rp, tf, string, count, dstBase, dstStride))
                return (LONG)count;
        }
    }

    /*
     * Fallback: render characters one at a time using BltTemplate.
     * BltTemplate goes through the IEGfx HIDD which has IE64 acceleration
     * for large template blits (color expansion).
     */
    {
        ULONG i;
        WORD x = rp->cp_x;
        WORD y = rp->cp_y - tf->tf_Baseline;
        UWORD lochar  = tf->tf_LoChar;
        UWORD hichar  = tf->tf_HiChar;
        UWORD modulo  = tf->tf_Modulo;
        UWORD ysize   = tf->tf_YSize;
        UWORD *charloc  = (UWORD *)tf->tf_CharLoc;
        UBYTE *chardata = (UBYTE *)tf->tf_CharData;

        for (i = 0; i < count; i++)
        {
            UBYTE ch = (UBYTE)string[i];
            UWORD idx;
            UWORD bitoff, charwidth;

            if (ch < lochar || ch > hichar)
                idx = hichar - lochar + 1;  /* default character */
            else
                idx = ch - lochar;

            bitoff   = charloc[idx * 2];
            charwidth = charloc[idx * 2 + 1];

            if (charwidth > 0)
            {
                BltTemplate(chardata + (bitoff >> 3),
                            bitoff & 7,
                            modulo,
                            rp, x, y,
                            charwidth, ysize);
            }

            if (tf->tf_CharSpace)
                x += ((WORD *)tf->tf_CharSpace)[idx];
            else
                x += charwidth;

            if (tf->tf_CharKern)
                x += ((WORD *)tf->tf_CharKern)[idx];
        }

        rp->cp_x = x;
    }

    return (LONG)count;

    AROS_LIBFUNC_EXIT
}

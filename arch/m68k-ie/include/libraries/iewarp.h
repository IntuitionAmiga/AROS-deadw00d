/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: iewarp.library — IE64 coprocessor acceleration library
*/

#ifndef LIBRARIES_IEWARP_H
#define LIBRARIES_IEWARP_H

#include <exec/types.h>
#include <exec/libraries.h>

#define IEWARPNAME "iewarp.library"

/* Operation codes (WARP_OP_*) */

/* Memory primitives */
#define WARP_OP_NOP             0
#define WARP_OP_MEMCPY          1
#define WARP_OP_MEMCPY_QUICK    2
#define WARP_OP_MEMSET          3
#define WARP_OP_MEMMOVE         4

/* Graphics primitives */
#define WARP_OP_BLIT_COPY       10
#define WARP_OP_BLIT_MASK       11
#define WARP_OP_BLIT_SCALE      12
#define WARP_OP_BLIT_CONVERT    13
#define WARP_OP_BLIT_ALPHA      14
#define WARP_OP_FILL_RECT       15
#define WARP_OP_AREA_FILL       16
#define WARP_OP_PIXEL_PROCESS   17

/* Audio */
#define WARP_OP_AUDIO_MIX       20
#define WARP_OP_AUDIO_RESAMPLE  21
#define WARP_OP_AUDIO_DECODE    22

/* Math */
#define WARP_OP_FP_BATCH        30
#define WARP_OP_MATRIX_MUL      31
#define WARP_OP_CRC32           32

/* Rendering */
#define WARP_OP_GRADIENT_FILL   40
#define WARP_OP_GLYPH_RENDER    41
#define WARP_OP_SCROLL          42

/* Pixel format codes for WARP_OP_BLIT_CONVERT */
#define WARP_PIXFMT_RGBA32      16
#define WARP_PIXFMT_ARGB32      17
#define WARP_PIXFMT_RGB24       18
#define WARP_PIXFMT_BGR24       19
#define WARP_PIXFMT_BGRA32      20
#define WARP_PIXFMT_ABGR32      21

/* FP_BATCH sub-operation codes */
#define WARP_FP_ADD     0
#define WARP_FP_SUB     1
#define WARP_FP_MUL     2
#define WARP_FP_DIV     3
#define WARP_FP_SQRT    4
#define WARP_FP_SIN     5
#define WARP_FP_COS     6
#define WARP_FP_TAN     7
#define WARP_FP_ATAN    8
#define WARP_FP_LOG     9
#define WARP_FP_EXP     10
#define WARP_FP_POW     11
#define WARP_FP_ABS     12
#define WARP_FP_NEG     13

/* FP_BATCH descriptor (12 bytes per operation) */
struct IEWarpFPDesc
{
    UBYTE  op;          /* WARP_FP_* sub-operation */
    UBYTE  pad[3];
    float  a;           /* First operand */
    float  b;           /* Second operand (unused for unary ops) */
};
/* Results are written to a separate packed float32 array (4 bytes per result),
 * NOT back into the descriptor buffer. */

/* Gradient fill direction */
#define WARP_GRADIENT_VERTICAL    0
#define WARP_GRADIENT_HORIZONTAL  1

/* Stats structure returned by IEWarpGetStats() */
struct IEWarpStats
{
    ULONG ops;              /* Total operations dispatched */
    ULONG bytes;            /* Total bytes processed */
    ULONG overheadNs;       /* Dispatch overhead in nanoseconds */
    ULONG completedTicket;  /* Last completed ticket */
    ULONG threshold;        /* Current adaptive threshold in bytes */
};

#endif /* LIBRARIES_IEWARP_H */

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
    /* Extended fields (IEWarpMon) */
    ULONG ringDepth;        /* Current IE64 ring occupancy */
    ULONG ringHighWater;    /* Peak ring occupancy since reset */
    ULONG uptimeSecs;       /* Seconds since IE64 worker started */
    ULONG irqEnabled;       /* 1 if completion IRQ enabled */
    ULONG workerState;      /* Bitmask of running workers */
    ULONG busyPct;          /* Worker busy % 0-100 */
};

/* Per-operation counter (returned by IEWarpGetOpStats) */
struct IEWarpOpCounter
{
    ULONG accelCount;       /* Dispatched to IE64 */
    ULONG accelBytes;
    ULONG fallbackCount;    /* Fell back to M68K */
    ULONG fallbackBytes;
};

/* Per-task stats (returned by IEWarpGetTaskStats) */
struct IEWarpTaskEntry
{
    struct Task *task;      /* NULL = free slot */
    char         name[32];
    ULONG        ops;
    ULONG        bytes;
};

/* Per-caller/consumer stats (returned by IEWarpGetCallerStats) */
struct IEWarpCallerEntry
{
    ULONG callerID;         /* IEWARP_CALLER_* */
    ULONG ops;
    ULONG bytes;
};

/* Error counters (returned by IEWarpGetErrorStats) */
struct IEWarpErrorStats
{
    ULONG queueFull;
    ULONG workerDown;
    ULONG staleTicket;
    ULONG enqueueFail;
};

/* Batch counters (returned by IEWarpGetBatchStats) */
struct IEWarpBatchStats
{
    ULONG batchCount;       /* Completed BatchBegin/BatchEnd brackets */
    ULONG totalBatchedOps;  /* Total ops inside all batches */
    ULONG maxBatchSize;     /* Largest single batch */
    ULONG currentBatchOps;  /* Ops in current open batch (0 if not batching) */
};

/* Waiter slot info (returned by IEWarpGetWaiterInfo) */
struct IEWarpWaiterInfo
{
    char   taskName[32];
    ULONG  ticket;
};

/* Caller ID constants for IEWarpSetCaller() */
#define IEWARP_CALLER_UNKNOWN    0
#define IEWARP_CALLER_EXEC       1
#define IEWARP_CALLER_GRAPHICS   2
#define IEWARP_CALLER_IEGFX      3
#define IEWARP_CALLER_CGFX       4
#define IEWARP_CALLER_AHI        5
#define IEWARP_CALLER_ICON       6
#define IEWARP_CALLER_MATH       7
#define IEWARP_CALLER_MUI        8
#define IEWARP_CALLER_FREETYPE   9
#define IEWARP_CALLER_DATATYPES  10
#define IEWARP_CALLER_APP        11
#define IEWARP_CALLER_MAX        12

#define IEWARP_MAX_OPS      48
#define IEWARP_MAX_TASKS    32
#define IEWARP_MAX_CALLERS  16

#endif /* LIBRARIES_IEWARP_H */

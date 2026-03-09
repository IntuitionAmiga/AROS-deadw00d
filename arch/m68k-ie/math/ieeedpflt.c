/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE DP float (integer to double) — GCC softfloat override
*/

#include <aros/libcall.h>

/*
 * IEEEDPFlt — convert LONG integer to IEEE double-precision.
 *
 * Arch-specific override using GCC's software FP emulation, which
 * generates inline softfloat calls rather than the generic AROS
 * bit-manipulation implementation. Individual FP operations are too
 * fast to benefit from IE64 coprocessor dispatch (~50us overhead).
 * The IE64 coprocessor's WARP_OP_FP_BATCH is for application-level
 * callers that can batch N operations in a single dispatch.
 */
AROS_LH1(double, IEEEDPFlt,
    AROS_LHA(LONG, a, D0),
    struct Library *, MathIeeeDoubBasBase, 6, MathIeeeDoubBas)
{
    AROS_LIBFUNC_INIT

    return (double)a;

    AROS_LIBFUNC_EXIT
}

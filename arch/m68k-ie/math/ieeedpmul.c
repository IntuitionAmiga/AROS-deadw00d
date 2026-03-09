/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE DP multiply — GCC softfloat override
*/

#include <aros/libcall.h>

/*
 * IEEEDPMul — IEEE double-precision multiply.
 *
 * Arch-specific override using GCC's software FP emulation, which
 * generates inline softfloat calls rather than the generic AROS
 * bit-manipulation implementation. Individual FP operations are too
 * fast to benefit from IE64 coprocessor dispatch (~50us overhead).
 * The IE64 coprocessor's WARP_OP_FP_BATCH is for application-level
 * callers that can batch N operations in a single dispatch.
 */
AROS_LH2(double, IEEEDPMul,
    AROS_LHA(double, a, D0),
    AROS_LHA(double, b, D2),
    struct Library *, MathIeeeDoubBasBase, 13, MathIeeeDoubBas)
{
    AROS_LIBFUNC_INIT

    return a * b;

    AROS_LIBFUNC_EXIT
}

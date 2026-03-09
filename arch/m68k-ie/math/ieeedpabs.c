/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE DP absolute value — GCC softfloat override
*/

#include <aros/libcall.h>

/*
 * IEEEDPAbs — IEEE double-precision absolute value.
 *
 * Arch-specific override using GCC's software FP emulation, which
 * generates inline softfloat calls rather than the generic AROS
 * bit-manipulation implementation. Individual FP operations are too
 * fast to benefit from IE64 coprocessor dispatch (~50us overhead).
 * The IE64 coprocessor's WARP_OP_FP_BATCH is for application-level
 * callers that can batch N operations in a single dispatch.
 */
AROS_LH1(double, IEEEDPAbs,
    AROS_LHA(double, a, D0),
    struct Library *, MathIeeeDoubBasBase, 9, MathIeeeDoubBas)
{
    AROS_LIBFUNC_INIT

    return (a < 0.0) ? -a : a;

    AROS_LIBFUNC_EXIT
}

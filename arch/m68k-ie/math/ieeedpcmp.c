/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE DP compare — GCC softfloat override
*/

#include <aros/libcall.h>

/*
 * IEEEDPCmp — IEEE double-precision compare.
 * Returns +1 if a > b, -1 if a < b, 0 if equal.
 *
 * Arch-specific override using GCC's software FP emulation, which
 * generates inline softfloat calls rather than the generic AROS
 * bit-manipulation implementation. Individual FP operations are too
 * fast to benefit from IE64 coprocessor dispatch (~50us overhead).
 * The IE64 coprocessor's WARP_OP_FP_BATCH is for application-level
 * callers that can batch N operations in a single dispatch.
 */
AROS_LH2(LONG, IEEEDPCmp,
    AROS_LHA(double, a, D0),
    AROS_LHA(double, b, D2),
    struct Library *, MathIeeeDoubBasBase, 7, MathIeeeDoubBas)
{
    AROS_LIBFUNC_INIT

    return (a > b) ? 1 : (a < b) ? -1 : 0;

    AROS_LIBFUNC_EXIT
}

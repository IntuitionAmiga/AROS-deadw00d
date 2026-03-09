/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE SP pow — GCC softfloat override
*/

#include <aros/libcall.h>
#include <math.h>

/*
 * IEEESPPow — IEEE single-precision power.
 *
 * Arch-specific override using GCC's software FP emulation, which
 * generates inline softfloat calls rather than the generic AROS
 * bit-manipulation implementation. Individual FP operations are too
 * fast to benefit from IE64 coprocessor dispatch (~50us overhead).
 * The IE64 coprocessor's WARP_OP_FP_BATCH is for application-level
 * callers that can batch N operations in a single dispatch.
 */
AROS_LH2(float, IEEESPPow,
    AROS_LHA(float, exp, D0),
    AROS_LHA(float, base, D2),
    struct Library *, MathTransBase, 15, MathTrans)
{
    AROS_LIBFUNC_INIT

    return powf(base, exp);

    AROS_LIBFUNC_EXIT
}

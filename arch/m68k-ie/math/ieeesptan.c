/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE SP tangent — GCC softfloat override
*/

#include <aros/libcall.h>
#include <math.h>

/*
 * IEEESPTan — IEEE single-precision tangent.
 *
 * Arch-specific override using GCC's software FP emulation, which
 * generates inline softfloat calls rather than the generic AROS
 * bit-manipulation implementation. Individual FP operations are too
 * fast to benefit from IE64 coprocessor dispatch (~50us overhead).
 * The IE64 coprocessor's WARP_OP_FP_BATCH is for application-level
 * callers that can batch N operations in a single dispatch.
 */
AROS_LH1(float, IEEESPTan,
    AROS_LHA(float, x, D0),
    struct Library *, MathTransBase, 8, MathTrans)
{
    AROS_LIBFUNC_INIT

    return tanf(x);

    AROS_LIBFUNC_EXIT
}

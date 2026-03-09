/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE SP multiply — GCC softfloat override
*/

#include <aros/libcall.h>
#include <libraries/mathieeesp.h>

/*
 * IEEESPMul — IEEE single-precision multiply.
 *
 * Arch-specific override using GCC's software FP emulation, which
 * generates inline softfloat calls rather than the generic AROS
 * bit-manipulation implementation. Individual FP operations are too
 * fast to benefit from IE64 coprocessor dispatch (~50μs overhead).
 * The IE64 coprocessor's WARP_OP_FP_BATCH is for application-level
 * callers that can batch N operations in a single dispatch.
 */
AROS_LH2(float, IEEESPMul,
    AROS_LHA(float, a, D0),
    AROS_LHA(float, b, D1),
    struct Library *, MathIeeeSingBasBase, 13, MathIeeeSingBas)
{
    AROS_LIBFUNC_INIT

    return a * b;

    AROS_LIBFUNC_EXIT
}

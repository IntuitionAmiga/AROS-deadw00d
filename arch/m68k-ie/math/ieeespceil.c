/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE SP ceil — GCC softfloat override
*/

#include <aros/libcall.h>
#include <libraries/mathieeesp.h>
#include <math.h>

/*
 * IEEESPCeil — IEEE single-precision ceiling.
 * See ieeespmul.c for acceleration strategy notes.
 */
AROS_LH1(float, IEEESPCeil,
    AROS_LHA(float, a, D0),
    struct Library *, MathIeeeSingBasBase, 16, MathIeeeSingBas)
{
    AROS_LIBFUNC_INIT

    return ceilf(a);

    AROS_LIBFUNC_EXIT
}

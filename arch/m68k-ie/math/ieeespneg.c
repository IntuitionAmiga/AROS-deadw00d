/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE SP negate — GCC softfloat override
*/

#include <aros/libcall.h>
#include <libraries/mathieeesp.h>

/*
 * IEEESPNeg — IEEE single-precision negate.
 * See ieeespmul.c for acceleration strategy notes.
 */
AROS_LH1(float, IEEESPNeg,
    AROS_LHA(float, a, D0),
    struct Library *, MathIeeeSingBasBase, 10, MathIeeeSingBas)
{
    AROS_LIBFUNC_INIT

    return -a;

    AROS_LIBFUNC_EXIT
}

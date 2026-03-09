/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE SP absolute value — GCC softfloat override
*/

#include <aros/libcall.h>
#include <libraries/mathieeesp.h>

/*
 * IEEESPAbs — IEEE single-precision absolute value.
 * See ieeespmul.c for acceleration strategy notes.
 */
AROS_LH1(float, IEEESPAbs,
    AROS_LHA(float, a, D0),
    struct Library *, MathIeeeSingBasBase, 9, MathIeeeSingBas)
{
    AROS_LIBFUNC_INIT

    return (a < 0.0f) ? -a : a;

    AROS_LIBFUNC_EXIT
}

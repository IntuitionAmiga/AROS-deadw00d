/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE SP divide — GCC softfloat override
*/

#include <aros/libcall.h>
#include <libraries/mathieeesp.h>

/*
 * IEEESPDiv — IEEE single-precision divide.
 * See ieeespmul.c for acceleration strategy notes.
 */
AROS_LH2(float, IEEESPDiv,
    AROS_LHA(float, a, D0),
    AROS_LHA(float, b, D1),
    struct Library *, MathIeeeSingBasBase, 14, MathIeeeSingBas)
{
    AROS_LIBFUNC_INIT

    if (b == 0.0f) return 0.0f;
    return a / b;

    AROS_LIBFUNC_EXIT
}

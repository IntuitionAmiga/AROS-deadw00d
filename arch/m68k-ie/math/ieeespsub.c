/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE SP subtract — GCC softfloat override
*/

#include <aros/libcall.h>
#include <libraries/mathieeesp.h>

/*
 * IEEESPSub — IEEE single-precision subtraction.
 * See ieeespmul.c for acceleration strategy notes.
 */
AROS_LH2(float, IEEESPSub,
    AROS_LHA(float, a, D0),
    AROS_LHA(float, b, D1),
    struct Library *, MathIeeeSingBasBase, 12, MathIeeeSingBas)
{
    AROS_LIBFUNC_INIT

    return a - b;

    AROS_LIBFUNC_EXIT
}

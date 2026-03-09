/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE SP floor — GCC softfloat override
*/

#include <aros/libcall.h>
#include <libraries/mathieeesp.h>
#include <math.h>

/*
 * IEEESPFloor — IEEE single-precision floor.
 * See ieeespmul.c for acceleration strategy notes.
 */
AROS_LH1(float, IEEESPFloor,
    AROS_LHA(float, a, D0),
    struct Library *, MathIeeeSingBasBase, 15, MathIeeeSingBas)
{
    AROS_LIBFUNC_INIT

    return floorf(a);

    AROS_LIBFUNC_EXIT
}

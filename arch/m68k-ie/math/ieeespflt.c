/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE SP float (integer to float) — GCC softfloat override
*/

#include <aros/libcall.h>
#include <libraries/mathieeesp.h>

/*
 * IEEESPFlt — convert LONG integer to IEEE single-precision.
 * See ieeespmul.c for acceleration strategy notes.
 */
AROS_LH1(float, IEEESPFlt,
    AROS_LHA(LONG, a, D0),
    struct Library *, MathIeeeSingBasBase, 6, MathIeeeSingBas)
{
    AROS_LIBFUNC_INIT

    return (float)a;

    AROS_LIBFUNC_EXIT
}

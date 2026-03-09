/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE SP fix (float to integer) — GCC softfloat override
*/

#include <aros/libcall.h>
#include <libraries/mathieeesp.h>

/*
 * IEEESPFix — convert IEEE single-precision to LONG integer.
 * See ieeespmul.c for acceleration strategy notes.
 */
AROS_LH1(LONG, IEEESPFix,
    AROS_LHA(float, a, D0),
    struct Library *, MathIeeeSingBasBase, 5, MathIeeeSingBas)
{
    AROS_LIBFUNC_INIT

    return (LONG)a;

    AROS_LIBFUNC_EXIT
}

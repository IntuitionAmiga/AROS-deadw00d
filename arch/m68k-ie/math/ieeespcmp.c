/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.
    Desc: IE arch-specific IEEE SP compare — GCC softfloat override
*/

#include <aros/libcall.h>
#include <libraries/mathieeesp.h>

/*
 * IEEESPCmp — IEEE single-precision compare.
 * Returns +1 if a > b, -1 if a < b, 0 if equal.
 * See ieeespmul.c for acceleration strategy notes.
 */
AROS_LH2(LONG, IEEESPCmp,
    AROS_LHA(float, a, D0),
    AROS_LHA(float, b, D1),
    struct Library *, MathIeeeSingBasBase, 7, MathIeeeSingBas)
{
    AROS_LIBFUNC_INIT

    return (a > b) ? 1 : (a < b) ? -1 : 0;

    AROS_LIBFUNC_EXIT
}

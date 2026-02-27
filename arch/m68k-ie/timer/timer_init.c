/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE timer.device initialization.
          Adapted from arch/m68k-amiga/timer/timer_init.c.
          All CIA hardware setup removed. The IE timer uses interrupt
          counting from the host-side timer goroutines:
            Level 4 → INTB_VERTB → VBlank (60Hz)
            Level 5 → INTB_EXTER → System timer (200Hz, 5ms)
*/

#define DEBUG 0
#include <aros/debug.h>

#include <aros/kernel.h>
#include <exec/types.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <exec/devices.h>
#include <exec/alerts.h>
#include <exec/initializers.h>
#include <devices/timer.h>
#include <hardware/intbits.h>
#include <graphics/gfxbase.h>

#include <proto/exec.h>
#include <proto/kernel.h>
#include <proto/timer.h>

#include <aros/symbolsets.h>

#include <defines/timer_LVO.h>

#include LC_LIBDEFS_FILE

#include <timer_intern.h>
#include <timer_platform.h>

AROS_INTP(ie_eclock_handler);
AROS_INTP(ie_vbint);

/****************************************************************************************/

/*
 * Create register-preserving call stubs (same pattern as Amiga timer).
 */
#define _AS_STRING(x)   #x
#define AS_STRING(x)    _AS_STRING(x)

#define PRESERVE_A0(lib, libname, funcname, funcid) \
        do { \
                void libname##_##funcname##_Wrapper(void) \
                { asm volatile ( \
                        "move.l %a0,%sp@-\n" \
                        "jsr " AS_STRING(AROS_SLIB_ENTRY(funcname, libname, funcid)) "\n" \
                        "move.l %sp@+,%a0\n" \
                        "rts\n" ); } \
                __AROS_SETVECADDR(lib, funcid, libname##_##funcname##_Wrapper); \
        } while (0)

#define PRESERVE_A0A1(lib, libname, funcname, funcid) \
        do { \
                void libname##_##funcname##_Wrapper(void) \
                { asm volatile ( \
                        "movem.l %a0-%a1,%sp@-\n" \
                        "jsr " AS_STRING(AROS_SLIB_ENTRY(funcname, libname, funcid)) "\n" \
                        "movem.l %sp@+,%a0-%a1\n" \
                        "rts\n" ); } \
                __AROS_SETVECADDR(lib, funcid, libname##_##funcname##_Wrapper); \
        } while (0)

static int GM_UNIQUENAME(Init)(LIBBASETYPEPTR LIBBASE)
{
    struct Interrupt *inter;
    struct GfxBase *GfxBase;

    PRESERVE_A0A1(LIBBASE, Timer, AddTime, LVOAddTime);
    PRESERVE_A0A1(LIBBASE, Timer, SubTime, LVOSubTime);
    PRESERVE_A0A1(LIBBASE, Timer, CmpTime, LVOCmpTime);
    PRESERVE_A0(LIBBASE, Timer, ReadEClock, LVOReadEClock);
    PRESERVE_A0(LIBBASE, Timer, GetSysTime, LVOGetSysTime);
    PRESERVE_A0(LIBBASE, Timer, GetUpTime, LVOGetUpTime);

    GfxBase = TaggedOpenLibrary(TAGGEDOPEN_GRAPHICS);

    InitCustom(GfxBase);

    /* IE uses NTSC timing (60Hz, ~715909 Hz E-clock).
     * These constants maintain compatibility with Amiga software.
     */
    LIBBASE->tb_eclock_rate = 715909;
    LIBBASE->tb_vblank_rate = 60;
    LIBBASE->tb_vblank_micros = 1000000 / LIBBASE->tb_vblank_rate; /* 16667 */
    SysBase->ex_EClockFrequency = LIBBASE->tb_eclock_rate;
    LIBBASE->tb_eclock_micro_mult = 91542;  /* NTSC */
    LIBBASE->tb_micro_eclock_mult = 23459;  /* NTSC */

    CloseLibrary((struct Library *)GfxBase);

    /* Initialise the lists */
    NEWLIST(&LIBBASE->tb_Lists[UNIT_VBLANK]);
    NEWLIST(&LIBBASE->tb_Lists[UNIT_MICROHZ]);

    /* VBlank interrupt — register with INTB_VERTB (Level 4).
     * Same as the Amiga version.
     */
    inter = &LIBBASE->tb_vbint;
    inter->is_Code         = (APTR)ie_vbint;
    inter->is_Data         = LIBBASE;
    inter->is_Node.ln_Name = "timer.device VBlank";
    inter->is_Node.ln_Pri  = 20;
    inter->is_Node.ln_Type = NT_INTERRUPT;
    AddIntServer(INTB_VERTB, inter);

    /* System timer interrupt — register with INTB_EXTER (Level 5).
     * On the Amiga, this slot is CIA-B/External. On the IE, Level 5
     * is the system timer (200Hz), and ie_irq.c dispatches it via
     * core_Cause(INTB_EXTER, ...).
     */
    inter = &LIBBASE->tb_timer_int;
    inter->is_Node.ln_Pri  = 0;
    inter->is_Node.ln_Type = NT_INTERRUPT;
    inter->is_Node.ln_Name = "timer.device eclock";
    inter->is_Code         = (APTR)ie_eclock_handler;
    inter->is_Data         = LIBBASE;
    AddIntServer(INTB_EXTER, inter);

    D(bug("ie timer.device init\n"));

    return TRUE;
}

/****************************************************************************************/

static int GM_UNIQUENAME(Open)
(
    LIBBASETYPEPTR LIBBASE,
    struct timerequest *tr,
    ULONG unitNum,
    ULONG flags
)
{
    switch (unitNum)
    {
    case UNIT_VBLANK:
    case UNIT_WAITUNTIL:
    case UNIT_MICROHZ:
    case UNIT_ECLOCK:
    case UNIT_WAITECLOCK:
        tr->tr_node.io_Error = 0;
        tr->tr_node.io_Unit = (struct Unit *)unitNum;
        tr->tr_node.io_Device = (struct Device *)LIBBASE;
        break;

    default:
        tr->tr_node.io_Error = IOERR_OPENFAIL;
    }

    return TRUE;
}

/****************************************************************************************/

static int GM_UNIQUENAME(Expunge)(LIBBASETYPEPTR LIBBASE)
{
    Disable();
    RemIntServer(INTB_VERTB, &LIBBASE->tb_vbint);
    RemIntServer(INTB_EXTER, &LIBBASE->tb_timer_int);
    Enable();
    return TRUE;
}

/****************************************************************************************/

ADD2INITLIB(GM_UNIQUENAME(Init), 0)
ADD2OPENDEV(GM_UNIQUENAME(Open), 0)
ADD2EXPUNGELIB(GM_UNIQUENAME(Expunge), 0)

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Intuition Engine M68K bootstrap to exec.
          Adapted from arch/m68k-amiga/boot/start.c — all Amiga custom
          chipset code (Paula, CIA, Gayle, autoconfig, overlay) removed.
          The IE provides a clean 68020+FPU environment with fixed memory.
*/

#define DEBUG 0
#include <aros/debug.h>

#include <aros/kernel.h>
#include <exec/memory.h>
#include <exec/resident.h>
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <proto/exec.h>
#include <hardware/cpu/memory.h>

#include <defines/exec_LVO.h>

#include <string.h>

#include "memory.h"

#include "kernel_romtags.h"
#include "kernel_base.h"

#define __AROS_KERNEL__
#include "exec_intern.h"

#include "ie_hwreg.h"
#include "ie_irq.h"
#include "early.h"
#include "debug.h"

#define SS_STACK_SIZE   0x02000

/* IE has no serial port — debug output goes to TERM_OUT MMIO */
static void ie_debug_putchar(int c)
{
    ie_write32(IE_TERM_OUT, (unsigned long)c);
}

#define DEFKRN_CMDLINE ""

extern const struct Resident Exec_resident;
extern struct ExecBase *AbsExecBase;

extern void __attribute__((interrupt)) Exec_Supervisor_Trap (void);
extern void __attribute__((interrupt)) Exec_Supervisor_Trap_00 (void);

/* TRAP #14/#15 handlers for KrnCli/KrnSti.
 * These modify the saved SR in the exception frame so that
 * interrupt masking works from both user and supervisor mode.
 * 68020 Format 0 exception frame: SR at (%sp), PC at 2(%sp).
 */
extern void IE_Trap_Cli(void);
extern void IE_Trap_Sti(void);
asm(
"	.text\n"
"	.balign 2\n"
"	.globl IE_Trap_Cli\n"
"IE_Trap_Cli:\n"
"	ori.w	#0x0700,(%sp)\n"
"	rte\n"
"	.globl IE_Trap_Sti\n"
"IE_Trap_Sti:\n"
"	andi.w	#0xf8ff,(%sp)\n"
"	rte\n"
);

#define _AS_STRING(x)   #x
#define AS_STRING(x)    _AS_STRING(x)

/* Sign-extending call stub macros (same as m68k-amiga) */
#define EXT_BYTE(lib, libname, funcname, funcid) \
        do { \
                void libname##_##funcname##_Wrapper(void) \
                { asm volatile ( \
                        "jsr " AS_STRING(AROS_SLIB_ENTRY(funcname, libname, funcid)) "\n" \
                        "ext.w %d0\n" \
                        "ext.l %d0\n" \
                        "rts\n"); } \
                __AROS_SETVECADDR(lib, funcid, libname##_##funcname##_Wrapper); \
        } while (0)
#define EXT_WORD(lib, libname, funcname, funcid) \
        do { \
                void libname##_##funcname##_Wrapper(void) \
                { asm volatile ( \
                        "jsr " AS_STRING(AROS_SLIB_ENTRY(funcname, libname, funcid)) "\n" \
                        "ext.l %d0\n" \
                        "rts\n"); } \
                __AROS_SETVECADDR(lib, funcid, libname##_##funcname##_Wrapper); \
        } while (0)
#define PRESERVE_ALL(lib, libname, funcname, funcid) \
        do { \
                void libname##_##funcname##_Wrapper(void) \
                { asm volatile ( \
                        "movem.l %d0-%d1/%a0-%a1,%sp@-\n" \
                        "jsr " AS_STRING(AROS_SLIB_ENTRY(funcname, libname, funcid)) "\n" \
                        "movem.l %sp@+,%d0-%d1/%a0-%a1\n" \
                        "rts\n" ); } \
                __AROS_SETVECADDR(lib, funcid, libname##_##funcname##_Wrapper); \
        } while (0)
#define FAKE_IT(lib, libname, funcname, funcid, ...) \
        do { \
                UWORD *asmcall = (UWORD *)__AROS_GETJUMPVEC(lib, funcid); \
                const UWORD code[] = { __VA_ARGS__ }; \
                asmcall[0] = code[0]; \
                asmcall[1] = code[1]; \
                asmcall[2] = code[2]; \
        } while (0)
#define FAKE_ID(lib, libname, funcname, funcid, value) \
        FAKE_IT(lib, libname, funcname, funcid, 0x303c, value, 0x4e75)

extern void SuperstackSwap(void);

static LONG doInitCode(void)
{
    /* Allocate a new supervisor stack */
    do {
        APTR ss_stack;
        ULONG ss_stack_size = SS_STACK_SIZE;

        ss_stack = AllocMem(SS_STACK_SIZE, MEMF_ANY | MEMF_CLEAR | MEMF_REVERSE);
        if (ss_stack && ((ULONG)ss_stack & (PAGE_SIZE - 1))) {
            FreeMem(ss_stack, SS_STACK_SIZE);
            ss_stack = AllocMem(SS_STACK_SIZE + PAGE_SIZE - 1, MEMF_ANY | MEMF_CLEAR | MEMF_REVERSE);
            ss_stack = (APTR)(((ULONG)ss_stack + PAGE_SIZE - 1) & PAGE_MASK);
        }
        if (ss_stack == NULL) {
            ie_puts("PANIC! Can't allocate supervisor stack!\n");
            Early_Alert(CODE_ALLOC_FAIL);
            break;
        }
        SysBase->SysStkLower = ss_stack;
        SysBase->SysStkUpper = ss_stack + ss_stack_size;
        SetSysBaseChkSum();

        Supervisor((ULONG_FUNC)SuperstackSwap);
    } while(0);

    ie_puts("exec.library initialized\n");
    InitCode(RTF_COLDSTART, 0);

    return 0;
}

extern BYTE _rom_start;
extern BYTE _rom_end;
extern BYTE _ext_start;
extern BYTE _ext_end;
extern BYTE _ss;
extern BYTE _ss_end;

static struct MemHeader *ie_addmemoryregion(ULONG startaddr, ULONG size, ULONG attrs)
{
    if (size < 65536)
        return NULL;

    krnCreateMemHeader(
        (attrs & MEMF_CHIP) ? "chip memory" : "fast memory",
        (attrs & MEMF_CHIP) ? -10 : -5,
        (APTR)startaddr, size,
        attrs);

    ie_puts("Added memory: ");
    /* Simple hex output for debug */
    ie_putchar('0'); ie_putchar('x');
    {
        int i;
        for (i = 28; i >= 0; i -= 4) {
            int nibble = (startaddr >> i) & 0xF;
            ie_putchar(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
        }
    }
    ie_putchar('-');
    ie_putchar('0'); ie_putchar('x');
    {
        ULONG end = startaddr + size - 1;
        int i;
        for (i = 28; i >= 0; i -= 4) {
            int nibble = (end >> i) & 0xF;
            ie_putchar(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
        }
    }
    ie_putchar('\n');

    return (struct MemHeader*)startaddr;
}

void exec_boot(ULONG *membanks, ULONG *cpupcr)
{
    struct TagItem bootmsg[] = {
        { KRN_CmdLine,          (IPTR)DEFKRN_CMDLINE   },
        { KRN_KernelStackBase,  (IPTR)&_ss              },
        { KRN_KernelStackSize,  (IPTR)(&_ss_end - &_ss) },
        { TAG_END                                       },
    };
    volatile APTR *trap;
    int i;
    struct MemHeader *mh;
    UWORD *kickrom[6];
    UWORD attnflags;

    trap = (APTR *)(NULL);

    /* Set all exceptions to Early_Exception handler */
    for (i = 2; i < 64; i++) {
        if (i != 31) /* Do not overwrite NMI */
            trap[i] = Early_Exception;
    }

    ie_puts("[IE] AROS boot\n");

    /* Extract AttnFlags from CPU/FPU data provided by rom_init.S */
    attnflags = cpupcr[0] & 0xffff;
    /* IE always has 32-bit addressing */
    attnflags |= AFF_ADDR32;
    /* FPU flags */
    if (cpupcr[0] & 0xffff0000) {
        attnflags |= AFF_FPU;
        if (((cpupcr[0] >> 16) & 0xff) <= 0x1f)
            attnflags |= AFF_68881;
        else
            attnflags |= AFF_68881 | AFF_68882;
    }

    ie_puts("[IE] CPU: 68020\n");

    /* ROM scan regions */
    kickrom[0] = (UWORD*)&_rom_start;
    kickrom[1] = (UWORD*)&_rom_end;
    kickrom[2] = (UWORD*)&_ext_start;
    kickrom[3] = (UWORD*)&_ext_end;
    kickrom[4] = (UWORD*)~0;
    kickrom[5] = (UWORD*)~0;

    /* Add first memory bank (fastest first, as provided by rom_init.S) */
    mh = ie_addmemoryregion(membanks[0], membanks[1],
        MEMF_FAST | MEMF_KICK | MEMF_PUBLIC | MEMF_LOCAL);
    if (mh == NULL) {
        ie_puts("PANIC! Can't create initial memory header!\n");
        Early_Alert(AT_DeadEnd | AG_NoMemory);
    }

    /* Initialize ExecBase */
    if (!krnPrepareExecBase(kickrom, mh, bootmsg))
        Early_Alert(AT_DeadEnd | AG_MakeLib | AO_ExecLib);

    ie_puts("[IE] ExecBase ready\n");

    /* PrepareExecBase does not create a bootstrap task — ThisTask is NULL.
     * M68KExceptionInit (called from IEIRQInit) installs exception handlers
     * that call FindTask(NULL) which returns SysBase->ThisTask.  If it's
     * NULL, FindTask returns NULL and the handler reads tc_TrapCode from
     * address 0x32 (inside the vector table) producing a garbage handler
     * address, causing an infinite exception cascade.
     *
     * Fix: install a minimal static task so FindTask(NULL) returns a valid
     * pointer with tc_TrapCode==0.  The real bootstrap task is created
     * later during InitCode(RTF_COLDSTART) in exec_init.c.
     */
    {
        static struct Task earlyBootTask;
        earlyBootTask.tc_Node.ln_Type = NT_TASK;
        earlyBootTask.tc_Node.ln_Name = "Early Boot";
        earlyBootTask.tc_State        = TS_RUN;
        earlyBootTask.tc_SPLower      = (APTR)&_ss;
        earlyBootTask.tc_SPUpper      = (APTR)&_ss_end;
        SysBase->ThisTask = &earlyBootTask;
    }

    /* Initialize IE interrupt handlers (autovector dispatch) */
    IEIRQInit(SysBase);
    ie_puts("[IE] IRQ handlers installed\n");

    PrivExecBase(SysBase)->PlatformData.BootMsg = bootmsg;

    SysBase->SysStkUpper = (APTR)&_ss_end;
    SysBase->SysStkLower = (APTR)&_ss;

    SysBase->AttnFlags = attnflags;

    /* GetCC instruction depends on CPU model — 68020 uses move.w %ccr,%d0 */
    /* move.w %ccr,%d0; rts; nop */
    FAKE_IT(SysBase, Exec, GetCC, LVOGetCC, 0x42c0, 0x4e75, 0x4e71);

    PRESERVE_ALL(SysBase, Exec, Permit, LVOPermit);
    PRESERVE_ALL(SysBase, Exec, ObtainSemaphore, LVOObtainSemaphore);
    PRESERVE_ALL(SysBase, Exec, ReleaseSemaphore, LVOReleaseSemaphore);
    PRESERVE_ALL(SysBase, Exec, ObtainSemaphoreShared, LVOObtainSemaphoreShared);

    /* Sign extension for return values */
    EXT_BYTE(SysBase, Exec, SetTaskPri, LVOSetTaskPri);
    EXT_BYTE(SysBase, Exec, AllocSignal, LVOAllocSignal);

    /* Create ROM memory header — covers .rom, .ext, .bss, .data sections */
    krnCreateROMHeader("Kickstart ROM",
        (APTR)IE_ROM_BASE,
        (APTR)(IE_ROM_BASE + 0x200000 - 1)); /* 2MB ROM region */

    /* Add remaining memory banks */
    for (i = 2; membanks[i + 1]; i += 2) {
        IPTR  addr = membanks[i];
        ULONG size = membanks[i + 1];

        mh = ie_addmemoryregion(addr, size,
            (addr < 0x00100000)
                ? (MEMF_CHIP | MEMF_KICK | MEMF_PUBLIC | MEMF_LOCAL | MEMF_24BITDMA)
                : (MEMF_FAST | MEMF_KICK | MEMF_PUBLIC | MEMF_LOCAL));
        Enqueue(&SysBase->MemList, &mh->mh_Node);

        if (addr < 0x00200000)
            SysBase->MaxLocMem = (size + 0xffff) & 0xffff0000;
    }

    SetSysBaseChkSum();

    /* Set privilege violation trap for Exec/Supervisor */
    trap[8] = Exec_Supervisor_Trap;

    /* KrnCli/KrnSti TRAP handlers — must be after M68KExceptionInit
     * which installs M68KTrapHelper for vectors 2-63.
     * TRAP #14 (vector 46) = disable interrupts (IPL=7)
     * TRAP #15 (vector 47) = enable interrupts (IPL=0)
     */
    trap[46] = IE_Trap_Cli;
    trap[47] = IE_Trap_Sti;

    /* IE has no CPU cache — install no-op cache stubs directly.
     * The m68k-all C dispatchers for CacheClearU/CacheClearE/CacheControl
     * call CPU-specific assembly functions via func(), which clobbers A6
     * (needed by 'jmp Supervisor(%a6)' in the 68020 variants).
     * Install 68000 no-op variants to bypass the dispatchers entirely.
     */
    {
        extern void AROS_SLIB_ENTRY(CacheClearU_00,Exec,106)(void);
        extern void AROS_SLIB_ENTRY(CacheClearE_00,Exec,107)(void);
        extern void AROS_SLIB_ENTRY(CacheControl_00,Exec,108)(void);
        __AROS_SETVECADDR(SysBase, LVOCacheClearU,
            AROS_SLIB_ENTRY(CacheClearU_00, Exec, 106));
        __AROS_SETVECADDR(SysBase, LVOCacheClearE,
            AROS_SLIB_ENTRY(CacheClearE_00, Exec, 107));
        __AROS_SETVECADDR(SysBase, LVOCacheControl,
            AROS_SLIB_ENTRY(CacheControl_00, Exec, 108));
    }

    CacheControl(CACRF_EnableI, CACRF_EnableI);
    CacheClearU();

    ie_puts("[IE] InitCode(RTF_SINGLETASK)\n");
    InitCode(RTF_SINGLETASK, 0);

    /* Allocate user stack and switch to user mode */
    do {
        const ULONG size = AROS_STACKSIZE;
        IPTR *usp;

        usp = AllocMem(size * sizeof(IPTR), MEMF_PUBLIC);
        if (usp == NULL) {
            ie_puts("PANIC! Can't allocate user stack!\n");
            Early_Alert(CODE_ALLOC_FAIL);
        }

        /* Switch to user mode */
        asm volatile (
            "move.l %0,%%usp\n"
            "move.w #0,%%sr\n"
            "pea 0f\n"
            "jmp %1@\n"
            "0:\n"
            :
            : "a" (&usp[size-3]),
              "a" (doInitCode)
            :);
    } while (0);

    /* Should not reach here */
    ie_puts("PANIC! doInitCode() returned!\n");
    Early_Alert(CODE_EXEC_FAIL);
}

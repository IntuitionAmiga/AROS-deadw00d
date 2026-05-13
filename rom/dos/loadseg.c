/*
    Copyright (C) 1995-2011, The AROS Development Team. All rights reserved.

    Desc: DOS function LoadSeg()
*/

#include <aros/asmcall.h>
#include <aros/config.h>
#include <dos/dos.h>
#include <dos/stdio.h>
#include <proto/dos.h>
#include <aros/debug.h>
#include "dos_intern.h"

#ifdef AROS_TARGET_IE_M68K
#include <exec/semaphores.h>
#include <proto/exec.h>
#include <ie_hwreg.h>

#define IE_DOS_MMIO_SEM_NAME "ie.dos.mmio"

static inline void ie_dos_write32(ULONG reg, ULONG val)
{
    *((volatile ULONG *)reg) = val;
}

static struct SignalSemaphore *ObtainIntuitionEngineDOSMMIOSemaphore(void)
{
    struct SignalSemaphore *sem;

    Forbid();
    sem = FindSemaphore(IE_DOS_MMIO_SEM_NAME);
    if (sem)
        ObtainSemaphore(sem);
    Permit();
    return sem;
}

static void NotifyIntuitionEngineLoadSegSymbols(BPTR file, CONST_STRPTR name,
    BPTR segs, struct DosLibrary *DOSBase)
{
    char path[512];
    CONST_STRPTR symname = name;
    struct SignalSemaphore *sem;
    ULONG base;

    (void)DOSBase;

    if (segs == BNULL)
        return;

    if (NameFromFH(file, path, sizeof(path)))
        symname = path;

    sem = ObtainIntuitionEngineDOSMMIOSemaphore();
    if (!sem)
        return;

    base = (ULONG)((IPTR)BADDR(segs) + sizeof(IPTR));
    ie_dos_write32(IE_DOS_ARG1, (ULONG)(IPTR)symname);
    ie_dos_write32(IE_DOS_ARG2, 0);
    ie_dos_write32(IE_DOS_ARG3, base);
    ie_dos_write32(IE_DOS_CMD, IE_DOS_CMD_LOADSEG_SYMS);

    ReleaseSemaphore(sem);
}
#else
static void NotifyIntuitionEngineLoadSegSymbols(BPTR file, CONST_STRPTR name,
    BPTR segs, struct DosLibrary *DOSBase)
{
    (void)file;
    (void)name;
    (void)segs;
    (void)DOSBase;
}
#endif

static AROS_UFH4(LONG, ReadFunc,
        AROS_UFHA(BPTR, file,   D1),
        AROS_UFHA(APTR, buffer, D2),
        AROS_UFHA(LONG, length, D3),
        AROS_UFHA(struct DosLibrary *, DOSBase, A6)
)
{
    AROS_USERFUNC_INIT

    return Read(file, buffer, length);

    AROS_USERFUNC_EXIT
}

static AROS_UFH4(LONG, SeekFunc,
        AROS_UFHA(BPTR, file,  D1),
        AROS_UFHA(LONG,   pos, D2),
        AROS_UFHA(LONG,  mode, D3),
        AROS_UFHA(struct DosLibrary *, DOSBase, A6)
)
{
    AROS_USERFUNC_INIT

    return Seek(file, pos, mode);

    AROS_USERFUNC_EXIT
}


static AROS_UFH3(APTR, AllocFunc,
        AROS_UFHA(ULONG, length, D0),
        AROS_UFHA(ULONG, flags,  D1),
        AROS_UFHA(struct ExecBase *, SysBase, A6)
)
{
    AROS_USERFUNC_INIT

    return AllocMem(length, flags);

    AROS_USERFUNC_EXIT
}

static AROS_UFH3(void, FreeFunc,
        AROS_UFHA(APTR, buffer, A1),
        AROS_UFHA(ULONG, length, D0),
        AROS_UFHA(struct ExecBase *, SysBase, A6)
)
{
    AROS_USERFUNC_INIT

    FreeMem(buffer, length);

    AROS_USERFUNC_EXIT
}

/*****************************************************************************

    NAME */
#include <proto/dos.h>

        AROS_LH1(BPTR, LoadSeg,

/*  SYNOPSIS */
        AROS_LHA(CONST_STRPTR, name, D1),

/*  LOCATION */
        struct DosLibrary *, DOSBase, 25, Dos)

/*  FUNCTION
        Loads an executable file into memory. Each hunk of the loadfile
        is loaded into its own memory section and a handle on all of them
        is returned. The segments can be freed with UnLoadSeg().

    INPUTS
        name - NUL terminated name of the file.

    RESULT
        Handle to the loaded executable or NULL if the load failed.
        IoErr() gives additional information in that case.

    NOTES
        This function is built on top of InternalLoadSeg()

    EXAMPLE

    BUGS

    SEE ALSO
        UnLoadSeg()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    BPTR file, segs=0;
    SIPTR err;
    LONG_FUNC FunctionArray[] = {
        (LONG_FUNC)ReadFunc,
        (LONG_FUNC)AllocFunc,
        (LONG_FUNC)FreeFunc,
        (LONG_FUNC)SeekFunc,    /* Only needed for ELF */
    };

    /* Open the file */
    D(bug("[LoadSeg] Opening '%s'...\n", name));
    file = Open (name, MODE_OLDFILE);

    if (file)
    {
        D(bug("[LoadSeg] Loading '%s'...\n", name));

        SetVBuf(file, NULL, BUF_FULL, 4096);
        segs = InternalLoadSeg(file, BNULL, FunctionArray, NULL);
        /* We cache the IoErr(), since Close() will alter it */
        err = IoErr();

        D(if (segs == BNULL)
            bug("[LoadSeg] Failed to load '%s'\n", name));
#if (AROS_FLAVOUR & AROS_FLAVOUR_BINCOMPAT)
        if (segs != BNULL)
        {
            BPTR loadedSegs = segs;
            if ((LONG)loadedSegs < 0)
                loadedSegs = (BPTR)-((LONG)loadedSegs);
            NotifyIntuitionEngineLoadSegSymbols(file, name, loadedSegs, DOSBase);
        }

        /* overlayed executables return -segs and handle must not be closed */
        if (segs == BNULL || (LONG)segs > 0)
            Close(file);
        else
            segs = (BPTR)-((LONG)segs);
#else
        NotifyIntuitionEngineLoadSegSymbols(file, name, segs, DOSBase);
        Close(file);
#endif
        SetIoErr(err);
    }

    D(bug("[LoadSeg] Returns '%p', IoErr=%d\n", segs, IoErr()));
  

    /* And return */
    return segs;

    AROS_LIBFUNC_EXIT
} /* LoadSeg */

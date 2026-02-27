/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE Host Filesystem handler header.
*/

#ifndef _IEHANDLER_H
#define _IEHANDLER_H

#include <exec/libraries.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>

#include "ie_hwreg.h"

/* Handler context — allocated during startup, lives for handler lifetime */
struct IEHandler
{
    struct MsgPort *proc_port;      /* Our process message port */
    struct DosList *volume;         /* Registered volume entry */
    struct Library *dosBase;        /* dos.library */
    struct Library *utilityBase;    /* utility.library (if needed) */
    BOOL           running;         /* Main loop flag */
};

/* Lock structure — stored in fl_Key of AROS FileLock */
struct IELock
{
    ULONG key;          /* MMIO lock key (from Go side) */
    LONG  access;       /* SHARED_LOCK or EXCLUSIVE_LOCK */
};

/* File handle context — stored in fh_Arg1 of AROS FileHandle */
struct IEFileHandle
{
    ULONG key;          /* MMIO file handle key (from Go side) */
};

/* MMIO helpers — write args, trigger command, read results */
static inline void ie_dos_set_arg1(ULONG val)
{
    ie_write32(IE_DOS_ARG1, val);
}

static inline void ie_dos_set_arg2(ULONG val)
{
    ie_write32(IE_DOS_ARG2, val);
}

static inline void ie_dos_set_arg3(ULONG val)
{
    ie_write32(IE_DOS_ARG3, val);
}

static inline void ie_dos_set_arg4(ULONG val)
{
    ie_write32(IE_DOS_ARG4, val);
}

static inline void ie_dos_command(ULONG cmd)
{
    ie_write32(IE_DOS_CMD, cmd);
}

static inline ULONG ie_dos_result1(void)
{
    return ie_read32(IE_DOS_RESULT1);
}

static inline ULONG ie_dos_result2(void)
{
    return ie_read32(IE_DOS_RESULT2);
}

#endif /* _IEHANDLER_H */

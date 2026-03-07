/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE Host Filesystem handler.
          Translates AmigaDOS packets to IE MMIO calls for host filesystem
          access. The Go side (aros_dos_intercept.go) performs actual host
          I/O and returns results via MMIO registers.
*/

#define DEBUG 0
#include <aros/debug.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>

/* Handler manages its own library bases — __NOLIBBASE__ prevents autoinit.
   Define the base variables before proto headers so the inline stubs work. */
struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

#define SysBase SysBase
#define DOSBase DOSBase

#include <proto/exec.h>
#include <proto/dos.h>

#include "handler.h"

/* Inline string helpers — avoids linking stdc (which drags in ADD2LIBS/autoinit) */
static inline ULONG ie_strlen(const char *s)
{
    ULONG n = 0;
    while (s[n]) n++;
    return n;
}

static inline void ie_strncpy(char *dst, const char *src, ULONG max)
{
    ULONG i;
    for (i = 0; i < max && src[i]; i++)
        dst[i] = src[i];
    dst[i < max ? i : max] = '\0';
}

/* File-scope volume pointer — set during handler init, used by lock creators */
static struct DosList *g_volume;

/* ========================================================================
 * Packet helper functions
 * ======================================================================== */

static struct DosPacket *GetPacket(struct MsgPort *port)
{
    struct Message *msg = GetMsg(port);
    if (!msg)
        return NULL;
    return (struct DosPacket *)msg->mn_Node.ln_Name;
}

static void ReplyPacket(struct MsgPort *proc_port, struct DosPacket *pkt,
                        SIPTR res1, LONG res2)
{
    struct MsgPort *reply = pkt->dp_Port;
    pkt->dp_Port = proc_port;
    pkt->dp_Res1 = res1;
    pkt->dp_Res2 = res2;
    PutMsg(reply, pkt->dp_Link);
}

/* Convert AROS BSTR to C string in static buffer */
static char bstr_buf[256];
static char *BStr(BSTR bstr)
{
    UBYTE *b = (UBYTE *)BADDR(bstr);
    UBYTE len = b[0];
    if (len > 254) len = 254;
    CopyMem(b + 1, bstr_buf, len);
    bstr_buf[len] = '\0';
    return bstr_buf;
}

/* Write a C string to guest memory at the given address for MMIO path arg.
 * Returns the address (unchanged). The Go side reads it via bus.Read8(). */
static UBYTE name_buf[256];
static ULONG PrepareNameArg(const char *name)
{
    ULONG i;
    ULONG len = ie_strlen(name);
    if (len > 254) len = 254;
    for (i = 0; i < len; i++)
        name_buf[i] = name[i];
    name_buf[len] = 0;
    return (ULONG)name_buf;
}

/* ========================================================================
 * Packet dispatch — each ACTION_* maps to an IE_DOS_CMD_* MMIO call
 * ======================================================================== */

static SIPTR HandleLocateObject(struct DosPacket *pkt)
{
    ULONG parent_key = 0;
    struct FileLock *parent = (struct FileLock *)BADDR(pkt->dp_Arg1);
    char *name;
    struct IELock *iel;
    struct FileLock *fl;
    ULONG key;

    if (parent)
        parent_key = ((struct IELock *)parent->fl_Key)->key;

    name = BStr(pkt->dp_Arg2);
    ie_dos_set_arg1(PrepareNameArg(name));
    ie_dos_set_arg2(parent_key);
    ie_dos_set_arg3((ULONG)pkt->dp_Arg3); /* access mode */
    ie_dos_command(IE_DOS_CMD_LOCK);

    key = ie_dos_result1();
    if (key == 0) {
        pkt->dp_Res2 = ie_dos_result2();
        return DOSFALSE;
    }

    /* Allocate IELock + FileLock */
    iel = AllocVec(sizeof(struct IELock), MEMF_CLEAR);
    fl = AllocVec(sizeof(struct FileLock), MEMF_CLEAR);
    if (!iel || !fl) {
        if (iel) FreeVec(iel);
        if (fl) FreeVec(fl);
        /* Tell Go side to release the lock */
        ie_dos_set_arg1(key);
        ie_dos_command(IE_DOS_CMD_UNLOCK);
        pkt->dp_Res2 = ERROR_NO_FREE_STORE;
        return DOSFALSE;
    }

    iel->key = key;
    iel->access = pkt->dp_Arg3;

    fl->fl_Key = (IPTR)iel;
    fl->fl_Access = pkt->dp_Arg3;
    fl->fl_Task = &((struct Process *)FindTask(NULL))->pr_MsgPort;
    fl->fl_Volume = MKBADDR(g_volume);

    return (SIPTR)MKBADDR(fl);
}

static SIPTR HandleFreeLock(struct DosPacket *pkt)
{
    struct FileLock *fl = (struct FileLock *)BADDR(pkt->dp_Arg1);
    struct IELock *iel;

    if (!fl)
        return DOSTRUE;

    iel = (struct IELock *)fl->fl_Key;
    if (iel) {
        ie_dos_set_arg1(iel->key);
        ie_dos_command(IE_DOS_CMD_UNLOCK);
        FreeVec(iel);
    }
    FreeVec(fl);
    return DOSTRUE;
}

static SIPTR HandleCopyDir(struct DosPacket *pkt)
{
    struct FileLock *src = (struct FileLock *)BADDR(pkt->dp_Arg1);
    struct IELock *src_iel;
    struct IELock *iel;
    struct FileLock *fl;
    ULONG key;

    if (!src)
        return 0; /* dup of NULL lock = NULL */

    src_iel = (struct IELock *)src->fl_Key;
    ie_dos_set_arg1(src_iel->key);
    ie_dos_command(IE_DOS_CMD_DUPLOCK);

    key = ie_dos_result1();
    if (key == 0) {
        pkt->dp_Res2 = ie_dos_result2();
        return DOSFALSE;
    }

    iel = AllocVec(sizeof(struct IELock), MEMF_CLEAR);
    fl = AllocVec(sizeof(struct FileLock), MEMF_CLEAR);
    if (!iel || !fl) {
        if (iel) FreeVec(iel);
        if (fl) FreeVec(fl);
        ie_dos_set_arg1(key);
        ie_dos_command(IE_DOS_CMD_UNLOCK);
        pkt->dp_Res2 = ERROR_NO_FREE_STORE;
        return DOSFALSE;
    }

    iel->key = key;
    iel->access = SHARED_LOCK;
    fl->fl_Key = (IPTR)iel;
    fl->fl_Access = SHARED_LOCK;
    fl->fl_Task = &((struct Process *)FindTask(NULL))->pr_MsgPort;
    fl->fl_Volume = MKBADDR(g_volume);

    return (SIPTR)MKBADDR(fl);
}

static SIPTR HandleParent(struct DosPacket *pkt)
{
    struct FileLock *fl = (struct FileLock *)BADDR(pkt->dp_Arg1);
    struct IELock *iel;
    struct IELock *new_iel;
    struct FileLock *new_fl;
    ULONG parent_key = 0;
    ULONG key;

    if (fl) {
        iel = (struct IELock *)fl->fl_Key;
        parent_key = iel->key;
    }

    ie_dos_set_arg1(parent_key);
    ie_dos_command(IE_DOS_CMD_PARENT);

    key = ie_dos_result1();
    if (key == 0) {
        /* At root — return NULL lock (valid, not error) */
        pkt->dp_Res2 = 0;
        return 0;
    }

    new_iel = AllocVec(sizeof(struct IELock), MEMF_CLEAR);
    new_fl = AllocVec(sizeof(struct FileLock), MEMF_CLEAR);
    if (!new_iel || !new_fl) {
        if (new_iel) FreeVec(new_iel);
        if (new_fl) FreeVec(new_fl);
        ie_dos_set_arg1(key);
        ie_dos_command(IE_DOS_CMD_UNLOCK);
        pkt->dp_Res2 = ERROR_NO_FREE_STORE;
        return DOSFALSE;
    }

    new_iel->key = key;
    new_iel->access = SHARED_LOCK;
    new_fl->fl_Key = (IPTR)new_iel;
    new_fl->fl_Access = SHARED_LOCK;
    new_fl->fl_Task = &((struct Process *)FindTask(NULL))->pr_MsgPort;
    new_fl->fl_Volume = MKBADDR(g_volume);

    return (SIPTR)MKBADDR(new_fl);
}

/* ACTION_PARENT_FH: dp_Arg1 is fh_Arg1 (raw IEFileHandle ptr, NOT a lock BPTR) */
static SIPTR HandleParentFH(struct DosPacket *pkt)
{
    struct IEFileHandle *iefh = (struct IEFileHandle *)pkt->dp_Arg1;
    struct IELock *new_iel;
    struct FileLock *new_fl;
    ULONG parent_key = 0;
    ULONG key;

    if (iefh)
        parent_key = iefh->key;

    ie_dos_set_arg1(parent_key);
    ie_dos_command(IE_DOS_CMD_PARENT);

    key = ie_dos_result1();
    if (key == 0) {
        pkt->dp_Res2 = 0;
        return 0;
    }

    new_iel = AllocVec(sizeof(struct IELock), MEMF_CLEAR);
    new_fl = AllocVec(sizeof(struct FileLock), MEMF_CLEAR);
    if (!new_iel || !new_fl) {
        if (new_iel) FreeVec(new_iel);
        if (new_fl) FreeVec(new_fl);
        ie_dos_set_arg1(key);
        ie_dos_command(IE_DOS_CMD_UNLOCK);
        pkt->dp_Res2 = ERROR_NO_FREE_STORE;
        return DOSFALSE;
    }

    new_iel->key = key;
    new_iel->access = SHARED_LOCK;
    new_fl->fl_Key = (IPTR)new_iel;
    new_fl->fl_Access = SHARED_LOCK;
    new_fl->fl_Task = &((struct Process *)FindTask(NULL))->pr_MsgPort;
    new_fl->fl_Volume = MKBADDR(g_volume);

    return (SIPTR)MKBADDR(new_fl);
}

/* ACTION_COPY_DIR_FH: dp_Arg1 is fh_Arg1 (raw IEFileHandle ptr, NOT a lock BPTR) */
static SIPTR HandleCopyDirFH(struct DosPacket *pkt)
{
    struct IEFileHandle *iefh = (struct IEFileHandle *)pkt->dp_Arg1;
    struct IELock *iel;
    struct FileLock *fl;
    ULONG key;
    ULONG src_key = 0;

    if (!iefh)
        return 0;

    src_key = iefh->key;
    ie_dos_set_arg1(src_key);
    ie_dos_command(IE_DOS_CMD_DUPLOCK);

    key = ie_dos_result1();
    if (key == 0) {
        pkt->dp_Res2 = ie_dos_result2();
        return DOSFALSE;
    }

    iel = AllocVec(sizeof(struct IELock), MEMF_CLEAR);
    fl = AllocVec(sizeof(struct FileLock), MEMF_CLEAR);
    if (!iel || !fl) {
        if (iel) FreeVec(iel);
        if (fl) FreeVec(fl);
        ie_dos_set_arg1(key);
        ie_dos_command(IE_DOS_CMD_UNLOCK);
        pkt->dp_Res2 = ERROR_NO_FREE_STORE;
        return DOSFALSE;
    }

    iel->key = key;
    iel->access = SHARED_LOCK;
    fl->fl_Key = (IPTR)iel;
    fl->fl_Access = SHARED_LOCK;
    fl->fl_Task = &((struct Process *)FindTask(NULL))->pr_MsgPort;
    fl->fl_Volume = MKBADDR(g_volume);

    return (SIPTR)MKBADDR(fl);
}

static SIPTR HandleSameLock(struct DosPacket *pkt)
{
    struct FileLock *fl1 = (struct FileLock *)BADDR(pkt->dp_Arg1);
    struct FileLock *fl2 = (struct FileLock *)BADDR(pkt->dp_Arg2);
    ULONG key1 = 0, key2 = 0;

    if (fl1) key1 = ((struct IELock *)fl1->fl_Key)->key;
    if (fl2) key2 = ((struct IELock *)fl2->fl_Key)->key;

    ie_dos_set_arg1(key1);
    ie_dos_set_arg2(key2);
    ie_dos_command(IE_DOS_CMD_SAMELOCK);

    /* Go side returns LOCK_SAME (0) / LOCK_SAME_VOLUME (1) / LOCK_DIFFERENT.
     * DOS packet semantics: dp_Res1 non-zero = "same lock" (DOSTRUE).
     * Translate: only LOCK_SAME (0) means identical → DOSTRUE; else DOSFALSE. */
    return (ie_dos_result1() == LOCK_SAME) ? DOSTRUE : DOSFALSE;
}

/* ========================================================================
 * Directory examination
 * ======================================================================== */

static SIPTR HandleExamineObject(struct DosPacket *pkt)
{
    struct FileLock *fl = (struct FileLock *)BADDR(pkt->dp_Arg1);
    struct FileInfoBlock *fib = (struct FileInfoBlock *)BADDR(pkt->dp_Arg2);
    ULONG lock_key = 0;

    if (fl)
        lock_key = ((struct IELock *)fl->fl_Key)->key;

    ie_dos_set_arg1(lock_key);
    ie_dos_set_arg2((ULONG)fib);
    ie_dos_command(IE_DOS_CMD_EXAMINE);

    if (ie_dos_result1() == 0) {
        pkt->dp_Res2 = ie_dos_result2();
        return DOSFALSE;
    }
    return DOSTRUE;
}

static SIPTR HandleExamineNext(struct DosPacket *pkt)
{
    struct FileLock *fl = (struct FileLock *)BADDR(pkt->dp_Arg1);
    struct FileInfoBlock *fib = (struct FileInfoBlock *)BADDR(pkt->dp_Arg2);
    ULONG lock_key = 0;

    if (fl)
        lock_key = ((struct IELock *)fl->fl_Key)->key;

    ie_dos_set_arg1(lock_key);
    ie_dos_set_arg2((ULONG)fib);
    ie_dos_command(IE_DOS_CMD_EXNEXT);

    if (ie_dos_result1() == 0) {
        pkt->dp_Res2 = ie_dos_result2();
        return DOSFALSE;
    }
    return DOSTRUE;
}

/* ========================================================================
 * File operations
 * ======================================================================== */

static SIPTR HandleFind(struct DosPacket *pkt, ULONG cmd)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(pkt->dp_Arg1);
    struct FileLock *dir_lock = (struct FileLock *)BADDR(pkt->dp_Arg2);
    char *name = BStr(pkt->dp_Arg3);
    ULONG parent_key = 0;
    struct IEFileHandle *iefh;
    ULONG handle_key;

    if (dir_lock)
        parent_key = ((struct IELock *)dir_lock->fl_Key)->key;

    ie_dos_set_arg1(PrepareNameArg(name));
    ie_dos_set_arg2(parent_key);
    ie_dos_command(cmd);

    handle_key = ie_dos_result1();
    if (handle_key == 0) {
        pkt->dp_Res2 = ie_dos_result2();
        return DOSFALSE;
    }

    iefh = AllocVec(sizeof(struct IEFileHandle), MEMF_CLEAR);
    if (!iefh) {
        ie_dos_set_arg1(handle_key);
        ie_dos_command(IE_DOS_CMD_CLOSE);
        pkt->dp_Res2 = ERROR_NO_FREE_STORE;
        return DOSFALSE;
    }

    iefh->key = handle_key;
    fh->fh_Arg1 = (SIPTR)iefh;
    fh->fh_Port = DOSTRUE; /* interactive = FALSE via non-zero */

    return DOSTRUE;
}

static SIPTR HandleEnd(struct DosPacket *pkt)
{
    struct IEFileHandle *iefh = (struct IEFileHandle *)pkt->dp_Arg1;

    if (iefh) {
        ie_dos_set_arg1(iefh->key);
        ie_dos_command(IE_DOS_CMD_CLOSE);
        FreeVec(iefh);
    }
    return DOSTRUE;
}

static SIPTR HandleRead(struct DosPacket *pkt)
{
    struct IEFileHandle *iefh = (struct IEFileHandle *)pkt->dp_Arg1;
    APTR buffer = (APTR)pkt->dp_Arg2;
    LONG length = pkt->dp_Arg3;
    ULONG result;

    if (!iefh) {
        pkt->dp_Res2 = ERROR_OBJECT_WRONG_TYPE;
        return -1;
    }

    ie_dos_set_arg1(iefh->key);
    ie_dos_set_arg2((ULONG)buffer);
    ie_dos_set_arg3((ULONG)length);
    ie_dos_command(IE_DOS_CMD_READ);

    result = ie_dos_result1();
    if (result == 0xFFFFFFFF) {
        pkt->dp_Res2 = ie_dos_result2();
        return -1;
    }
    return (SIPTR)result;
}

static SIPTR HandleWrite(struct DosPacket *pkt)
{
    struct IEFileHandle *iefh = (struct IEFileHandle *)pkt->dp_Arg1;
    APTR buffer = (APTR)pkt->dp_Arg2;
    LONG length = pkt->dp_Arg3;
    ULONG result;

    if (!iefh) {
        pkt->dp_Res2 = ERROR_OBJECT_WRONG_TYPE;
        return -1;
    }

    ie_dos_set_arg1(iefh->key);
    ie_dos_set_arg2((ULONG)buffer);
    ie_dos_set_arg3((ULONG)length);
    ie_dos_command(IE_DOS_CMD_WRITE);

    result = ie_dos_result1();
    if (result == 0xFFFFFFFF) {
        pkt->dp_Res2 = ie_dos_result2();
        return -1;
    }
    return (SIPTR)result;
}

static SIPTR HandleSeek(struct DosPacket *pkt)
{
    struct IEFileHandle *iefh = (struct IEFileHandle *)pkt->dp_Arg1;
    LONG offset = pkt->dp_Arg2;
    LONG mode = pkt->dp_Arg3;
    ULONG result;

    if (!iefh) {
        pkt->dp_Res2 = ERROR_OBJECT_WRONG_TYPE;
        return -1;
    }

    ie_dos_set_arg1(iefh->key);
    ie_dos_set_arg2((ULONG)offset);
    ie_dos_set_arg3((ULONG)mode);
    ie_dos_command(IE_DOS_CMD_SEEK);

    result = ie_dos_result1();
    if (result == 0xFFFFFFFF) {
        pkt->dp_Res2 = ie_dos_result2();
        return -1;
    }
    return (SIPTR)result;
}

static SIPTR HandleSetFileSize(struct DosPacket *pkt)
{
    struct IEFileHandle *iefh = (struct IEFileHandle *)pkt->dp_Arg1;
    LONG newsize = pkt->dp_Arg2;
    LONG mode = pkt->dp_Arg3;
    ULONG result;

    if (!iefh) {
        pkt->dp_Res2 = ERROR_OBJECT_WRONG_TYPE;
        return -1;
    }

    ie_dos_set_arg1(iefh->key);
    ie_dos_set_arg2((ULONG)newsize);
    ie_dos_set_arg3((ULONG)mode);
    ie_dos_command(IE_DOS_CMD_SET_FILESIZE);

    result = ie_dos_result1();
    if (result == 0xFFFFFFFF) {
        pkt->dp_Res2 = ie_dos_result2();
        return -1;
    }
    return (SIPTR)result;
}

/* ========================================================================
 * Filesystem modification operations
 * ======================================================================== */

static SIPTR HandleDeleteObject(struct DosPacket *pkt)
{
    struct FileLock *fl = (struct FileLock *)BADDR(pkt->dp_Arg1);
    char *name = BStr(pkt->dp_Arg2);
    ULONG parent_key = 0;

    if (fl)
        parent_key = ((struct IELock *)fl->fl_Key)->key;

    ie_dos_set_arg1(parent_key);
    ie_dos_set_arg2(PrepareNameArg(name));
    ie_dos_command(IE_DOS_CMD_DELETE);

    if (ie_dos_result1() == 0) {
        pkt->dp_Res2 = ie_dos_result2();
        return DOSFALSE;
    }
    return DOSTRUE;
}

static SIPTR HandleCreateDir(struct DosPacket *pkt)
{
    struct FileLock *fl = (struct FileLock *)BADDR(pkt->dp_Arg1);
    char *name = BStr(pkt->dp_Arg2);
    ULONG parent_key = 0;
    struct IELock *iel;
    struct FileLock *new_fl;
    ULONG key;

    if (fl)
        parent_key = ((struct IELock *)fl->fl_Key)->key;

    ie_dos_set_arg1(parent_key);
    ie_dos_set_arg2(PrepareNameArg(name));
    ie_dos_command(IE_DOS_CMD_CREATEDIR);

    key = ie_dos_result1();
    if (key == 0) {
        pkt->dp_Res2 = ie_dos_result2();
        return DOSFALSE;
    }

    iel = AllocVec(sizeof(struct IELock), MEMF_CLEAR);
    new_fl = AllocVec(sizeof(struct FileLock), MEMF_CLEAR);
    if (!iel || !new_fl) {
        if (iel) FreeVec(iel);
        if (new_fl) FreeVec(new_fl);
        ie_dos_set_arg1(key);
        ie_dos_command(IE_DOS_CMD_UNLOCK);
        pkt->dp_Res2 = ERROR_NO_FREE_STORE;
        return DOSFALSE;
    }

    iel->key = key;
    iel->access = EXCLUSIVE_LOCK;
    new_fl->fl_Key = (IPTR)iel;
    new_fl->fl_Access = EXCLUSIVE_LOCK;
    new_fl->fl_Task = &((struct Process *)FindTask(NULL))->pr_MsgPort;
    new_fl->fl_Volume = MKBADDR(g_volume);

    return (SIPTR)MKBADDR(new_fl);
}

static SIPTR HandleRenameObject(struct DosPacket *pkt)
{
    struct FileLock *src_fl = (struct FileLock *)BADDR(pkt->dp_Arg1);
    struct FileLock *dst_fl = (struct FileLock *)BADDR(pkt->dp_Arg3);
    char src_name[256], dst_name[256];
    ULONG src_key = 0, dst_key = 0;
    char *tmp;

    if (src_fl)
        src_key = ((struct IELock *)src_fl->fl_Key)->key;
    if (dst_fl)
        dst_key = ((struct IELock *)dst_fl->fl_Key)->key;

    /* Copy BSTR names — BStr uses static buffer, so copy before second call */
    tmp = BStr(pkt->dp_Arg2);
    ie_strncpy(src_name, tmp, 255);
    src_name[255] = '\0';

    tmp = BStr(pkt->dp_Arg4);
    ie_strncpy(dst_name, tmp, 255);
    dst_name[255] = '\0';

    ie_dos_set_arg1(src_key);
    ie_dos_set_arg2(PrepareNameArg(src_name));
    ie_dos_set_arg3(dst_key);
    ie_dos_set_arg4(PrepareNameArg(dst_name));
    ie_dos_command(IE_DOS_CMD_RENAME);

    if (ie_dos_result1() == 0) {
        pkt->dp_Res2 = ie_dos_result2();
        return DOSFALSE;
    }
    return DOSTRUE;
}

static SIPTR HandleSetProtect(struct DosPacket *pkt)
{
    /* No-op: always succeed (host filesystem has its own permissions) */
    return DOSTRUE;
}

static SIPTR HandleInfo(struct DosPacket *pkt)
{
    /* ACTION_INFO: dp_Arg1 = lock BPTR, dp_Arg2 = InfoData BPTR */
    struct InfoData *id = (struct InfoData *)BADDR(pkt->dp_Arg2);

    ie_dos_set_arg1((ULONG)id);
    ie_dos_command(IE_DOS_CMD_DISKINFO);

    return DOSTRUE;
}

static SIPTR HandleDiskInfo(struct DosPacket *pkt)
{
    struct InfoData *id = (struct InfoData *)BADDR(pkt->dp_Arg1);

    ie_dos_set_arg1((ULONG)id);
    ie_dos_command(IE_DOS_CMD_DISKINFO);

    return DOSTRUE;
}

static SIPTR HandleExamineFH(struct DosPacket *pkt)
{
    struct IEFileHandle *iefh = (struct IEFileHandle *)pkt->dp_Arg1;
    struct FileInfoBlock *fib = (struct FileInfoBlock *)BADDR(pkt->dp_Arg2);

    if (!iefh) {
        pkt->dp_Res2 = ERROR_OBJECT_WRONG_TYPE;
        return DOSFALSE;
    }

    ie_dos_set_arg1(iefh->key);
    ie_dos_set_arg2((ULONG)fib);
    ie_dos_command(IE_DOS_CMD_EXAMINE_FH);

    if (ie_dos_result1() == 0) {
        pkt->dp_Res2 = ie_dos_result2();
        return DOSFALSE;
    }
    return DOSTRUE;
}

/* ========================================================================
 * Main handler entry point and packet loop
 * ======================================================================== */

LONG IEHandlerMain(struct ExecBase *sysBase)
{
    SysBase = sysBase;
    struct Process *proc = (struct Process *)FindTask(NULL);
    struct MsgPort *proc_port = &proc->pr_MsgPort;
    struct DosPacket *startup_pkt;
    struct DeviceNode *dev_node;
    struct DosList *volume;
    struct IEHandler handler;
    BOOL running = TRUE;

    D(bug("[iehandler] Starting up\n"));

    /* Wait for startup packet from dos.library */
    WaitPort(proc_port);
    startup_pkt = GetPacket(proc_port);
    if (!startup_pkt) {
        D(bug("[iehandler] No startup packet!\n"));
        return RETURN_FAIL;
    }

    /* Extract device node from startup packet */
    dev_node = (struct DeviceNode *)BADDR(startup_pkt->dp_Arg3);

    /* Initialise handler context */
    /* Zero-init handler context (avoid string.h/stdc dependency) */
    {
        UBYTE *p = (UBYTE *)&handler;
        ULONG i;
        for (i = 0; i < sizeof(handler); i++) p[i] = 0;
    }
    handler.proc_port = proc_port;
    handler.running = TRUE;

    handler.dosBase = OpenLibrary("dos.library", 39);
    if (!handler.dosBase) {
        D(bug("[iehandler] Cannot open dos.library!\n"));
        ReplyPacket(proc_port, startup_pkt, DOSFALSE, ERROR_INVALID_RESIDENT_LIBRARY);
        return RETURN_FAIL;
    }

    DOSBase = (struct DosLibrary *)handler.dosBase;

    /* Create and register volume entry */
    volume = MakeDosEntry("IE", DLT_VOLUME);
    if (volume) {
        volume->dol_Task = proc_port;
        volume->dol_misc.dol_volume.dol_DiskType = ID_DOS_DISK;
        DateStamp(&volume->dol_misc.dol_volume.dol_VolumeDate);
        AddDosEntry(volume);
        handler.volume = volume;
        g_volume = volume;
    }

    /* Set device node task to our port */
    dev_node->dn_Task = proc_port;

    /* Reply startup packet — we're ready */
    D(bug("[iehandler] Ready, replying startup packet\n"));
    ReplyPacket(proc_port, startup_pkt, DOSTRUE, 0);

    /* === Main packet dispatch loop === */

    while (running) {
        struct DosPacket *pkt;

        WaitPort(proc_port);

        while ((pkt = GetPacket(proc_port)) != NULL) {
            SIPTR res1 = DOSTRUE;
            LONG  res2 = 0;

            D(bug("[iehandler] Packet type=%ld\n", (long)pkt->dp_Type));

            switch (pkt->dp_Type) {

            /* Lock operations */
            case ACTION_LOCATE_OBJECT:
                res1 = HandleLocateObject(pkt);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_FREE_LOCK:
                res1 = HandleFreeLock(pkt);
                break;

            case ACTION_COPY_DIR:
                res1 = HandleCopyDir(pkt);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_COPY_DIR_FH:
                res1 = HandleCopyDirFH(pkt);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_PARENT:
                res1 = HandleParent(pkt);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_PARENT_FH:
                res1 = HandleParentFH(pkt);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_SAME_LOCK:
                res1 = HandleSameLock(pkt);
                break;

            /* Directory examination */
            case ACTION_EXAMINE_OBJECT:
                res1 = HandleExamineObject(pkt);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_EXAMINE_NEXT:
                res1 = HandleExamineNext(pkt);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_EXAMINE_FH:
                res1 = HandleExamineFH(pkt);
                res2 = pkt->dp_Res2;
                break;

            /* File operations */
            case ACTION_FINDINPUT:
                res1 = HandleFind(pkt, IE_DOS_CMD_FINDINPUT);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_FINDOUTPUT:
                res1 = HandleFind(pkt, IE_DOS_CMD_FINDOUTPUT);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_FINDUPDATE:
                res1 = HandleFind(pkt, IE_DOS_CMD_FINDUPDATE);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_END:
                res1 = HandleEnd(pkt);
                break;

            case ACTION_READ:
                res1 = HandleRead(pkt);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_WRITE:
                res1 = HandleWrite(pkt);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_SEEK:
                res1 = HandleSeek(pkt);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_SET_FILE_SIZE:
                res1 = HandleSetFileSize(pkt);
                res2 = pkt->dp_Res2;
                break;

            /* Filesystem modification */
            case ACTION_DELETE_OBJECT:
                res1 = HandleDeleteObject(pkt);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_CREATE_DIR:
                res1 = HandleCreateDir(pkt);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_RENAME_OBJECT:
                res1 = HandleRenameObject(pkt);
                res2 = pkt->dp_Res2;
                break;

            case ACTION_SET_PROTECT:
                res1 = HandleSetProtect(pkt);
                break;

            /* Volume/disk info */
            case ACTION_INFO:
                res1 = HandleInfo(pkt);
                break;

            case ACTION_DISK_INFO:
                res1 = HandleDiskInfo(pkt);
                break;

            case ACTION_CURRENT_VOLUME:
                res1 = (SIPTR)(handler.volume ? MKBADDR(handler.volume) : 0);
                break;

            case ACTION_IS_FILESYSTEM:
                res1 = DOSTRUE;
                break;

            /* Handler lifecycle */
            case ACTION_DIE:
                running = FALSE;
                res1 = DOSTRUE;
                break;

            /* Unimplemented / no-op */
            case ACTION_FLUSH:
                res1 = DOSTRUE;
                break;

            case ACTION_SET_COMMENT:
            case ACTION_SET_DATE:
            case ACTION_RENAME_DISK:
                res1 = DOSTRUE; /* silently succeed */
                break;

            default:
                D(bug("[iehandler] Unknown action %ld\n", (long)pkt->dp_Type));
                res1 = DOSFALSE;
                res2 = ERROR_ACTION_NOT_KNOWN;
                break;
            }

            ReplyPacket(proc_port, pkt, res1, res2);
        }
    }

    /* Cleanup */
    if (handler.volume) {
        RemDosEntry(handler.volume);
        FreeDosEntry(handler.volume);
    }
    if (handler.dosBase)
        CloseLibrary(handler.dosBase);

    D(bug("[iehandler] Shutdown complete\n"));

    return RETURN_OK;
}

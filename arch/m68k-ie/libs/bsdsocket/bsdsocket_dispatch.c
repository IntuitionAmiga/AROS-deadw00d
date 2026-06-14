/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE host-backed bsdsocket.library v4 ABI dispatch.
*/

#include <aros/libcall.h>
#include <aros/asmcall.h>
#include <bsdsocket/socketbasetags.h>
#include <libraries/bsdsocket.h>

#include "bsdsocket_intern.h"

static void req_clear(struct IEBSDRequest *req)
{
    int i;
    for (i = 0; i < 24; i++)
        req->a[i] = 0;
}

void ieb_socket_set_errno(struct IEBSDSocketBase *base, LONG error)
{
    base->defErrno = error;
    if (base->errnoSize == 4)
        *(ULONG *)base->errnoPtr = (ULONG)error;
    else if (base->errnoSize == 2)
        *(UWORD *)base->errnoPtr = (UWORD)error;
    else
        *(UBYTE *)base->errnoPtr = (UBYTE)error;
}

LONG ieb_socket_get_errno(struct IEBSDSocketBase *base)
{
    if (base->errnoSize == 4)
        return *(ULONG *)base->errnoPtr;
    if (base->errnoSize == 2)
        return *(UWORD *)base->errnoPtr;
    return *(UBYTE *)base->errnoPtr;
}

void ieb_socket_set_herrno(struct IEBSDSocketBase *base, LONG error)
{
    base->defHErrno = error;
    if (base->hErrnoPtr)
        *base->hErrnoPtr = error;
}

LONG ieb_socket_call(struct IEBSDSocketBase *base, ULONG cmd, struct IEBSDRequest *req)
{
    ie_write32(IE_SOCK_REQ_PTR, (ULONG)req);
    ie_write32(IE_SOCK_REQ_LEN, sizeof(*req));
    ie_write32(IE_SOCK_CMD, cmd);

    ieb_socket_set_errno(base, ie_read32(IE_SOCK_ERRNO));
    ieb_socket_set_herrno(base, ie_read32(IE_SOCK_HERRNO));
    return (LONG)ie_read32(IE_SOCK_RES1);
}

LONG ieb_socket_fail(struct IEBSDSocketBase *base, LONG error)
{
    ieb_socket_set_errno(base, error);
    return -1;
}

LONG ieb_socket_unsupported(struct IEBSDSocketBase *base)
{
    return ieb_socket_fail(base, EOPNOTSUPP);
}

static LONG ieb_fd_callback(struct IEBSDSocketBase *base, LONG fd, LONG action)
{
    if (!base->fdCallback)
        return 0;

    return AROS_UFC2(int, (APTR)base->fdCallback,
        AROS_UFCA(int, fd, D0),
        AROS_UFCA(int, action, D1));
}

static LONG ieb_socket_validate_fd(struct IEBSDSocketBase *base, LONG fd)
{
    if (fd < 0 || (ULONG)fd >= base->dTableSize)
        return EBADF;
    return 0;
}

static LONG ieb_socket_validate_active_fd(struct IEBSDSocketBase *base, LONG fd)
{
    LONG error;

    if ((error = ieb_socket_validate_fd(base, fd)) != 0)
        return error;
    if (!base->dTableUsed[fd])
        return EBADF;
    return 0;
}

static LONG ieb_socket_close_host(struct IEBSDSocketBase *base, LONG s)
{
    struct IEBSDRequest req;
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    return ieb_socket_call(base, IE_SOCK_CMD_CLOSE, &req);
}

static LONG ieb_socket_release_host(struct IEBSDSocketBase *base, LONG s, LONG id)
{
    struct IEBSDRequest req;
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_AUX1] = id;
    return ieb_socket_call(base, IE_SOCK_CMD_RELEASE, &req);
}

static LONG ieb_socket_release_copy_host(struct IEBSDSocketBase *base, LONG s, LONG id)
{
    struct IEBSDRequest req;
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_AUX1] = id;
    return ieb_socket_call(base, IE_SOCK_CMD_RELEASECOPY, &req);
}

static LONG ieb_socket_finish_alloc(struct IEBSDSocketBase *base, LONG fd)
{
    LONG error;

    if (fd < 0)
        return fd;

    if ((error = ieb_socket_validate_fd(base, fd)) != 0)
    {
        ieb_socket_close_host(base, fd);
        return ieb_socket_fail(base, error);
    }

    if ((error = ieb_fd_callback(base, fd, FDCB_ALLOC)) != 0)
    {
        ieb_socket_close_host(base, fd);
        return ieb_socket_fail(base, error);
    }

    base->dTableUsed[fd] = TRUE;
    return fd;
}

static LONG ieb_socket_finish_free(struct IEBSDSocketBase *base, LONG fd)
{
    LONG error;

    if ((error = ieb_socket_validate_active_fd(base, fd)) != 0)
        return ieb_socket_fail(base, error);

    if ((error = ieb_fd_callback(base, fd, FDCB_FREE)) != 0)
        return ieb_socket_fail(base, error);

    base->dTableUsed[fd] = FALSE;
    return ieb_socket_close_host(base, fd);
}

static LONG ieb_socket_finish_release(struct IEBSDSocketBase *base, LONG fd, LONG id)
{
    LONG error;
    LONG releaseId;

    if ((error = ieb_socket_validate_active_fd(base, fd)) != 0)
        return ieb_socket_fail(base, error);

    releaseId = ieb_socket_release_host(base, fd, id);
    if (releaseId < 0)
        return releaseId;

    ieb_fd_callback(base, fd, FDCB_FREE);
    base->dTableUsed[fd] = FALSE;
    return releaseId;
}

static LONG ieb_socket_release_copy(struct IEBSDSocketBase *base, LONG fd, LONG id)
{
    LONG error;

    if ((error = ieb_socket_validate_active_fd(base, fd)) != 0)
        return ieb_socket_fail(base, error);

    return ieb_socket_release_copy_host(base, fd, id);
}

static struct TagItem *ieb_next_tag_item(struct TagItem **tagList)
{
    struct TagItem *tag = tagList ? *tagList : NULL;

    while (tag)
    {
        switch (tag->ti_Tag)
        {
            case TAG_DONE:
                *tagList = tag;
                return NULL;
            case TAG_IGNORE:
                tag++;
                break;
            case TAG_SKIP:
                tag += tag->ti_Data + 1;
                break;
            case TAG_MORE:
                tag = (struct TagItem *)tag->ti_Data;
                break;
            default:
                *tagList = tag + 1;
                return tag;
        }
    }

    if (tagList)
        *tagList = NULL;
    return NULL;
}

AROS_LH3(int, socket,
    AROS_LHA(int, domain, D0),
    AROS_LHA(int, type, D1),
    AROS_LHA(int, protocol, D2),
    struct IEBSDSocketBase *, SocketBase, 5, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    LONG fd;
    req_clear(&req);
    req.a[IEBSD_REQ_DOMAIN] = domain;
    req.a[IEBSD_REQ_TYPE] = type;
    req.a[IEBSD_REQ_PROTOCOL] = protocol;
    fd = ieb_socket_call(SocketBase, IE_SOCK_CMD_SOCKET, &req);
    return ieb_socket_finish_alloc(SocketBase, fd);
    AROS_LIBFUNC_EXIT
}

AROS_LH3(int, bind,
    AROS_LHA(int, s, D0),
    AROS_LHA(struct sockaddr *, name, A0),
    AROS_LHA(socklen_t, namelen, D1),
    struct IEBSDSocketBase *, SocketBase, 6, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    if (!name || namelen > IEBSD_MAX_SOCKADDR)
        return ieb_socket_fail(SocketBase, EINVAL);
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_PTR1] = (ULONG)name;
    req.a[IEBSD_REQ_LEN1] = namelen;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_BIND, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH2(int, listen,
    AROS_LHA(int, s, D0),
    AROS_LHA(int, backlog, D1),
    struct IEBSDSocketBase *, SocketBase, 7, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_AUX1] = backlog;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_LISTEN, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH3(int, accept,
    AROS_LHA(int, s, D0),
    AROS_LHA(struct sockaddr *, addr, A0),
    AROS_LHA(socklen_t *, addrlen, A1),
    struct IEBSDSocketBase *, SocketBase, 8, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_PTR1] = (ULONG)addr;
    req.a[IEBSD_REQ_PTR2] = (ULONG)addrlen;
    req.a[IEBSD_REQ_LEN1] = (addrlen != NULL) ? *addrlen : 0;
    return ieb_socket_finish_alloc(SocketBase, ieb_socket_call(SocketBase, IE_SOCK_CMD_ACCEPT, &req));
    AROS_LIBFUNC_EXIT
}

AROS_LH3(int, connect,
    AROS_LHA(int, s, D0),
    AROS_LHA(struct sockaddr *, name, A0),
    AROS_LHA(socklen_t, namelen, D1),
    struct IEBSDSocketBase *, SocketBase, 9, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    if (!name || namelen > IEBSD_MAX_SOCKADDR)
        return ieb_socket_fail(SocketBase, EINVAL);
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_PTR1] = (ULONG)name;
    req.a[IEBSD_REQ_LEN1] = namelen;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_CONNECT, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH6(int, sendto,
    AROS_LHA(int, s, D0),
    AROS_LHA(const void *, msg, A0),
    AROS_LHA(int, len, D1),
    AROS_LHA(int, flags, D2),
    AROS_LHA(const struct sockaddr *, to, A1),
    AROS_LHA(socklen_t, tolen, D3),
    struct IEBSDSocketBase *, SocketBase, 10, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    if (len < 0 || (ULONG)len > IEBSD_MAX_PAYLOAD || (!msg && len > 0))
        return ieb_socket_fail(SocketBase, (len < 0) ? EINVAL : EMSGSIZE);
    if (tolen > IEBSD_MAX_SOCKADDR)
        return ieb_socket_fail(SocketBase, EINVAL);
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_PTR1] = (ULONG)msg;
    req.a[IEBSD_REQ_LEN1] = len;
    req.a[IEBSD_REQ_FLAGS] = flags;
    req.a[IEBSD_REQ_PTR2] = (ULONG)to;
    req.a[IEBSD_REQ_LEN2] = tolen;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_SENDTO, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH4(int, send,
    AROS_LHA(int, s, D0),
    AROS_LHA(const void *, msg, A0),
    AROS_LHA(int, len, D1),
    AROS_LHA(int, flags, D2),
    struct IEBSDSocketBase *, SocketBase, 11, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    if (len < 0 || (ULONG)len > IEBSD_MAX_PAYLOAD || (!msg && len > 0))
        return ieb_socket_fail(SocketBase, (len < 0) ? EINVAL : EMSGSIZE);
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_PTR1] = (ULONG)msg;
    req.a[IEBSD_REQ_LEN1] = len;
    req.a[IEBSD_REQ_FLAGS] = flags;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_SENDTO, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH6(int, recvfrom,
    AROS_LHA(int, s, D0),
    AROS_LHA(void *, buf, A0),
    AROS_LHA(int, len, D1),
    AROS_LHA(int, flags, D2),
    AROS_LHA(struct sockaddr *, from, A1),
    AROS_LHA(socklen_t *, fromlen, A2),
    struct IEBSDSocketBase *, SocketBase, 12, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    if (len < 0 || (ULONG)len > IEBSD_MAX_PAYLOAD || (!buf && len > 0))
        return ieb_socket_fail(SocketBase, (len < 0) ? EINVAL : EMSGSIZE);
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_PTR1] = (ULONG)buf;
    req.a[IEBSD_REQ_LEN1] = len;
    req.a[IEBSD_REQ_FLAGS] = flags;
    req.a[IEBSD_REQ_PTR2] = (ULONG)from;
    req.a[IEBSD_REQ_AUX1] = (ULONG)fromlen;
    req.a[IEBSD_REQ_LEN2] = (fromlen != NULL) ? *fromlen : 0;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_RECVFROM, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH4(int, recv,
    AROS_LHA(int, s, D0),
    AROS_LHA(void *, buf, A0),
    AROS_LHA(int, len, D1),
    AROS_LHA(int, flags, D2),
    struct IEBSDSocketBase *, SocketBase, 13, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    if (len < 0 || (ULONG)len > IEBSD_MAX_PAYLOAD || (!buf && len > 0))
        return ieb_socket_fail(SocketBase, (len < 0) ? EINVAL : EMSGSIZE);
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_PTR1] = (ULONG)buf;
    req.a[IEBSD_REQ_LEN1] = len;
    req.a[IEBSD_REQ_FLAGS] = flags;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_RECVFROM, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH2(int, shutdown,
    AROS_LHA(int, s, D0),
    AROS_LHA(int, how, D1),
    struct IEBSDSocketBase *, SocketBase, 14, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_AUX1] = how;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_SHUTDOWN, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH5(int, setsockopt,
    AROS_LHA(int, s, D0),
    AROS_LHA(int, level, D1),
    AROS_LHA(int, optname, D2),
    AROS_LHA(void *, optval, A0),
    AROS_LHA(socklen_t, optlen, D3),
    struct IEBSDSocketBase *, SocketBase, 15, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_LEVEL] = level;
    req.a[IEBSD_REQ_OPTNAME] = optname;
    req.a[IEBSD_REQ_PTR1] = (ULONG)optval;
    req.a[IEBSD_REQ_LEN1] = optlen;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_SETSOCKOPT, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH5(int, getsockopt,
    AROS_LHA(int, s, D0),
    AROS_LHA(int, level, D1),
    AROS_LHA(int, optname, D2),
    AROS_LHA(void *, optval, A0),
    AROS_LHA(socklen_t *, optlen, A1),
    struct IEBSDSocketBase *, SocketBase, 16, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_LEVEL] = level;
    req.a[IEBSD_REQ_OPTNAME] = optname;
    req.a[IEBSD_REQ_PTR1] = (ULONG)optval;
    req.a[IEBSD_REQ_PTR2] = (ULONG)optlen;
    req.a[IEBSD_REQ_LEN1] = (optlen != NULL) ? *optlen : 0;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_GETSOCKOPT, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH3(int, getsockname,
    AROS_LHA(int, s, D0),
    AROS_LHA(struct sockaddr *, name, A0),
    AROS_LHA(socklen_t *, namelen, A1),
    struct IEBSDSocketBase *, SocketBase, 17, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_PTR1] = (ULONG)name;
    req.a[IEBSD_REQ_PTR2] = (ULONG)namelen;
    req.a[IEBSD_REQ_LEN1] = (namelen != NULL) ? *namelen : 0;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_GETSOCKNAME, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH3(int, getpeername,
    AROS_LHA(int, s, D0),
    AROS_LHA(struct sockaddr *, name, A0),
    AROS_LHA(socklen_t *, namelen, A1),
    struct IEBSDSocketBase *, SocketBase, 18, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_PTR1] = (ULONG)name;
    req.a[IEBSD_REQ_PTR2] = (ULONG)namelen;
    req.a[IEBSD_REQ_LEN1] = (namelen != NULL) ? *namelen : 0;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_GETPEERNAME, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH3(int, IoctlSocket,
    AROS_LHA(int, s, D0),
    AROS_LHA(unsigned long, request, D1),
    AROS_LHA(char *, argp, A0),
    struct IEBSDSocketBase *, SocketBase, 19, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    req_clear(&req);
    req.a[IEBSD_REQ_S] = s;
    req.a[IEBSD_REQ_AUX1] = request;
    req.a[IEBSD_REQ_PTR1] = (ULONG)argp;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_IOCTL, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH1(int, CloseSocket,
    AROS_LHA(int, s, D0),
    struct IEBSDSocketBase *, SocketBase, 20, BSDSocket)
{
    AROS_LIBFUNC_INIT
    return ieb_socket_finish_free(SocketBase, s);
    AROS_LIBFUNC_EXIT
}

AROS_LH6(int, WaitSelect,
    AROS_LHA(int, nfds, D0),
    AROS_LHA(fd_set *, readfds, A0),
    AROS_LHA(fd_set *, writefds, A1),
    AROS_LHA(fd_set *, exceptfds, A2),
    AROS_LHA(struct timeval *, timeout, A3),
    AROS_LHA(ULONG *, sigmask, D1),
    struct IEBSDSocketBase *, SocketBase, 21, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    if (nfds < 0 || nfds > (int)SocketBase->dTableSize)
        return ieb_socket_fail(SocketBase, EINVAL);
    req_clear(&req);
    req.a[IEBSD_REQ_AUX1] = nfds;
    req.a[IEBSD_REQ_PTR1] = (ULONG)readfds;
    req.a[IEBSD_REQ_PTR2] = (ULONG)writefds;
    req.a[IEBSD_REQ_AUX2] = (ULONG)exceptfds;
    req.a[IEBSD_REQ_TIMEOUT_PTR] = (ULONG)timeout;
    req.a[IEBSD_REQ_SIGMASK_PTR] = (ULONG)sigmask;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_WAITSELECT, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH3(void, SetSocketSignals,
    AROS_LHA(ULONG, intrmask, D0),
    AROS_LHA(ULONG, iomask, D1),
    AROS_LHA(ULONG, urgmask, D2),
    struct IEBSDSocketBase *, SocketBase, 22, BSDSocket)
{
    AROS_LIBFUNC_INIT
    SocketBase->sigIntrMask = intrmask;
    SocketBase->sigIOMask = iomask;
    SocketBase->sigUrgMask = urgmask;
    AROS_LIBFUNC_EXIT
}

AROS_LH0(int, getdtablesize,
    struct IEBSDSocketBase *, SocketBase, 23, BSDSocket)
{
    AROS_LIBFUNC_INIT
    return SocketBase->dTableSize;
    AROS_LIBFUNC_EXIT
}

AROS_LH4(LONG, ObtainSocket,
    AROS_LHA(LONG, id, D0),
    AROS_LHA(LONG, domain, D1),
    AROS_LHA(LONG, type, D2),
    AROS_LHA(LONG, protocol, D3),
    struct IEBSDSocketBase *, SocketBase, 24, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    req_clear(&req);
    req.a[IEBSD_REQ_AUX1] = id;
    req.a[IEBSD_REQ_DOMAIN] = domain;
    req.a[IEBSD_REQ_TYPE] = type;
    req.a[IEBSD_REQ_PROTOCOL] = protocol;
    return ieb_socket_finish_alloc(SocketBase, ieb_socket_call(SocketBase, IE_SOCK_CMD_OBTAIN, &req));
    AROS_LIBFUNC_EXIT
}

AROS_LH2(LONG, ReleaseSocket,
    AROS_LHA(LONG, sd, D0),
    AROS_LHA(LONG, id, D1),
    struct IEBSDSocketBase *, SocketBase, 25, BSDSocket)
{
    AROS_LIBFUNC_INIT
    return ieb_socket_finish_release(SocketBase, sd, id);
    AROS_LIBFUNC_EXIT
}

AROS_LH2(LONG, ReleaseCopyOfSocket,
    AROS_LHA(LONG, sd, D0),
    AROS_LHA(LONG, id, D1),
    struct IEBSDSocketBase *, SocketBase, 26, BSDSocket)
{
    AROS_LIBFUNC_INIT
    return ieb_socket_release_copy(SocketBase, sd, id);
    AROS_LIBFUNC_EXIT
}

AROS_LH0(LONG, Errno,
    struct IEBSDSocketBase *, SocketBase, 27, BSDSocket)
{
    AROS_LIBFUNC_INIT
    return ieb_socket_get_errno(SocketBase);
    AROS_LIBFUNC_EXIT
}

AROS_LH2(void, SetErrnoPtr,
    AROS_LHA(void *, ptr, A0),
    AROS_LHA(int, size, D0),
    struct IEBSDSocketBase *, SocketBase, 28, BSDSocket)
{
    AROS_LIBFUNC_INIT
    if (ptr && (size == 1 || size == 2 || size == 4))
    {
        SocketBase->errnoPtr = (UBYTE *)ptr;
        SocketBase->errnoSize = size;
        ieb_socket_set_errno(SocketBase, SocketBase->defErrno);
    }
    AROS_LIBFUNC_EXIT
}

AROS_LH1(struct hostent *, gethostbyname,
    AROS_LHA(const char *, name, A0),
    struct IEBSDSocketBase *, SocketBase, 35, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    LONG rc;
    if (!name)
    {
        ieb_socket_set_herrno(SocketBase, HOST_NOT_FOUND);
        ieb_socket_set_errno(SocketBase, EINVAL);
        return NULL;
    }
    req_clear(&req);
    req.a[IEBSD_REQ_PTR1] = (ULONG)name;
    req.a[IEBSD_REQ_HOST_NAME_PTR] = (ULONG)SocketBase->hostName;
    req.a[IEBSD_REQ_HOST_NAME_LEN] = sizeof(SocketBase->hostName);
    req.a[IEBSD_REQ_HOST_ADDRS_PTR] = (ULONG)SocketBase->hostAddrs;
    req.a[IEBSD_REQ_HOST_ADDR_COUNT] = IEBSD_ADDR_COUNT;
    rc = ieb_socket_call(SocketBase, IE_SOCK_CMD_GETHOSTBYNAME, &req);
    return (rc >= 0) ? &SocketBase->hostent : NULL;
    AROS_LIBFUNC_EXIT
}

AROS_LH3(struct hostent *, gethostbyaddr,
    AROS_LHA(const void *, addr, A0),
    AROS_LHA(int, len, D0),
    AROS_LHA(int, type, D1),
    struct IEBSDSocketBase *, SocketBase, 36, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    LONG rc;
    if (!addr || len != 4 || type != AF_INET)
    {
        ieb_socket_set_herrno(SocketBase, HOST_NOT_FOUND);
        ieb_socket_set_errno(SocketBase, EINVAL);
        return NULL;
    }
    req_clear(&req);
    req.a[IEBSD_REQ_PTR1] = (ULONG)addr;
    req.a[IEBSD_REQ_LEN1] = len;
    req.a[IEBSD_REQ_AUX1] = type;
    req.a[IEBSD_REQ_HOST_NAME_PTR] = (ULONG)SocketBase->hostName;
    req.a[IEBSD_REQ_HOST_NAME_LEN] = sizeof(SocketBase->hostName);
    req.a[IEBSD_REQ_HOST_ADDRS_PTR] = (ULONG)SocketBase->hostAddrs;
    req.a[IEBSD_REQ_HOST_ADDR_COUNT] = IEBSD_ADDR_COUNT;
    rc = ieb_socket_call(SocketBase, IE_SOCK_CMD_GETHOSTBYADDR, &req);
    return (rc >= 0) ? &SocketBase->hostent : NULL;
    AROS_LIBFUNC_EXIT
}

AROS_LH1(struct netent *, getnetbyname,
    AROS_LHA(const char *, name, A0),
    struct IEBSDSocketBase *, SocketBase, 37, BSDSocket)
{
    AROS_LIBFUNC_INIT
    ieb_socket_unsupported(SocketBase);
    return NULL;
    AROS_LIBFUNC_EXIT
}

AROS_LH2(struct netent *, getnetbyaddr,
    AROS_LHA(long, net, D0),
    AROS_LHA(int, type, D1),
    struct IEBSDSocketBase *, SocketBase, 38, BSDSocket)
{
    AROS_LIBFUNC_INIT
    ieb_socket_unsupported(SocketBase);
    return NULL;
    AROS_LIBFUNC_EXIT
}

AROS_LH2(struct servent *, getservbyname,
    AROS_LHA(char *, name, A0),
    AROS_LHA(char *, proto, A1),
    struct IEBSDSocketBase *, SocketBase, 39, BSDSocket)
{
    AROS_LIBFUNC_INIT
    ieb_socket_unsupported(SocketBase);
    return NULL;
    AROS_LIBFUNC_EXIT
}

AROS_LH2(struct servent *, getservbyport,
    AROS_LHA(int, port, D0),
    AROS_LHA(char *, proto, A0),
    struct IEBSDSocketBase *, SocketBase, 40, BSDSocket)
{
    AROS_LIBFUNC_INIT
    ieb_socket_unsupported(SocketBase);
    return NULL;
    AROS_LIBFUNC_EXIT
}

AROS_LH1(struct protoent *, getprotobyname,
    AROS_LHA(char *, name, A0),
    struct IEBSDSocketBase *, SocketBase, 41, BSDSocket)
{
    AROS_LIBFUNC_INIT
    ieb_socket_unsupported(SocketBase);
    return NULL;
    AROS_LIBFUNC_EXIT
}

AROS_LH1(struct protoent *, getprotobynumber,
    AROS_LHA(int, proto, D0),
    struct IEBSDSocketBase *, SocketBase, 42, BSDSocket)
{
    AROS_LIBFUNC_INIT
    ieb_socket_unsupported(SocketBase);
    return NULL;
    AROS_LIBFUNC_EXIT
}

AROS_LH3(void, vsyslog,
    AROS_LHA(int, level, D0),
    AROS_LHA(const char *, format, A0),
    AROS_LHA(IPTR *, args, A1),
    struct IEBSDSocketBase *, SocketBase, 43, BSDSocket)
{
    AROS_LIBFUNC_INIT
    AROS_LIBFUNC_EXIT
}

AROS_LH2(int, Dup2Socket,
    AROS_LHA(int, fd1, D0),
    AROS_LHA(int, fd2, D1),
    struct IEBSDSocketBase *, SocketBase, 44, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    LONG error;
    LONG newfd;

    if (fd1 != -1 && (error = ieb_socket_validate_fd(SocketBase, fd1)) != 0)
        return ieb_socket_fail(SocketBase, error);
    if (fd1 != -1 && !SocketBase->dTableUsed[fd1])
        return ieb_socket_fail(SocketBase, EBADF);
    if (fd2 != -1 && (error = ieb_socket_validate_fd(SocketBase, fd2)) != 0)
        return ieb_socket_fail(SocketBase, error);
    if (fd1 == fd2)
        return fd2;
    if (fd2 != -1)
    {
        if (SocketBase->dTableUsed[fd2])
        {
            if ((error = ieb_socket_finish_free(SocketBase, fd2)) != 0)
                return error;
        }
        if ((error = ieb_fd_callback(SocketBase, fd2, FDCB_CHECK)) != 0)
            return ieb_socket_fail(SocketBase, error);
    }

    req_clear(&req);
    req.a[IEBSD_REQ_AUX1] = fd1;
    req.a[IEBSD_REQ_AUX2] = fd2;
    newfd = ieb_socket_call(SocketBase, IE_SOCK_CMD_DUP2, &req);
    if (newfd < 0)
        return newfd;
    return ieb_socket_finish_alloc(SocketBase, newfd);
    AROS_LIBFUNC_EXIT
}

AROS_LH3(int, sendmsg,
    AROS_LHA(int, s, D0),
    AROS_LHA(const struct msghdr *, msg, A0),
    AROS_LHA(int, flags, D1),
    struct IEBSDSocketBase *, SocketBase, 45, BSDSocket)
{
    AROS_LIBFUNC_INIT
    return ieb_socket_unsupported(SocketBase);
    AROS_LIBFUNC_EXIT
}

AROS_LH3(int, recvmsg,
    AROS_LHA(int, s, D0),
    AROS_LHA(struct msghdr *, msg, A0),
    AROS_LHA(int, flags, D1),
    struct IEBSDSocketBase *, SocketBase, 46, BSDSocket)
{
    AROS_LIBFUNC_INIT
    return ieb_socket_unsupported(SocketBase);
    AROS_LIBFUNC_EXIT
}

AROS_LH2(int, gethostname,
    AROS_LHA(char *, name, A0),
    AROS_LHA(int, namelen, D0),
    struct IEBSDSocketBase *, SocketBase, 47, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    if (!name || namelen <= 0)
        return ieb_socket_fail(SocketBase, EINVAL);
    req_clear(&req);
    req.a[IEBSD_REQ_PTR1] = (ULONG)name;
    req.a[IEBSD_REQ_LEN1] = namelen;
    return ieb_socket_call(SocketBase, IE_SOCK_CMD_GETHOSTNAME, &req);
    AROS_LIBFUNC_EXIT
}

AROS_LH0(long, gethostid,
    struct IEBSDSocketBase *, SocketBase, 48, BSDSocket)
{
    AROS_LIBFUNC_INIT
    ieb_socket_unsupported(SocketBase);
    return 0;
    AROS_LIBFUNC_EXIT
}

AROS_LH1(ULONG, SocketBaseTagList,
    AROS_LHA(struct TagItem *, tagList, A0),
    struct IEBSDSocketBase *, SocketBase, 49, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct TagItem *tag;
    UWORD tagCode;
    IPTR *tagData;
    ULONG errIndex = 0;
    ULONG index = 0;
    static const char errUnknown[] = "Unknown error";
    static const char errNoError[] = "No error";
    static const char errBadf[] = "Bad file descriptor";
    static const char errInval[] = "Invalid argument";
    static const char errMsgSize[] = "Message too long";
    static const char errOpNotSupp[] = "Operation not supported";
    static const char errNoSys[] = "Function not implemented";
    static const char herrHostNotFound[] = "Host not found";
    static const char herrNoRecovery[] = "Non-recoverable resolver error";

    while ((tag = ieb_next_tag_item(&tagList)) != NULL)
    {
        IPTR value;

        if (!(tag->ti_Tag & TAG_USER))
        {
            index++;
            continue;
        }

        tagCode = (UWORD)(tag->ti_Tag & ~SBTF_REF);
        tagData = (tag->ti_Tag & SBTF_REF) ? (IPTR *)tag->ti_Data : &tag->ti_Data;
        value = tagData ? *tagData : 0;

        if (tagCode & SBTF_SET)
        {
            switch (tagCode)
            {
                case (SBTC_ERRNO << SBTB_CODE) | SBTF_SET:
                    ieb_socket_set_errno(SocketBase, value);
                    break;
                case (SBTC_HERRNO << SBTB_CODE) | SBTF_SET:
                    ieb_socket_set_herrno(SocketBase, value);
                    break;
                case (SBTC_BREAKMASK << SBTB_CODE) | SBTF_SET:
                    SocketBase->sigIntrMask = value;
                    break;
                case (SBTC_DTABLESIZE << SBTB_CODE) | SBTF_SET:
                    if ((ULONG)value > 0 && (ULONG)value <= IEBSD_DTABLE_SIZE)
                        SocketBase->dTableSize = value;
                    break;
                case (SBTC_FDCALLBACK << SBTB_CODE) | SBTF_SET:
                    SocketBase->fdCallback = value;
                    break;
                case (SBTC_LOGSTAT << SBTB_CODE) | SBTF_SET:
                    SocketBase->logStat = (UBYTE)value;
                    break;
                case (SBTC_LOGTAGPTR << SBTB_CODE) | SBTF_SET:
                    SocketBase->logTag = value;
                    break;
                case (SBTC_LOGFACILITY << SBTB_CODE) | SBTF_SET:
                    SocketBase->logFacility = (UWORD)value;
                    break;
                case (SBTC_LOGMASK << SBTB_CODE) | SBTF_SET:
                    SocketBase->logMask = (UBYTE)value;
                    break;
                case (SBTC_ERRNOBYTEPTR << SBTB_CODE) | SBTF_SET:
                    if (value)
                    {
                        SocketBase->errnoPtr = (UBYTE *)value;
                        SocketBase->errnoSize = 1;
                        ieb_socket_set_errno(SocketBase, SocketBase->defErrno);
                    }
                    break;
                case (SBTC_ERRNOWORDPTR << SBTB_CODE) | SBTF_SET:
                    if (value)
                    {
                        SocketBase->errnoPtr = (UBYTE *)value;
                        SocketBase->errnoSize = 2;
                        ieb_socket_set_errno(SocketBase, SocketBase->defErrno);
                    }
                    break;
                case (SBTC_ERRNOLONGPTR << SBTB_CODE) | SBTF_SET:
                    if (value)
                    {
                        SocketBase->errnoPtr = (UBYTE *)value;
                        SocketBase->errnoSize = 4;
                        ieb_socket_set_errno(SocketBase, SocketBase->defErrno);
                    }
                    break;
                case (SBTC_HERRNOLONGPTR << SBTB_CODE) | SBTF_SET:
                    SocketBase->hErrnoPtr = (LONG *)value;
                    if (!SocketBase->hErrnoPtr)
                        SocketBase->hErrnoPtr = &SocketBase->defHErrno;
                    break;
                case (SBTC_SIGIOMASK << SBTB_CODE) | SBTF_SET:
                    SocketBase->sigIOMask = value;
                    break;
                case (SBTC_SIGURGMASK << SBTB_CODE) | SBTF_SET:
                    SocketBase->sigUrgMask = value;
                    break;
                case (SBTC_SIGEVENTMASK << SBTB_CODE) | SBTF_SET:
                    SocketBase->sigEventMask = value;
                    break;
                default:
                    if (!errIndex)
                        errIndex = index + 1;
                    break;
            }
        }
        else
        {
            if (tagData)
            {
                switch (tagCode)
                {
                    case SBTC_ERRNO << SBTB_CODE:
                        *tagData = ieb_socket_get_errno(SocketBase);
                        break;
                    case SBTC_HERRNO << SBTB_CODE:
                        *tagData = SocketBase->hErrnoPtr ? *SocketBase->hErrnoPtr : 0;
                        break;
                    case SBTC_DTABLESIZE << SBTB_CODE:
                        *tagData = SocketBase->dTableSize;
                        break;
                    case SBTC_BREAKMASK << SBTB_CODE:
                        *tagData = SocketBase->sigIntrMask;
                        break;
                    case SBTC_SIGIOMASK << SBTB_CODE:
                        *tagData = SocketBase->sigIOMask;
                        break;
                    case SBTC_SIGURGMASK << SBTB_CODE:
                        *tagData = SocketBase->sigUrgMask;
                        break;
                    case SBTC_SIGEVENTMASK << SBTB_CODE:
                        *tagData = SocketBase->sigEventMask;
                        break;
                    case SBTC_FDCALLBACK << SBTB_CODE:
                        *tagData = SocketBase->fdCallback;
                        break;
                    case SBTC_LOGSTAT << SBTB_CODE:
                        *tagData = SocketBase->logStat;
                        break;
                    case SBTC_LOGTAGPTR << SBTB_CODE:
                        *tagData = SocketBase->logTag;
                        break;
                    case SBTC_LOGFACILITY << SBTB_CODE:
                        *tagData = SocketBase->logFacility;
                        break;
                    case SBTC_LOGMASK << SBTB_CODE:
                        *tagData = SocketBase->logMask;
                        break;
                    case SBTC_HERRNOLONGPTR << SBTB_CODE:
                        *tagData = (IPTR)SocketBase->hErrnoPtr;
                        break;
                    case SBTC_RELEASESTRPTR << SBTB_CODE:
                        *tagData = (IPTR)"IE bsdsocket.library 4.0";
                        break;
                    case SBTC_ERRNOSTRPTR << SBTB_CODE:
                        switch ((LONG)value)
                        {
                            case 0: *tagData = (IPTR)errNoError; break;
                            case EBADF: *tagData = (IPTR)errBadf; break;
                            case EINVAL: *tagData = (IPTR)errInval; break;
                            case EMSGSIZE: *tagData = (IPTR)errMsgSize; break;
                            case EOPNOTSUPP: *tagData = (IPTR)errOpNotSupp; break;
                            case ENOSYS: *tagData = (IPTR)errNoSys; break;
                            default: *tagData = (IPTR)errUnknown; break;
                        }
                        break;
                    case SBTC_HERRNOSTRPTR << SBTB_CODE:
                        switch ((LONG)value)
                        {
                            case 0: *tagData = (IPTR)errNoError; break;
                            case HOST_NOT_FOUND: *tagData = (IPTR)herrHostNotFound; break;
                            case NO_RECOVERY: *tagData = (IPTR)herrNoRecovery; break;
                            default: *tagData = (IPTR)errUnknown; break;
                        }
                        break;
                    case SBTC_HAVE_DNS_API << SBTB_CODE:
                    case SBTC_HAVE_ADDRESS_CONVERSION_API << SBTB_CODE:
                        *tagData = TRUE;
                        break;
                    case SBTC_HAVE_ROUTING_API << SBTB_CODE:
                    case SBTC_HAVE_INTERFACE_API << SBTB_CODE:
                    case SBTC_HAVE_MONITORING_API << SBTB_CODE:
                    case SBTC_HAVE_STATUS_API << SBTB_CODE:
                    case SBTC_HAVE_LOCAL_DATABASE_API << SBTB_CODE:
                    case SBTC_HAVE_KERNEL_MEMORY_API << SBTB_CODE:
                    case SBTC_HAVE_SERVER_API << SBTB_CODE:
                        *tagData = FALSE;
                        break;
                    default:
                        if (!errIndex)
                            errIndex = index + 1;
                        break;
                }
            }
        }
        index++;
    }

    return errIndex;
    AROS_LIBFUNC_EXIT
}

AROS_LH1(LONG, GetSocketEvents,
    AROS_LHA(ULONG *, eventsp, A0),
    struct IEBSDSocketBase *, SocketBase, 50, BSDSocket)
{
    AROS_LIBFUNC_INIT
    struct IEBSDRequest req;
    LONG rc;
    req_clear(&req);
    req.a[IEBSD_REQ_PTR1] = (ULONG)eventsp;
    rc = ieb_socket_call(SocketBase, IE_SOCK_CMD_GETEVENTS, &req);
    if (eventsp && rc >= 0)
        *eventsp = ie_read32(IE_SOCK_EVENTS);
    return rc;
    AROS_LIBFUNC_EXIT
}

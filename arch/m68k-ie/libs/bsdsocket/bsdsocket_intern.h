/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE host-backed bsdsocket.library internals.
*/

#ifndef IE_BSDSOCKET_INTERN_H
#define IE_BSDSOCKET_INTERN_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <ie_hwreg.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <utility/tagitem.h>

#define IEBSD_DTABLE_SIZE       64
#define IEBSD_MAX_PAYLOAD       65536UL
#define IEBSD_MAX_SOCKADDR      128
#define IEBSD_HOST_NAME_SIZE    256
#define IEBSD_CANON_NAME_SIZE   256
#define IEBSD_ALIAS_COUNT       2
#define IEBSD_ADDR_COUNT        8

#ifndef EBADF
#define EBADF           9
#endif
#ifndef EINVAL
#define EINVAL          22
#endif
#ifndef ENOMEM
#define ENOMEM          12
#endif
#ifndef ENOSYS
#define ENOSYS          78
#endif
#ifndef EMSGSIZE
#define EMSGSIZE        40
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP      45
#endif
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT    47
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK     35
#endif

struct IEBSDSocketBase
{
    struct Library lib;
    UBYTE         *errnoPtr;
    UBYTE          errnoSize;
    LONG           defErrno;
    LONG          *hErrnoPtr;
    LONG           defHErrno;
    ULONG          sigIntrMask;
    ULONG          sigIOMask;
    ULONG          sigUrgMask;
    ULONG          sigEventMask;
    char           inet_ntoa[20];
    struct hostent hostent;
    char           hostName[IEBSD_HOST_NAME_SIZE];
    char          *hostAliases[IEBSD_ALIAS_COUNT];
    char          *hostAddrList[IEBSD_ADDR_COUNT + 1];
    ULONG          hostAddrs[IEBSD_ADDR_COUNT];
};

struct IEBSDRequest
{
    ULONG a[24];
};

enum
{
    IEBSD_REQ_S = 0,
    IEBSD_REQ_DOMAIN = 0,
    IEBSD_REQ_TYPE = 1,
    IEBSD_REQ_PROTOCOL = 2,
    IEBSD_REQ_PTR1 = 3,
    IEBSD_REQ_LEN1 = 4,
    IEBSD_REQ_PTR2 = 5,
    IEBSD_REQ_LEN2 = 6,
    IEBSD_REQ_FLAGS = 7,
    IEBSD_REQ_LEVEL = 8,
    IEBSD_REQ_OPTNAME = 9,
    IEBSD_REQ_TIMEOUT_PTR = 10,
    IEBSD_REQ_SIGMASK_PTR = 11,
    IEBSD_REQ_AUX1 = 12,
    IEBSD_REQ_AUX2 = 13,
    IEBSD_REQ_AUX3 = 14,
    IEBSD_REQ_AUX4 = 15,
    IEBSD_REQ_HOST_NAME_PTR = 16,
    IEBSD_REQ_HOST_NAME_LEN = 17,
    IEBSD_REQ_HOST_ADDRS_PTR = 18,
    IEBSD_REQ_HOST_ADDR_COUNT = 19
};

void ieb_socket_set_errno(struct IEBSDSocketBase *base, LONG error);
LONG ieb_socket_get_errno(struct IEBSDSocketBase *base);
void ieb_socket_set_herrno(struct IEBSDSocketBase *base, LONG error);
LONG ieb_socket_call(struct IEBSDSocketBase *base, ULONG cmd, struct IEBSDRequest *req);
LONG ieb_socket_fail(struct IEBSDSocketBase *base, LONG error);
LONG ieb_socket_unsupported(struct IEBSDSocketBase *base);
ULONG ieb_inet_addr_parse(const char *cp);

#endif /* IE_BSDSOCKET_INTERN_H */

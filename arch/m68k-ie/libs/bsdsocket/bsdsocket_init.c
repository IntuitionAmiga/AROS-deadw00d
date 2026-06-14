/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE host-backed bsdsocket.library initialisation.
*/

#include <aros/symbolsets.h>

#include "bsdsocket_intern.h"

static int IEBSDSocket_Init(struct IEBSDSocketBase *base)
{
    int i;

    base->errnoPtr = (UBYTE *)&base->defErrno;
    base->errnoSize = sizeof(base->defErrno);
    base->defErrno = 0;
    base->hErrnoPtr = &base->defHErrno;
    base->defHErrno = 0;
    base->sigIntrMask = 0;
    base->sigIOMask = 0;
    base->sigUrgMask = 0;
    base->sigEventMask = 0;
    base->dTableSize = IEBSD_DTABLE_SIZE;
    base->fdCallback = 0;
    base->logTag = 0;
    base->logStat = 0;
    base->logFacility = 0;
    base->logMask = 0xff;
    for (i = 0; i < IEBSD_DTABLE_SIZE; i++)
        base->dTableUsed[i] = 0;
    base->inet_ntoa[0] = 0;
    base->hostName[0] = 0;
    base->hostAliases[0] = NULL;

    for (i = 0; i < IEBSD_ADDR_COUNT; i++)
        base->hostAddrList[i] = (char *)&base->hostAddrs[i];
    base->hostAddrList[IEBSD_ADDR_COUNT] = NULL;

    base->hostent.h_name = base->hostName;
    base->hostent.h_aliases = base->hostAliases;
    base->hostent.h_addrtype = AF_INET;
    base->hostent.h_length = 4;
    base->hostent.h_addr_list = base->hostAddrList;

    return TRUE;
}

ADD2INITLIB(IEBSDSocket_Init, 0)

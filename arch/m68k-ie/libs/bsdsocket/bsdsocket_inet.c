/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Local address conversion helpers for IE bsdsocket.library.
*/

#include <aros/libcall.h>
#include <netinet/in.h>

#include "bsdsocket_intern.h"

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffffUL
#endif

static int digit_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

ULONG ieb_inet_addr_parse(const char *cp)
{
    ULONG parts[4];
    int nparts = 0;
    ULONG val;
    int base;

    if (!cp || !*cp)
        return INADDR_NONE;

    for (;;)
    {
        int d;
        val = 0;
        base = 10;

        if (*cp == '0')
        {
            cp++;
            if (*cp == 'x' || *cp == 'X')
            {
                base = 16;
                cp++;
            }
            else
            {
                base = 8;
            }
        }

        d = digit_value(*cp);
        if (d < 0 || d >= base)
            return INADDR_NONE;

        while ((d = digit_value(*cp)) >= 0 && d < base)
        {
            val = (val * base) + d;
            cp++;
        }

        if (*cp == '.')
        {
            if (nparts >= 3 || val > 0xff)
                return INADDR_NONE;
            parts[nparts++] = val;
            cp++;
            continue;
        }
        break;
    }

    if (*cp != 0)
        return INADDR_NONE;

    switch (nparts)
    {
        case 0:
            break;
        case 1:
            if (val > 0xffffff)
                return INADDR_NONE;
            val |= parts[0] << 24;
            break;
        case 2:
            if (val > 0xffff)
                return INADDR_NONE;
            val |= (parts[0] << 24) | (parts[1] << 16);
            break;
        case 3:
            if (val > 0xff)
                return INADDR_NONE;
            val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
            break;
    }

    return val;
}

AROS_LH1(char *, Inet_NtoA,
    AROS_LHA(unsigned long, in, D0),
    struct IEBSDSocketBase *, SocketBase, 29, BSDSocket)
{
    AROS_LIBFUNC_INIT
    unsigned char *p = (unsigned char *)&in;
    char *out = SocketBase->inet_ntoa;
    int i;

    for (i = 0; i < 4; i++)
    {
        unsigned int v = p[i];
        if (v >= 100)
            *out++ = '0' + (v / 100);
        if (v >= 10)
            *out++ = '0' + ((v / 10) % 10);
        *out++ = '0' + (v % 10);
        if (i != 3)
            *out++ = '.';
    }
    *out = 0;
    return SocketBase->inet_ntoa;
    AROS_LIBFUNC_EXIT
}

AROS_LH1(unsigned long, inet_addr,
    AROS_LHA(const char *, cp, A0),
    struct IEBSDSocketBase *, SocketBase, 30, BSDSocket)
{
    AROS_LIBFUNC_INIT
    ULONG addr = ieb_inet_addr_parse(cp);
    if (addr == INADDR_NONE)
        ieb_socket_set_errno(SocketBase, EINVAL);
    return addr;
    AROS_LIBFUNC_EXIT
}

AROS_LH1(unsigned long, Inet_LnaOf,
    AROS_LHA(unsigned long, in, D0),
    struct IEBSDSocketBase *, SocketBase, 31, BSDSocket)
{
    AROS_LIBFUNC_INIT
    ULONG i = in;
    if ((i & 0x80000000UL) == 0)
        return i & 0x00ffffffUL;
    if ((i & 0xc0000000UL) == 0x80000000UL)
        return i & 0x0000ffffUL;
    return i & 0x000000ffUL;
    AROS_LIBFUNC_EXIT
}

AROS_LH1(unsigned long, Inet_NetOf,
    AROS_LHA(unsigned long, in, D0),
    struct IEBSDSocketBase *, SocketBase, 32, BSDSocket)
{
    AROS_LIBFUNC_INIT
    ULONG i = in;
    if ((i & 0x80000000UL) == 0)
        return (i >> 24) & 0xff;
    if ((i & 0xc0000000UL) == 0x80000000UL)
        return (i >> 16) & 0xffff;
    return (i >> 8) & 0xffffff;
    AROS_LIBFUNC_EXIT
}

AROS_LH2(unsigned long, Inet_MakeAddr,
    AROS_LHA(int, net, D0),
    AROS_LHA(int, lna, D1),
    struct IEBSDSocketBase *, SocketBase, 33, BSDSocket)
{
    AROS_LIBFUNC_INIT
    ULONG n = (ULONG)net;
    ULONG host = (ULONG)lna;
    if (n < 128)
        return (n << 24) | (host & 0x00ffffffUL);
    if (n < 65536)
        return (n << 16) | (host & 0x0000ffffUL);
    return (n << 8) | (host & 0x000000ffUL);
    AROS_LIBFUNC_EXIT
}

AROS_LH1(unsigned long, inet_network,
    AROS_LHA(const char *, cp, A0),
    struct IEBSDSocketBase *, SocketBase, 34, BSDSocket)
{
    AROS_LIBFUNC_INIT
    ULONG i = ieb_inet_addr_parse(cp);
    if (i == INADDR_NONE)
        return INADDR_NONE;
    if ((i & 0x80000000UL) == 0)
        return (i >> 24) & 0xff;
    if ((i & 0xc0000000UL) == 0x80000000UL)
        return (i >> 16) & 0xffff;
    return (i >> 8) & 0xffffff;
    AROS_LIBFUNC_EXIT
}

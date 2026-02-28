/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE timer platform init.
          IE has no custom chipset to detect — always 60Hz NTSC.
*/

#ifndef TIMER_PLATFORM_H
#define TIMER_PLATFORM_H

#include <exec/types.h>
#include <devices/timer.h>
#include <graphics/gfxbase.h>

struct PlatformTimer
{
    LONG           tb_TimerIRQNum;
    struct timeval tb_VBlankTime;
};

void InitCustom(struct GfxBase *gfx);

#endif /* TIMER_PLATFORM_H */

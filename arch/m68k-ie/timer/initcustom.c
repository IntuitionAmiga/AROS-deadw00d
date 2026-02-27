/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE display configuration for timer.device.
          The Amiga version probes custom chipset for PAL/NTSC.
          IE always runs at 60Hz (NTSC-like timing).
*/

#include <aros/symbolsets.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>

#include <timer_platform.h>

void InitCustom(struct GfxBase *gfx)
{
    /* IE is always 60Hz, NTSC-like timing */
    gfx->DisplayFlags = NTSC;

    SysBase->VBlankFrequency = 60;
}

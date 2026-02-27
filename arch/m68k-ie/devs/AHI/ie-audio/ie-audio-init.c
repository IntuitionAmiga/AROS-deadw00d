/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: IE AHI audio driver — init/cleanup.
*/

#include <exec/types.h>
#include <proto/exec.h>

#include "DriverData.h"

const char LibName[]     = "ie-audio.audio";
const char LibIDString[] = "ie-audio 1.0 (27.02.2026)";

/******************************************************************************
** Custom driver init *********************************************************
******************************************************************************/

BOOL
DriverInit(struct DriverBase *AHIsubBase)
{
    /* IE SoundChip is always present — no hardware detection needed. */
    return TRUE;
}


/******************************************************************************
** Custom driver clean-up *****************************************************
******************************************************************************/

VOID
DriverCleanup(struct DriverBase *AHIsubBase)
{
    /* Nothing to release */
}

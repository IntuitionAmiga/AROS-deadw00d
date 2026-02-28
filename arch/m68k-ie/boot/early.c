/*
 * Copyright (C) 2026, The AROS Development Team. All rights reserved.
 *
 * Desc: Early boot support for IE — alert and trap handlers.
 *       IE has no custom chipset, so early alerts go to TERM_OUT.
 *       Adapted from arch/m68k-amiga/boot/early.c.
 *
 * Licensed under the AROS PUBLIC LICENSE (APL) Version 1.1
 */

#define DEBUG 0

#include <proto/exec.h>
#include <exec/memory.h>

#include "ie_hwreg.h"

#include "early.h"
#include "debug.h"

void Early_ScreenCode(ULONG code)
{
    /* IE has no screen hardware for boot codes — ignore silently */
}

void Early_Alert(ULONG alert)
{
    ie_puts("*** EARLY ALERT: 0x");
    /* Simple hex output */
    {
        int i;
        for (i = 28; i >= 0; i -= 4) {
            int nibble = (alert >> i) & 0xF;
            ie_putchar(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
        }
    }
    ie_puts(" ***\n");

    DEBUGPUTHEX(("Early_Alert", alert));

    if (alert & AT_DeadEnd) {
        ie_puts("DEADEND — system halted.\n");
        for (;;)
            ;
    }
}

/* Fatal trap for early problems */
extern void Exec_MagicResetCode(void);
void __attribute__((interrupt)) Early_TrapHandler(void)
{
    ie_puts("*** EARLY TRAP ***\n");

    if (SysBase != NULL)
        Debug(0);
    else
        Early_Alert(AT_DeadEnd | 1);

    Exec_MagicResetCode();
}


APTR Early_AllocAbs(struct MemHeader *mh, APTR location, IPTR byteSize)
{
    APTR ret = NULL;
    APTR endlocation = location + byteSize;
    struct MemChunk *p1, *p2, *p3, *p4;

    if (mh->mh_Lower > location || mh->mh_Upper < endlocation)
        return (APTR)1;

    /* Align size to the requirements */
    byteSize += (IPTR)location&(MEMCHUNK_TOTAL - 1);
    byteSize  = (byteSize + MEMCHUNK_TOTAL-1) & ~(MEMCHUNK_TOTAL-1);

    /* Align the location as well */
    location=(APTR)((IPTR)location & ~(MEMCHUNK_TOTAL-1));

    /* Start and end(+1) of the block */
    p3=(struct MemChunk *)location;
    p4=(struct MemChunk *)((UBYTE *)p3+byteSize);

    p1 = (struct MemChunk *)&mh->mh_First;
    p2 = p1->mc_Next;

    /* Follow the list to find a chunk with our memory. */
    while (p2 != NULL)
    {
        /* Found a chunk that fits? */
        if((UBYTE *)p2+p2->mc_Bytes>=(UBYTE *)p4&&p2<=p3)
        {
            /* Check if there's memory left at the end. */
            if((UBYTE *)p2+p2->mc_Bytes!=(UBYTE *)p4)
            {
                /* Yes. Add it to the list */
                p4->mc_Next  = p2->mc_Next;
                p4->mc_Bytes = (UBYTE *)p2+p2->mc_Bytes-(UBYTE *)p4;
                p2->mc_Next  = p4;
            }

            /* Check if there's memory left at the start. */
            if(p2!=p3)
                /* Yes. Adjust the size */
                p2->mc_Bytes=(UBYTE *)p3-(UBYTE *)p2;
            else
                /* No. Skip the old chunk */
                p1->mc_Next=p2->mc_Next;

            /* Adjust free memory count */
            mh->mh_Free-=byteSize;

            /* Return the memory */
            ret = p3;
            break;
        }
        /* goto next chunk */

        p1=p2;
        p2=p2->mc_Next;
    }

    return ret;
}

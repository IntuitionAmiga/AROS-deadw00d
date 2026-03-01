/*
 * Copyright (C) 2026, The AROS Development Team. All rights reserved.
 *
 * Licensed under the AROS PUBLIC LICENSE (APL) Version 1.1
 *
 * Global variables for kernel.resource on m68k-ie.
 * Mirrors rom/kernel/kernel_globals.c.
 */
#include <aros/symbolsets.h>

/*
 * Some globals we can't live without.
 * IMPORTANT: BootMsg should survive warm restarts, this is why it's placed in .data.
 */
struct KernelBase *KernelBase;
__attribute__((section(".data"))) struct TagItem *BootMsg;

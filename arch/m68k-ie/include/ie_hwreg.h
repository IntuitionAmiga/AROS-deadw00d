/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Intuition Engine MMIO register definitions for AROS m68k-ie port.

    This header provides C-accessible definitions for the IE's memory-mapped
    I/O registers. The canonical reference is registers.go in the IE source.
    Only the registers needed by the AROS port are included here; add more
    as needed during implementation.
*/

#ifndef IE_HWREG_H
#define IE_HWREG_H

/* ========================================================================
 * Video Chip (0xF0000 - 0xF0054)
 * ======================================================================== */

#define IE_VIDEO_CTRL       0xF0000     /* Control: bit 0 = enable */
#define IE_VIDEO_MODE       0xF0004     /* Display mode (32-bit register) */
#define IE_VIDEO_STATUS     0xF0008     /* Status: bit 1 = vblank */

/* Video mode values (written to IE_VIDEO_MODE) */
#define IE_VIDEO_MODE_640x480   0x00
#define IE_VIDEO_MODE_800x600   0x01
#define IE_VIDEO_MODE_1024x768  0x02
#define IE_VIDEO_MODE_1280x960  0x03

/* Blitter (0xF001C - 0xF0074) */
#define IE_BLT_CTRL         0xF001C     /* Control: bit 0 = start */
#define IE_BLT_OP           0xF0020     /* Operation: 0=copy,1=fill,2=line,3=masked,4=alpha,5=mode7 */
#define IE_BLT_SRC          0xF0024     /* Source address */
#define IE_BLT_DST          0xF0028     /* Destination address */
#define IE_BLT_WIDTH        0xF002C     /* Width in pixels */
#define IE_BLT_HEIGHT       0xF0030     /* Height in pixels */
#define IE_BLT_SRC_STRIDE   0xF0034     /* Source stride in bytes */
#define IE_BLT_DST_STRIDE   0xF0038     /* Destination stride in bytes */
#define IE_BLT_COLOR        0xF003C     /* Fill/line color (RGBA32) */
#define IE_BLT_MASK         0xF0040     /* Mask address for masked copy */
#define IE_BLT_STATUS       0xF0044     /* Status: bit 0 = busy */

/* Copper (0xF000C - 0xF0018) */
#define IE_COPPER_CTRL      0xF000C     /* Control: bit 0 = enable */
#define IE_COPPER_PTR       0xF0010     /* Display list base address */
#define IE_COPPER_STATUS    0xF0018     /* Status */

/* Copper instructions:
 * WAIT   (opcode 0): 4 bytes - wait for raster position
 * MOVE   (opcode 1): 8 bytes - write value to register
 * SETBASE(opcode 2): 8 bytes - set framebuffer base address
 * END    (opcode 3): 4 bytes - end display list
 */
#define IE_COPPER_OP_WAIT       0
#define IE_COPPER_OP_MOVE       1
#define IE_COPPER_OP_SETBASE    2
#define IE_COPPER_OP_END        3

/* ========================================================================
 * Terminal / Input (0xF0700 - 0xF07FF)
 * ======================================================================== */

#define IE_TERM_OUT         0xF0700     /* Write character to host terminal */
#define IE_TERM_STATUS      0xF0704     /* Bit 0: input available, Bit 1: output ready */
#define IE_TERM_IN          0xF0708     /* Read next input character (dequeues) */

/* Mouse */
#define IE_MOUSE_X          0xF0730     /* Absolute X position */
#define IE_MOUSE_Y          0xF0734     /* Absolute Y position */
#define IE_MOUSE_BUTTONS    0xF0738     /* Bit 0=left, 1=right, 2=middle */
#define IE_MOUSE_STATUS     0xF073C     /* Bit 0=changed (clear-on-read) */

/* Keyboard */
#define IE_SCAN_CODE        0xF0740     /* Raw scancode (dequeue on read) */
#define IE_SCAN_STATUS      0xF0744     /* Bit 0=scancode available */
#define IE_SCAN_MODIFIERS   0xF0748     /* Bit 0=shift, 1=ctrl, 2=alt, 3=capslock */

/* ========================================================================
 * Audio - Flex Channels (0xF0A80 - 0xF0B3F)
 * ======================================================================== */

#define IE_FLEX_CH0_BASE    0xF0A80     /* Channel 0 base */
#define IE_FLEX_CH1_BASE    0xF0AC0     /* Channel 1 base */
#define IE_FLEX_CH2_BASE    0xF0B00     /* Channel 2 base */
#define IE_FLEX_CH3_BASE    0xF0B40     /* Channel 3 base */
#define IE_FLEX_CH_STRIDE   0x40        /* 64 bytes per channel */

/* Per-channel register offsets */
#define IE_FLEX_OFF_VOL     0x04        /* Volume (0-255) */
#define IE_FLEX_OFF_CTRL    0x08        /* Control: bit 0=enable, bit 1=gate */
#define IE_FLEX_OFF_DAC     0x3C        /* DAC mode: signed int8 sample */

/* ========================================================================
 * Audio DMA (0xF2260 - 0xF22AF) — Paula-compatible DMA emulation
 * ======================================================================== */

#define IE_AUD_BASE         0xF2260

/* Per-channel registers (4 channels × 16 bytes) */
#define IE_AUD_CH0_PTR      0xF2260     /* Sample pointer in guest RAM */
#define IE_AUD_CH0_LEN      0xF2264     /* Length in words (×2 = bytes) */
#define IE_AUD_CH0_PER      0xF2268     /* Period (PAULA_CLOCK / freq) */
#define IE_AUD_CH0_VOL      0xF226C     /* Volume (0-64) */

#define IE_AUD_CH1_PTR      0xF2270
#define IE_AUD_CH1_LEN      0xF2274
#define IE_AUD_CH1_PER      0xF2278
#define IE_AUD_CH1_VOL      0xF227C

#define IE_AUD_CH2_PTR      0xF2280
#define IE_AUD_CH2_LEN      0xF2284
#define IE_AUD_CH2_PER      0xF2288
#define IE_AUD_CH2_VOL      0xF228C

#define IE_AUD_CH3_PTR      0xF2290
#define IE_AUD_CH3_LEN      0xF2294
#define IE_AUD_CH3_PER      0xF2298
#define IE_AUD_CH3_VOL      0xF229C

/* Global registers */
#define IE_AUD_DMACON       0xF22A0     /* DMA control: bit 15=set/clear, bits 0-3=channels */
#define IE_AUD_STATUS       0xF22A4     /* Completion status: bits 0-3, write-to-clear */
#define IE_AUD_INTENA       0xF22A8     /* Interrupt enable: bit 15=set/clear, bits 0-3 */

/* Per-channel register stride */
#define IE_AUD_CH_STRIDE    16

/* Per-channel register offsets */
#define IE_AUD_OFF_PTR      0x00
#define IE_AUD_OFF_LEN      0x04
#define IE_AUD_OFF_PER      0x08
#define IE_AUD_OFF_VOL      0x0C

/* Compute channel N base address */
#define IE_AUD_CH_BASE(n)   (IE_AUD_BASE + (n) * IE_AUD_CH_STRIDE)

/* ========================================================================
 * AROS DOS Handler MMIO (0xF2220 - 0xF225F)
 * ======================================================================== */

#define IE_DOS_BASE         0xF2220

#define IE_DOS_CMD          0xF2220     /* Command code (write triggers action) */
#define IE_DOS_ARG1         0xF2224     /* Argument 1 (pointer or value) */
#define IE_DOS_ARG2         0xF2228     /* Argument 2 */
#define IE_DOS_ARG3         0xF222C     /* Argument 3 */
#define IE_DOS_ARG4         0xF2230     /* Argument 4 */
#define IE_DOS_RESULT1      0xF2234     /* Primary result (dp_Res1) */
#define IE_DOS_RESULT2      0xF2238     /* Secondary result / IoErr (dp_Res2) */
#define IE_DOS_STATUS       0xF223C     /* Status: 0=ready */

/* DOS command codes (written to IE_DOS_CMD) */
#define IE_DOS_CMD_LOCK         1   /* ARG1=name, ARG2=parent, ARG3=mode → R1=key */
#define IE_DOS_CMD_UNLOCK       2   /* ARG1=key */
#define IE_DOS_CMD_EXAMINE      3   /* ARG1=key, ARG2=fib_ptr → fills FIB */
#define IE_DOS_CMD_EXNEXT       4   /* ARG1=key, ARG2=fib_ptr → next entry */
#define IE_DOS_CMD_FINDINPUT    5   /* ARG1=name, ARG2=parent → R1=handle */
#define IE_DOS_CMD_FINDOUTPUT   6   /* ARG1=name, ARG2=parent → R1=handle */
#define IE_DOS_CMD_FINDUPDATE   7   /* ARG1=name, ARG2=parent → R1=handle */
#define IE_DOS_CMD_READ         8   /* ARG1=handle, ARG2=buf, ARG3=len → R1=read */
#define IE_DOS_CMD_WRITE        9   /* ARG1=handle, ARG2=buf, ARG3=len → R1=written */
#define IE_DOS_CMD_SEEK         10  /* ARG1=handle, ARG2=offset, ARG3=mode → R1=old */
#define IE_DOS_CMD_CLOSE        11  /* ARG1=handle */
#define IE_DOS_CMD_PARENT       12  /* ARG1=key → R1=parent_key */
#define IE_DOS_CMD_DELETE       13  /* ARG1=parent, ARG2=name */
#define IE_DOS_CMD_CREATEDIR    14  /* ARG1=parent, ARG2=name → R1=key */
#define IE_DOS_CMD_RENAME       15  /* ARG1-4: src/dst parent+name */
#define IE_DOS_CMD_DISKINFO     16  /* ARG1=info_ptr → fills InfoData */
#define IE_DOS_CMD_DUPLOCK      17  /* ARG1=key → R1=new_key */
#define IE_DOS_CMD_SAMELOCK     18  /* ARG1=key1, ARG2=key2 → R1=result */
#define IE_DOS_CMD_IS_FS        19  /* → R1=DOSTRUE */
#define IE_DOS_CMD_SET_FILESIZE 20  /* ARG1=handle, ARG2=size, ARG3=mode */
#define IE_DOS_CMD_SET_PROTECT  21  /* ARG1=parent, ARG2=name, ARG3=bits */
#define IE_DOS_CMD_EXAMINE_FH   22  /* ARG1=handle, ARG2=fib_ptr → fills FIB */

/* ========================================================================
 * Memory Map
 * ======================================================================== */

#define IE_MAIN_RAM_BASE    0x000000    /* Start of main RAM */
#define IE_MAIN_RAM_END     0x09EFFF    /* End of main RAM (636KB) */
#define IE_VRAM_BASE        0x100000    /* Video RAM base (5MB) */
#define IE_VRAM_SIZE        0x500000    /* Video RAM size */
#define IE_ROM_BASE         0x600000    /* AROS ROM load address */
#define IE_IO_BASE          0xF0000     /* I/O region start */
#define IE_IO_END           0xFFFFF     /* I/O region end */

/* Memory regions for ExecBase initialization.
 * Skip I/O holes at 0xA0000-0xBFFFF (VGA) and 0xF0000-0xFFFFF (MMIO).
 * Bank 1: 0x001000 - 0x09FFFF  (636KB, first 4KB reserved for vectors)
 * Bank 2: 0x100000 - 0x5FFFFF  (5MB, shared with VRAM when not in use)
 */
#define IE_MEM_BANK1_START  0x001000
#define IE_MEM_BANK1_SIZE   0x09F000    /* 636KB */
#define IE_MEM_BANK2_START  0x100000
#define IE_MEM_BANK2_SIZE   0x500000    /* 5MB */

/* ========================================================================
 * Inline register access helpers
 * ======================================================================== */

static inline void ie_write32(unsigned long reg, unsigned long val)
{
    volatile unsigned long *r = (volatile unsigned long *)reg;
    *r = val;
}

static inline unsigned long ie_read32(unsigned long reg)
{
    volatile unsigned long *r = (volatile unsigned long *)reg;
    return *r;
}

static inline void ie_write16(unsigned long reg, unsigned short val)
{
    volatile unsigned short *r = (volatile unsigned short *)reg;
    *r = val;
}

static inline unsigned short ie_read16(unsigned long reg)
{
    volatile unsigned short *r = (volatile unsigned short *)reg;
    return *r;
}

static inline void ie_write8(unsigned long reg, unsigned char val)
{
    volatile unsigned char *r = (volatile unsigned char *)reg;
    *r = val;
}

static inline unsigned char ie_read8(unsigned long reg)
{
    volatile unsigned char *r = (volatile unsigned char *)reg;
    return *r;
}

/* Terminal debug output */
static inline void ie_putchar(char c)
{
    ie_write32(IE_TERM_OUT, (unsigned long)c);
}

static inline void ie_puts(const char *s)
{
    while (*s)
        ie_putchar(*s++);
}

#endif /* IE_HWREG_H */

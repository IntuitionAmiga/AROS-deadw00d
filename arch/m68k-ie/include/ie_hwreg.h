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

/* CLUT8 Palette Mode (0xF0078 - 0xF0487) */
#define IE_VIDEO_PAL_INDEX      0xF0078     /* Palette write index (0-255) */
#define IE_VIDEO_PAL_DATA       0xF007C     /* Palette data (0x00RRGGBB, auto-increments) */
#define IE_VIDEO_COLOR_MODE     0xF0080     /* 0=RGBA32 (direct), 1=CLUT8 (indexed) */
#define IE_VIDEO_FB_BASE        0xF0084     /* Framebuffer base address (default 0x100000) */
#define IE_VIDEO_PAL_TABLE      0xF0088     /* 256x4-byte direct palette access */
#define IE_VIDEO_PAL_ENTRY(x)   (IE_VIDEO_PAL_TABLE + (x) * 4)

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
#define IE_BLT_STATUS       0xF0044     /* Status: bit 0 = error */

/* Mode7 affine texture mapping registers (16.16 fixed-point) */
#define IE_BLT_MODE7_U0        0xF0058     /* Starting U coordinate */
#define IE_BLT_MODE7_V0        0xF005C     /* Starting V coordinate */
#define IE_BLT_MODE7_DU_COL    0xF0060     /* U delta per column */
#define IE_BLT_MODE7_DV_COL    0xF0064     /* V delta per column */
#define IE_BLT_MODE7_DU_ROW    0xF0068     /* U delta per row */
#define IE_BLT_MODE7_DV_ROW    0xF006C     /* V delta per row */
#define IE_BLT_MODE7_TEX_W     0xF0070     /* Texture width mask (255 = 256-wide) */
#define IE_BLT_MODE7_TEX_H     0xF0074     /* Texture height mask */

/* Extended blitter registers (BPP mode, draw modes, color expansion) */
#define IE_BLT_FLAGS            0xF0488     /* BPP, draw mode, JAM1/invert flags */
#define IE_BLT_FG               0xF048C     /* Foreground color for color expansion */
#define IE_BLT_BG               0xF0490     /* Background color for color expansion */
#define IE_BLT_MASK_MOD         0xF0494     /* Template/mask row modulo (bytes) */
#define IE_BLT_MASK_SRCX        0xF0498     /* Starting X bit offset in template */

/* Blitter operation codes */
#define IE_BLT_OP_COPY         0           /* Rectangular copy */
#define IE_BLT_OP_FILL         1           /* Rectangular fill */
#define IE_BLT_OP_LINE         2           /* Line draw */
#define IE_BLT_OP_MASKED       3           /* Masked copy */
#define IE_BLT_OP_ALPHA        4           /* Alpha blended copy */
#define IE_BLT_OP_MODE7        5           /* Affine texture mapping */
#define IE_BLT_OP_COLOR_EXPAND 6           /* 1-bit template to colored pixels */

/* Blitter control bits (BLT_CTRL register) */
#define IE_BLT_CTRL_START      (1 << 0)    /* Write 1 to start blit */
#define IE_BLT_CTRL_BUSY       (1 << 1)    /* Read: 1 = blit in progress */
#define IE_BLT_CTRL_IRQ        (1 << 2)    /* IRQ enable */

/* BLT_FLAGS bit definitions */
#define IE_BLT_FLAGS_BPP_RGBA32  0x00       /* 4 bytes per pixel (default) */
#define IE_BLT_FLAGS_BPP_CLUT8   0x01       /* 1 byte per pixel */
#define IE_BLT_FLAGS_BPP_MASK    0x03       /* Bits 0-1: BPP mode */
#define IE_BLT_FLAGS_DRAWMODE_SHIFT 4        /* Bits 4-7: draw mode (16 raster ops) */
#define IE_BLT_FLAGS_DRAWMODE_MASK  0xF0
#define IE_BLT_FLAGS_JAM1        (1 << 8)   /* Template: skip BG pixels */
#define IE_BLT_FLAGS_INVERT_TMPL (1 << 9)   /* Invert template bits */
#define IE_BLT_FLAGS_INVERT_MODE (1 << 10)  /* XOR dst for set template bits */

/* Helper to build BLT_FLAGS value */
#define IE_BLT_MAKE_FLAGS(bpp, drawmode) \
    (((bpp) & 0x03) | (((drawmode) & 0x0F) << 4))

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

/* Real-Time Clock */
#define IE_RTC_EPOCH        0xF0750     /* Read: host UTC seconds since Unix epoch */

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
 * Coprocessor MMIO (0xF2340 - 0xF238F)
 * ======================================================================== */

#define IE_COPROC_BASE              0xF2340
#define IE_COPROC_CMD               0xF2340     /* Command register (W triggers) */
#define IE_COPROC_CPU_TYPE          0xF2344     /* Target CPU type */
#define IE_COPROC_CMD_STATUS        0xF2348     /* Command status: 0=ok, 1=error */
#define IE_COPROC_CMD_ERROR         0xF234C     /* Error code */
#define IE_COPROC_TICKET            0xF2350     /* Ticket ID (R/W) */
#define IE_COPROC_TICKET_STATUS     0xF2354     /* Per-ticket status */
#define IE_COPROC_OP                0xF2358     /* Operation code */
#define IE_COPROC_REQ_PTR           0xF235C     /* Request data pointer */
#define IE_COPROC_REQ_LEN           0xF2360     /* Request data length */
#define IE_COPROC_RESP_PTR          0xF2364     /* Response buffer pointer */
#define IE_COPROC_RESP_CAP          0xF2368     /* Response buffer capacity */
#define IE_COPROC_TIMEOUT           0xF236C     /* Timeout ms */
#define IE_COPROC_NAME_PTR          0xF2370     /* Service filename pointer */
#define IE_COPROC_WORKER_STATE      0xF2374     /* Bitmask of running workers (R) */
#define IE_COPROC_STATS_OPS         0xF2378     /* Total ops dispatched (R) */
#define IE_COPROC_STATS_BYTES       0xF237C     /* Total bytes processed (R) */
#define IE_COPROC_IRQ_CTRL          0xF2380     /* IRQ enable (bit 0) */
#define IE_COPROC_DISPATCH_OVERHEAD 0xF2384     /* Calibrated overhead ns (R) */
#define IE_COPROC_COMPLETED_TICKET  0xF2388     /* Last completed ticket (R) */

/* Coprocessor commands */
#define IE_COPROC_CMD_START     1
#define IE_COPROC_CMD_STOP      2
#define IE_COPROC_CMD_ENQUEUE   3
#define IE_COPROC_CMD_POLL      4
#define IE_COPROC_CMD_WAIT_CMD  5

/* Ticket status codes */
#define IE_COPROC_ST_PENDING        0
#define IE_COPROC_ST_RUNNING        1
#define IE_COPROC_ST_OK             2
#define IE_COPROC_ST_ERROR          3
#define IE_COPROC_ST_TIMEOUT        4
#define IE_COPROC_ST_WORKER_DOWN    5

/* CPU type constants */
#define IE_EXEC_TYPE_IE32       1
#define IE_EXEC_TYPE_IE64       2
#define IE_EXEC_TYPE_6502       3
#define IE_EXEC_TYPE_M68K       4
#define IE_EXEC_TYPE_Z80        5
#define IE_EXEC_TYPE_X86        6

/* ========================================================================
 * Clipboard Bridge (0xF2390 - 0xF23AF)
 * ======================================================================== */

#define IE_CLIP_BASE            0xF2390
#define IE_CLIP_DATA_PTR        0xF2390     /* Guest RAM pointer for data */
#define IE_CLIP_DATA_LEN        0xF2394     /* Data length in bytes */
#define IE_CLIP_CTRL            0xF2398     /* Command: 1=read from host, 2=write to host */
#define IE_CLIP_STATUS          0xF239C     /* Status: 0=ready, 1=busy, 2=empty, 3=error */
#define IE_CLIP_RESULT_LEN      0xF23A0     /* Bytes actually read/written */
#define IE_CLIP_FORMAT          0xF23A4     /* Format: 0=text, 1=IFF */

#define IE_CLIP_CMD_READ        1
#define IE_CLIP_CMD_WRITE       2

#define IE_CLIP_STATUS_READY    0
#define IE_CLIP_STATUS_BUSY     1
#define IE_CLIP_STATUS_EMPTY    2
#define IE_CLIP_STATUS_ERROR    3

/* ========================================================================
 * Memory Map
 * ======================================================================== */

#define IE_MAIN_RAM_BASE    0x000000    /* Start of main RAM */
#define IE_MAIN_RAM_END     0x09EFFF    /* End of main RAM (636KB) */
#define IE_VRAM_BASE        0x1E00000   /* Video RAM base (top of 32MB bus) */
#define IE_VRAM_SIZE        0x200000    /* Video RAM size (2MB) */
#define IE_ROM_BASE         0x600000    /* AROS ROM load address */
#define IE_IO_BASE          0xF0000     /* I/O region start */
#define IE_IO_END           0xFFFFF     /* I/O region end */

/* Memory regions for ExecBase initialization (32MB bus).
 * Chip A: 0x002800 - 0x09FFFF (630KB, below VGA hole)
 * Chip B: 0x100000 - 0x5FFFFF (5MB)
 * ROM:    0x600000 - 0x7FFFFF (2MB, AROS ROM image)
 * Fast:   0x800000 - 0x1DFFFFF (22MB)
 * VRAM:   0x1E00000 - 0x1FFFFFF (2MB, IEGfx HIDD bump allocator)
 * Total:  ~5.6MB chip + 22MB fast = ~27.6MB
 */
#define IE_MEM_BANK1_START  0x002800
#define IE_MEM_BANK1_SIZE   0x09D800    /* 630KB */
#define IE_MEM_BANK2_START  0x100000
#define IE_MEM_BANK2_SIZE   0x500000    /* 5MB */
#define IE_MEM_BANK3_START  0x800000
#define IE_MEM_BANK3_SIZE   0x1600000   /* 22MB */

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

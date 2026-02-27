/*
 * AROS m68k-ie ROM linker script.
 * Adapted from arch/m68k-amiga/boot/aros-rom.ld
 *
 * ROM is loaded at IE_ROM_BASE (0x600000) by the IE loader.
 * The linker places everything at 0x000000 (position-independent
 * offsets); the loader relocates by writing to bus memory at the
 * ROM base address.
 */

MEMORY {
  valid   (rwx) : org = 0x00000000 , l = 2M
  invalid (rwx) : org = 0xdead0000 , l = 0x0
}

SECTIONS
{
  .ext                        : { _ext_start = .;
INCLUDE ext_objs.ld
                                  _ext_end = .;
                                } >valid
  .rom                        : { _rom_start = .;
INCLUDE rom_objs.ld
                                  /* Static libs */
                                  *(.text .text.* .rodata .rodata.*)
                                  *(.eh_frame)
                                  _rom_end = .;
                                } >valid

  /* Supervisor stack: 0x800 - 0x1000 (2KB).
   * Below MEMF_CHIP RAM which starts at 0x1000.
   * Only used during early boot until a proper stack is allocated.
   */
  .ss                         : {
                                  _ss = .;
                                  . = . + 0x800;
                                  _ss_end = .;
                                } >valid

  /* .bss and .data must not exist in a ROM image */
  .bss                        : { *(.bss) } >invalid
  .data                       : { *(.data) } >invalid
}

/*
 * AROS m68k-ie ROM linker script.
 * Adapted from arch/m68k-amiga/boot/aros-rom.ld
 *
 * ROM is loaded at IE_ROM_BASE (0x600000) by the IE loader.
 * org = 0x600000 so the final link produces correct absolute addresses.
 *
 * IMPORTANT: .ext must precede .rom so its INCLUDE patterns claim
 * sections from ext objects BEFORE .rom's *(.text ...) wildcard.
 * The binary order is controlled by --section-start in the stage 2 link,
 * not by the order here.  Memory region is deliberately oversized for
 * the relocatable stage 1 link; stage 2 assigns final addresses.
 */

MEMORY {
  valid   (rwx) : org = 0x600000 , l = 16M
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
                                  /* AROS module initialisation sets */
                                  *(.aros.set.*)
                                  __eh_frame_start = .;
                                  *(.eh_frame)
                                  __eh_frame_end = .;
                                  _rom_end = .;
                                } >valid

  /* Supervisor stack: 8KB.
   * Used during early boot until a proper stack is allocated.
   * Must be large enough to survive exception cascades without
   * overflowing into the vector table (0x000-0x3FF).
   */
  .ss                         : {
                                  _ss = .;
                                  . = . + 0x2000;
                                  _ss_end = .;
                                } >valid

  /* For relocatable link (-r), .bss and .data are gathered here.
   * The final objcopy --output-target binary strips them.
   */
  .bss                        : { *(.bss .bss.*) } >valid
  .data                       : { *(.data .data.*) } >valid
}

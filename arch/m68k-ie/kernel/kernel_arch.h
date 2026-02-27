/*
 * Machine-specific definitions for Intuition Engine hardware.
 */

/* Number of IRQs used in the machine. Needed by kernel_base.h */
#define IRQ_COUNT 14
#define HW_IRQ_COUNT IRQ_COUNT

/* We don't need to emulate VBlank, it's a real hardware interrupt here */
#define NO_VBLANK_EMU

/* IE-specific interrupt enable/disable are no-ops at the chipset level;
 * the kernel uses M68K SR (Status Register) directly.
 */
#define ictl_enable_irq(irq, base)
#define ictl_disable_irq(irq, base)

/* IE has no custom chipset — _CUSTOM stays NULL (kernel_intr.h default) */

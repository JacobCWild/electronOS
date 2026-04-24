/* ===========================================================================
 * Neutron Kernel — AArch64 Boot Code
 * ===========================================================================
 * Entry point for the Neutron kernel. Transitions from EL2 to EL1,
 * initializes the stack, clears BSS, and jumps to kernel_main().
 *
 * On QEMU virt, firmware drops us at EL2 (or EL1 with -machine virt).
 * On RPi5, the stub puts us at EL2.
 * =========================================================================== */

.section .text.boot, "ax"
.global _start
.type _start, @function

_start:
    /* ---- Disable all cores except #0 -------------------------------- */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF          /* CPU ID (Aff0) */
    cbnz    x0, .Lpark             /* Park non-primary cores */

    /* ---- Check current exception level ------------------------------ */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    cmp     x0, #2
    b.eq    .Lfrom_el2
    cmp     x0, #1
    b.eq    .Lat_el1
    /* EL3 — shouldn't happen on QEMU virt, but handle gracefully */
    b       .Lpark

.Lfrom_el2:
    /* ---- Configure EL1 from EL2 ------------------------------------- */

    /* Enable AArch64 for EL1 */
    mov     x0, #(1 << 31)         /* HCR_EL2.RW = 1 (AArch64 at EL1) */
    orr     x0, x0, #(1 << 1)      /* HCR_EL2.SWIO */
    msr     hcr_el2, x0

    /* Set up SPSR for EL1h with all exceptions masked */
    mov     x0, #0x3C5             /* EL1h | DAIF masked */
    msr     spsr_el2, x0

    /* Return address = .Lat_el1 */
    adr     x0, .Lat_el1
    msr     elr_el2, x0

    /* Enable GICv3 system register access at EL1 */
    mrs     x0, S3_4_C12_C9_5     /* ICC_SRE_EL2 */
    orr     x0, x0, #(1 << 0)     /* SRE bit */
    orr     x0, x0, #(1 << 3)     /* Enable lower EL access */
    msr     S3_4_C12_C9_5, x0
    isb

    /* Allow EL1 to access physical timer */
    mov     x0, #3                 /* EL1PCTEN | EL1PCEN */
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr      /* No virtual timer offset */

    /* Drop to EL1 */
    eret

.Lat_el1:
    /* ---- We are now at EL1 ------------------------------------------ */

    /* Enable FP/SIMD access (CPACR_EL1.FPEN = 0b11) */
    mov     x0, #(3 << 20)
    msr     cpacr_el1, x0
    isb

    /* Set up the kernel stack */
    ldr     x0, =__stack_top
    mov     sp, x0

    /* Clear BSS */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
.Lbss_loop:
    cmp     x0, x1
    b.ge    .Lbss_done
    str     xzr, [x0], #8
    b       .Lbss_loop
.Lbss_done:

    /* Disable all exceptions (DAIF) during early init */
    msr     daifset, #0xf

    /* Jump to C code */
    bl      kernel_main

    /* Should never return — but if it does, halt */
    b       .Lpark

.Lpark:
    wfi
    b       .Lpark

.size _start, . - _start

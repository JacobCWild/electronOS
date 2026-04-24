/* ===========================================================================
 * Neutron Kernel — Context Switch Assembly
 * ===========================================================================
 * void arch_context_switch(u64 *old_sp, u64 new_sp)
 *
 * Saves callee-saved registers (x19-x30, sp) to old_sp,
 * restores them from new_sp, and returns to the new context.
 * =========================================================================== */

.global arch_context_switch
.type arch_context_switch, @function
arch_context_switch:
    /* Save callee-saved registers to current stack */
    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!
    stp     x27, x28, [sp, #-16]!
    stp     x29, x30, [sp, #-16]!

    /* Save current SP to *old_sp */
    mov     x9, sp
    str     x9, [x0]

    /* Load new SP */
    mov     sp, x1

    /* Restore callee-saved registers from new stack */
    ldp     x29, x30, [sp], #16
    ldp     x27, x28, [sp], #16
    ldp     x25, x26, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16

    ret
.size arch_context_switch, . - arch_context_switch


/* ===========================================================================
 * void arch_enter_userspace(u64 entry, u64 user_sp)
 *
 * Drops from EL1 to EL0 at the given entry point with the given stack.
 * =========================================================================== */

.global arch_enter_userspace
.type arch_enter_userspace, @function
arch_enter_userspace:
    msr     elr_el1, x0            /* Entry point */
    msr     sp_el0, x1             /* User stack pointer */

    mov     x0, #0                 /* SPSR: EL0t, interrupts enabled */
    msr     spsr_el1, x0

    /* Zero all general-purpose registers for security */
    mov     x0, xzr
    mov     x1, xzr
    mov     x2, xzr
    mov     x3, xzr
    mov     x4, xzr
    mov     x5, xzr
    mov     x6, xzr
    mov     x7, xzr
    mov     x8, xzr
    mov     x9, xzr
    mov     x10, xzr
    mov     x11, xzr
    mov     x12, xzr
    mov     x13, xzr
    mov     x14, xzr
    mov     x15, xzr
    mov     x16, xzr
    mov     x17, xzr
    mov     x18, xzr
    mov     x19, xzr
    mov     x20, xzr
    mov     x21, xzr
    mov     x22, xzr
    mov     x23, xzr
    mov     x24, xzr
    mov     x25, xzr
    mov     x26, xzr
    mov     x27, xzr
    mov     x28, xzr
    mov     x29, xzr
    mov     x30, xzr

    eret
.size arch_enter_userspace, . - arch_enter_userspace

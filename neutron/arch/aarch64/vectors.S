/* ===========================================================================
 * Neutron Kernel — AArch64 Exception Vectors
 * ===========================================================================
 * ARMv8-A exception vector table. Each entry is 128 bytes (32 instructions).
 * Table must be aligned to 2048 bytes.
 *
 * Layout (16 entries, 4 groups of 4):
 *   - Current EL with SP_EL0:   Sync, IRQ, FIQ, SError
 *   - Current EL with SP_ELx:   Sync, IRQ, FIQ, SError
 *   - Lower EL (AArch64):       Sync, IRQ, FIQ, SError
 *   - Lower EL (AArch32):       Sync, IRQ, FIQ, SError
 * =========================================================================== */

.section .text.vectors, "ax"
.balign 2048
.global exception_vectors
exception_vectors:

/* ===== Current EL with SP_EL0 (shouldn't happen — we use SP_EL1) ======== */
.balign 128
    b       exc_sync_cur_sp0

.balign 128
    b       exc_irq_cur_sp0

.balign 128
    b       exc_fiq_cur_sp0

.balign 128
    b       exc_serror_cur_sp0

/* ===== Current EL with SP_ELx (kernel exceptions) ======================== */
.balign 128
    b       exc_sync_cur_spx

.balign 128
    b       exc_irq_cur_spx

.balign 128
    b       exc_fiq_cur_spx

.balign 128
    b       exc_serror_cur_spx

/* ===== Lower EL (AArch64) — system calls and user-mode exceptions ======== */
.balign 128
    b       exc_sync_lower64

.balign 128
    b       exc_irq_lower64

.balign 128
    b       exc_fiq_lower64

.balign 128
    b       exc_serror_lower64

/* ===== Lower EL (AArch32) — not supported ================================ */
.balign 128
    b       exc_unsupported

.balign 128
    b       exc_unsupported

.balign 128
    b       exc_unsupported

.balign 128
    b       exc_unsupported


/* ===========================================================================
 * Trap frame: save all general-purpose registers to the stack.
 * Layout: x0-x30, SP_EL0, ELR_EL1, SPSR_EL1 (34 * 8 = 272 bytes)
 * =========================================================================== */

.macro SAVE_CONTEXT
    sub     sp, sp, #272

    stp     x0,  x1,  [sp, #0]
    stp     x2,  x3,  [sp, #16]
    stp     x4,  x5,  [sp, #32]
    stp     x6,  x7,  [sp, #48]
    stp     x8,  x9,  [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    str     x30,       [sp, #240]

    mrs     x0, sp_el0
    str     x0,        [sp, #248]

    mrs     x0, elr_el1
    mrs     x1, spsr_el1
    stp     x0, x1,    [sp, #256]
.endm

.macro RESTORE_CONTEXT
    ldp     x0, x1,    [sp, #256]
    msr     elr_el1, x0
    msr     spsr_el1, x1

    ldr     x0,        [sp, #248]
    msr     sp_el0, x0

    ldp     x0,  x1,  [sp, #0]
    ldp     x2,  x3,  [sp, #16]
    ldp     x4,  x5,  [sp, #32]
    ldp     x6,  x7,  [sp, #48]
    ldp     x8,  x9,  [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    ldr     x30,       [sp, #240]

    add     sp, sp, #272
.endm


/* ===========================================================================
 * Exception handlers
 * =========================================================================== */

exc_sync_cur_sp0:
exc_irq_cur_sp0:
exc_fiq_cur_sp0:
exc_serror_cur_sp0:
    SAVE_CONTEXT
    mov     x0, sp
    mov     x1, #0          /* source = current EL, SP_EL0 */
    bl      exception_handler
    RESTORE_CONTEXT
    eret

exc_sync_cur_spx:
    SAVE_CONTEXT
    mov     x0, sp
    mov     x1, #1          /* source = current EL, SP_ELx (sync) */
    bl      exception_handler
    RESTORE_CONTEXT
    eret

exc_irq_cur_spx:
    SAVE_CONTEXT
    mov     x0, sp
    mov     x1, #2          /* source = current EL, SP_ELx (IRQ) */
    bl      irq_handler
    RESTORE_CONTEXT
    eret

exc_fiq_cur_spx:
exc_serror_cur_spx:
    SAVE_CONTEXT
    mov     x0, sp
    mov     x1, #3          /* source = current EL, SP_ELx (FIQ/SError) */
    bl      exception_handler
    RESTORE_CONTEXT
    eret

exc_sync_lower64:
    SAVE_CONTEXT
    mov     x0, sp
    mov     x1, #4          /* source = lower EL (sync — syscall) */
    bl      syscall_entry
    RESTORE_CONTEXT
    eret

exc_irq_lower64:
    SAVE_CONTEXT
    mov     x0, sp
    mov     x1, #5          /* source = lower EL (IRQ) */
    bl      irq_handler
    RESTORE_CONTEXT
    eret

exc_fiq_lower64:
exc_serror_lower64:
    SAVE_CONTEXT
    mov     x0, sp
    mov     x1, #6          /* source = lower EL (FIQ/SError) */
    bl      exception_handler
    RESTORE_CONTEXT
    eret

exc_unsupported:
    SAVE_CONTEXT
    mov     x0, sp
    mov     x1, #7          /* unsupported (AArch32 lower EL) */
    bl      exception_handler
    RESTORE_CONTEXT
    eret

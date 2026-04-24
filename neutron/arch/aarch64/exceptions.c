/* ===========================================================================
 * Neutron Kernel — Exception Handler (C side)
 * ===========================================================================
 * Called from vectors.S after saving context. Handles synchronous exceptions,
 * IRQs, and routes system calls.
 * =========================================================================== */

#include <neutron/arch/aarch64.h>
#include <neutron/console.h>
#include <neutron/string.h>

/* ---- Trap frame layout (matches vectors.S) ------------------------------ */
typedef struct {
    u64 x[31];         /* x0 - x30 */
    u64 sp_el0;        /* Saved EL0 stack pointer */
    u64 elr_el1;       /* Exception return address */
    u64 spsr_el1;      /* Saved program status */
} trap_frame_t;

/* Forward declarations */
void timer_irq_handler(void);
u32  gic_acknowledge(void);
void gic_end_of_interrupt(u32 irq);
void scheduler_tick(void);
void syscall_dispatch(trap_frame_t *frame);

/* ---- ESR_EL1 decoding -------------------------------------------------- */
#define ESR_EC_SHIFT    26
#define ESR_EC_MASK     0x3F
#define ESR_EC_SVC64    0x15    /* SVC from AArch64 */
#define ESR_EC_DABT_CUR 0x25    /* Data abort from current EL */
#define ESR_EC_IABT_CUR 0x21    /* Instruction abort from current EL */
#define ESR_EC_DABT_LOW 0x24    /* Data abort from lower EL */
#define ESR_EC_IABT_LOW 0x20    /* Instruction abort from lower EL */

static const char *ec_to_str(u32 ec)
{
    switch (ec) {
    case 0x00: return "Unknown";
    case 0x01: return "WFI/WFE trapped";
    case 0x15: return "SVC (AArch64)";
    case 0x18: return "MSR/MRS trapped";
    case 0x20: return "Instruction abort (lower EL)";
    case 0x21: return "Instruction abort (current EL)";
    case 0x22: return "PC alignment fault";
    case 0x24: return "Data abort (lower EL)";
    case 0x25: return "Data abort (current EL)";
    case 0x26: return "SP alignment fault";
    case 0x2C: return "FP exception";
    case 0x30: return "Breakpoint (lower EL)";
    case 0x31: return "Breakpoint (current EL)";
    case 0x32: return "Software step (lower EL)";
    case 0x33: return "Software step (current EL)";
    case 0x34: return "Watchpoint (lower EL)";
    case 0x35: return "Watchpoint (current EL)";
    case 0x3C: return "BRK instruction";
    default:   return "Reserved";
    }
}

/* ---- Generic exception handler ------------------------------------------ */

void exception_handler(trap_frame_t *frame, u64 source)
{
    u64 esr  = READ_SYSREG(esr_el1);
    u64 far  = READ_SYSREG(far_el1);
    u32 ec   = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;

    kprintf("\n*** EXCEPTION (source=%lu) ***\n", source);
    kprintf("  ESR_EL1 : 0x%016lx (EC=0x%02x: %s)\n", esr, ec, ec_to_str(ec));
    kprintf("  FAR_EL1 : 0x%016lx\n", far);
    kprintf("  ELR_EL1 : 0x%016lx\n", frame->elr_el1);
    kprintf("  SPSR_EL1: 0x%016lx\n", frame->spsr_el1);
    kprintf("  SP_EL0  : 0x%016lx\n", frame->sp_el0);

    /* Dump key registers */
    kprintf("  x0=%016lx x1=%016lx x2=%016lx x3=%016lx\n",
            frame->x[0], frame->x[1], frame->x[2], frame->x[3]);
    kprintf("  x29=%016lx x30=%016lx\n", frame->x[29], frame->x[30]);

    /* For kernel-mode faults, panic */
    if (source <= 3) {
        panic("Unhandled kernel exception at 0x%016lx\n", frame->elr_el1);
    }

    /* For user-mode faults, kill the process (TODO: signal delivery) */
    kprintf("  [FATAL] User process caused unhandled exception — terminating\n");
    /* TODO: send signal to process or kill it */
}

/* ---- IRQ handler -------------------------------------------------------- */

void irq_handler(trap_frame_t *frame, u64 source)
{
    (void)frame;
    (void)source;

    u32 irq = gic_acknowledge();

    /* Spurious interrupt check (INTID 1020-1023) */
    if (irq >= 1020) {
        return;
    }

    switch (irq) {
    case TIMER_IRQ_EL1_PHYS:
        timer_irq_handler();
        scheduler_tick();
        break;

    default:
        kprintf("[neutron] Unhandled IRQ %u\n", irq);
        break;
    }

    gic_end_of_interrupt(irq);
}

/* ---- System call entry -------------------------------------------------- */

void syscall_entry(trap_frame_t *frame, u64 source)
{
    (void)source;

    u64 esr = READ_SYSREG(esr_el1);
    u32 ec  = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;

    if (ec == ESR_EC_SVC64) {
        syscall_dispatch(frame);
    } else {
        exception_handler(frame, source);
    }
}

/* ---- Install exception vectors ------------------------------------------ */

extern void exception_vectors(void);

void exceptions_init(void)
{
    WRITE_SYSREG(vbar_el1, (u64)exception_vectors);
    isb();
    kprintf("[neutron] Exception vectors installed at %p\n",
            (void *)exception_vectors);
}

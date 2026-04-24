/* ===========================================================================
 * Neutron Kernel — ARM Generic Timer Driver
 * ===========================================================================
 * Uses the EL1 physical timer (CNTP) for scheduling ticks.
 * Timer frequency is read from CNTFRQ_EL0.
 * =========================================================================== */

#include <neutron/arch/aarch64.h>
#include <neutron/console.h>

/* Forward declarations */
void gic_enable_irq(u32 irq);
void gic_set_priority(u32 irq, u8 priority);

static u64 timer_freq;         /* Timer frequency in Hz */
static u64 tick_interval;      /* Ticks per scheduling quantum */
static volatile u64 system_ticks;  /* Monotonic tick counter */

#define SCHED_HZ        100    /* 100 Hz = 10 ms quantum */

/* ---- Timer register accessors ------------------------------------------- */

static ALWAYS_INLINE u64 read_cntfrq(void)
{
    return READ_SYSREG(cntfrq_el0);
}

static ALWAYS_INLINE u64 read_cntpct(void)
{
    return READ_SYSREG(cntpct_el0);
}

static ALWAYS_INLINE void write_cntp_tval(u64 val)
{
    WRITE_SYSREG(cntp_tval_el0, val);
}

static ALWAYS_INLINE void write_cntp_ctl(u64 val)
{
    WRITE_SYSREG(cntp_ctl_el0, val);
}

static ALWAYS_INLINE u64 read_cntp_ctl(void)
{
    return READ_SYSREG(cntp_ctl_el0);
}

/* ---- Public API --------------------------------------------------------- */

void timer_init(void)
{
    timer_freq = read_cntfrq();
    tick_interval = timer_freq / SCHED_HZ;
    system_ticks = 0;

    /* Disable timer */
    write_cntp_ctl(0);

    /* Set up the GIC for the timer IRQ (PPI 30) */
    gic_set_priority(TIMER_IRQ_EL1_PHYS, 0x40);
    gic_enable_irq(TIMER_IRQ_EL1_PHYS);

    /* Set the timer compare value */
    write_cntp_tval(tick_interval);

    /* Enable timer, unmask interrupt */
    write_cntp_ctl(1); /* ENABLE=1, IMASK=0 */

    kprintf("[neutron] Timer: freq=%lu Hz, quantum=%lu ticks (%d Hz)\n",
            timer_freq, tick_interval, SCHED_HZ);
}

void timer_irq_handler(void)
{
    system_ticks++;

    /* Re-arm the timer for the next quantum */
    write_cntp_tval(tick_interval);
}

u64 timer_get_ticks(void)
{
    return system_ticks;
}

u64 timer_get_timestamp_ns(void)
{
    u64 cnt = read_cntpct();
    /* ns = cnt * 1e9 / freq — avoid overflow with division first */
    return (cnt / (timer_freq / 1000000)) * 1000;
}

u64 timer_get_frequency(void)
{
    return timer_freq;
}

void timer_delay_us(u64 us)
{
    u64 start = read_cntpct();
    u64 target = start + (timer_freq * us) / 1000000;
    while (read_cntpct() < target)
        ;
}

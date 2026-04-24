/* ===========================================================================
 * Neutron Kernel — GICv3 Interrupt Controller Driver
 * ===========================================================================
 * Minimal GICv3 driver for QEMU virt machine.
 * Handles distributor, redistributor, and CPU interface initialization.
 * =========================================================================== */

#include <neutron/arch/aarch64.h>
#include <neutron/console.h>

/* ---- GIC Distributor ---------------------------------------------------- */

static void gicd_init(void)
{
    vaddr_t base = GICD_BASE;

    /* Disable distributor */
    mmio_write32(base + GICD_CTLR, 0);

    /* Read how many IRQ lines are supported */
    u32 typer = mmio_read32(base + GICD_TYPER);
    u32 num_irqs = ((typer & 0x1F) + 1) * 32;
    if (num_irqs > 1020)
        num_irqs = 1020;

    /* Disable all interrupts */
    for (u32 i = 0; i < num_irqs / 32; i++)
        mmio_write32(base + GICD_ICENABLER + i * 4, 0xFFFFFFFF);

    /* Set all SPIs to lowest priority */
    for (u32 i = 32; i < num_irqs; i += 4)
        mmio_write32(base + GICD_IPRIORITYR + i, 0xA0A0A0A0);

    /* Set all SPIs to level-triggered, group 1 */
    for (u32 i = 2; i < num_irqs / 16; i++)
        mmio_write32(base + GICD_ICFGR + i * 4, 0);

    /* Enable distributor with ARE (Affinity Routing Enable) for group 1 */
    mmio_write32(base + GICD_CTLR, (1 << 1) | (1 << 4)); /* EnableGrp1NS | ARE_NS */
}

/* ---- GIC Redistributor -------------------------------------------------- */

static void gicr_init(void)
{
    vaddr_t base = GICR_BASE;

    /* Wake up the redistributor */
    u32 waker = mmio_read32(base + GICR_WAKER);
    waker &= ~(1 << 1); /* Clear ProcessorSleep */
    mmio_write32(base + GICR_WAKER, waker);

    /* Wait for ChildrenAsleep to clear */
    while (mmio_read32(base + GICR_WAKER) & (1 << 2))
        ;

    /* SGI/PPI redistributor base */
    vaddr_t sgi_base = base + GICR_SGI_BASE;

    /* Disable all PPIs and SGIs */
    mmio_write32(sgi_base + 0x180, 0xFFFFFFFF); /* ICENABLER0 */

    /* Set all to lowest priority */
    for (u32 i = 0; i < 8; i++)
        mmio_write32(sgi_base + 0x400 + i * 4, 0xA0A0A0A0);

    /* Set all to group 1 */
    mmio_write32(sgi_base + 0x080, 0xFFFFFFFF); /* IGROUPR0 */

    /* Wait for RWP */
    while (mmio_read32(base + 0x0008) & (1 << 3)) /* GICR_CTLR.RWP */
        ;
}

/* ---- GIC CPU Interface (System Registers) ------------------------------- */

static void gicc_init(void)
{
    /* Enable system register access */
    u64 sre;
    __asm__ volatile("mrs %0, " "S3_0_C12_C12_5" : "=r"(sre));
    sre |= 1; /* SRE bit */
    __asm__ volatile("msr " "S3_0_C12_C12_5" ", %0" :: "r"(sre));
    isb();

    /* Set priority mask to accept all priorities */
    __asm__ volatile("msr " "S3_0_C4_C6_0" ", %0" :: "r"((u64)0xFF));

    /* Enable group 1 interrupts */
    __asm__ volatile("msr " "S3_0_C12_C12_7" ", %0" :: "r"((u64)1));

    isb();
}

/* ---- Public API --------------------------------------------------------- */

void gic_init(void)
{
    gicd_init();
    gicr_init();
    gicc_init();
    kprintf("[neutron] GICv3 initialized\n");
}

void gic_enable_irq(u32 irq)
{
    if (irq < 32) {
        /* SGI/PPI — use redistributor */
        vaddr_t sgi_base = GICR_BASE + GICR_SGI_BASE;
        mmio_write32(sgi_base + 0x100, 1 << irq); /* ISENABLER0 */
    } else {
        /* SPI — use distributor */
        u32 reg = irq / 32;
        u32 bit = irq % 32;
        mmio_write32(GICD_BASE + GICD_ISENABLER + reg * 4, 1 << bit);
    }
}

void gic_disable_irq(u32 irq)
{
    if (irq < 32) {
        vaddr_t sgi_base = GICR_BASE + GICR_SGI_BASE;
        mmio_write32(sgi_base + 0x180, 1 << irq); /* ICENABLER0 */
    } else {
        u32 reg = irq / 32;
        u32 bit = irq % 32;
        mmio_write32(GICD_BASE + GICD_ICENABLER + reg * 4, 1 << bit);
    }
}

u32 gic_acknowledge(void)
{
    u64 iar;
    __asm__ volatile("mrs %0, " "S3_0_C12_C12_0" : "=r"(iar));
    return (u32)iar;
}

void gic_end_of_interrupt(u32 irq)
{
    __asm__ volatile("msr " "S3_0_C12_C12_1" ", %0" :: "r"((u64)irq));
}

void gic_set_priority(u32 irq, u8 priority)
{
    if (irq < 32) {
        vaddr_t sgi_base = GICR_BASE + GICR_SGI_BASE;
        u32 offset = irq & ~3;
        u32 shift = (irq & 3) * 8;
        u32 val = mmio_read32(sgi_base + 0x400 + offset);
        val &= ~(0xFF << shift);
        val |= (u32)priority << shift;
        mmio_write32(sgi_base + 0x400 + offset, val);
    } else {
        u32 offset = (irq & ~3);
        u32 shift = (irq & 3) * 8;
        u32 val = mmio_read32(GICD_BASE + GICD_IPRIORITYR + offset);
        val &= ~(0xFF << shift);
        val |= (u32)priority << shift;
        mmio_write32(GICD_BASE + GICD_IPRIORITYR + offset, val);
    }
}

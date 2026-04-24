/* ===========================================================================
 * Neutron Kernel — AArch64 Architecture Definitions
 * =========================================================================== */

#ifndef NEUTRON_ARCH_AARCH64_H
#define NEUTRON_ARCH_AARCH64_H

#include <neutron/types.h>

/* ---- Exception Levels --------------------------------------------------- */
#define EL0     0       /* User / application */
#define EL1     1       /* Kernel */
#define EL2     2       /* Hypervisor */
#define EL3     3       /* Secure monitor */

/* ---- System register accessors ----------------------------------------- */
#define READ_SYSREG(reg) ({                     \
    u64 __val;                                  \
    __asm__ volatile("mrs %0, " #reg            \
                     : "=r"(__val));             \
    __val;                                      \
})

#define WRITE_SYSREG(reg, val) do {             \
    __asm__ volatile("msr " #reg ", %0"         \
                     :: "r"((u64)(val)));        \
} while (0)

/* ---- SCTLR_EL1 bits ---------------------------------------------------- */
#define SCTLR_M        BIT(0)          /* MMU enable */
#define SCTLR_A        BIT(1)          /* Alignment check enable */
#define SCTLR_C        BIT(2)          /* Data cache enable */
#define SCTLR_I        BIT(12)         /* Instruction cache enable */
#define SCTLR_WXN      BIT(19)         /* Write-execute never */
#define SCTLR_SPAN     BIT(23)         /* Set PAN */
#define SCTLR_EnIA     BIT(31)         /* PAC for instructions */

/* ---- TCR_EL1 bits ------------------------------------------------------- */
#define TCR_T0SZ(n)    ((u64)(n))              /* TTBR0 region size */
#define TCR_T1SZ(n)    ((u64)(n) << 16)        /* TTBR1 region size */
#define TCR_TG0_4K     (0ULL << 14)
#define TCR_TG1_4K     (2ULL << 30)
#define TCR_SH0_INNER  (3ULL << 12)
#define TCR_SH1_INNER  (3ULL << 28)
#define TCR_ORGN0_WB   (1ULL << 10)
#define TCR_IRGN0_WB   (1ULL << 8)
#define TCR_ORGN1_WB   (1ULL << 26)
#define TCR_IRGN1_WB   (1ULL << 24)
#define TCR_IPS_48BIT  (5ULL << 32)

/* ---- Page table entry bits ---------------------------------------------- */
#define PTE_VALID       BIT(0)
#define PTE_TABLE       BIT(1)          /* Table descriptor (levels 0-2) */
#define PTE_PAGE        BIT(1)          /* Page descriptor (level 3) */
#define PTE_AF          BIT(10)         /* Access flag */
#define PTE_SH_INNER   (3ULL << 8)     /* Inner shareable */
#define PTE_AP_RW_EL1  (0ULL << 6)     /* EL1 RW, EL0 no access */
#define PTE_AP_RW_ALL  (1ULL << 6)     /* EL1 RW, EL0 RW */
#define PTE_AP_RO_EL1  (2ULL << 6)     /* EL1 RO, EL0 no access */
#define PTE_AP_RO_ALL  (3ULL << 6)     /* EL1 RO, EL0 RO */
#define PTE_UXN        BIT(54)         /* Unprivileged execute never */
#define PTE_PXN        BIT(53)         /* Privileged execute never */

/* Memory attribute indices (MAIR_EL1) */
#define MATTR_DEVICE    0              /* Device-nGnRnE */
#define MATTR_NORMAL    1              /* Normal, Write-Back */
#define MATTR_NC        2              /* Normal, Non-Cacheable */
#define PTE_MATTR(idx)  ((u64)(idx) << 2)

/* MAIR_EL1 value: idx0=Device, idx1=Normal-WB, idx2=Normal-NC */
#define MAIR_VALUE      (0x00ULL | (0xFFULL << 8) | (0x44ULL << 16))

/* ---- SPSR_EL1 (saved program status) ----------------------------------- */
#define SPSR_EL0t       0x00000000ULL  /* EL0, SP_EL0, AArch64 */
#define SPSR_EL1h       0x00000005ULL  /* EL1, SP_EL1, AArch64 */
#define SPSR_DAIF_MASK  0x000003C0ULL  /* All exceptions masked */

/* ---- GICv3 registers ---------------------------------------------------- */
#define GICD_BASE       0x08000000ULL  /* QEMU virt GICv3 distributor */
#define GICR_BASE       0x080A0000ULL  /* QEMU virt GICv3 redistributor */

/* GIC Distributor registers */
#define GICD_CTLR       0x0000
#define GICD_TYPER      0x0004
#define GICD_ISENABLER  0x0100
#define GICD_ICENABLER  0x0180
#define GICD_ISPENDR    0x0200
#define GICD_IPRIORITYR 0x0400
#define GICD_ITARGETSR  0x0800
#define GICD_ICFGR      0x0C00

/* GIC Redistributor registers */
#define GICR_WAKER      0x0014
#define GICR_SGI_BASE   0x10000

/* GIC CPU interface (system registers for GICv3) */
#define ICC_SRE_EL1     S3_0_C12_C12_5
#define ICC_PMR_EL1     S3_0_C4_C6_0
#define ICC_IAR1_EL1    S3_0_C12_C12_0
#define ICC_EOIR1_EL1   S3_0_C12_C12_1
#define ICC_IGRPEN1_EL1 S3_0_C12_C12_7
#define ICC_CTLR_EL1    S3_0_C12_C12_4

/* ---- ARM Generic Timer -------------------------------------------------- */
#define TIMER_IRQ_EL1_PHYS  30  /* PPI 14 = INTID 30 */
#define TIMER_IRQ_EL1_VIRT  27  /* PPI 11 = INTID 27 */

/* ---- QEMU virt platform ------------------------------------------------ */
#define UART0_BASE      0x09000000ULL  /* PL011 UART */
#define UART0_IRQ       33             /* SPI 1 */
#define RAM_BASE        0x40000000ULL
#define RAM_SIZE        0x80000000ULL  /* 2 GB default */

/* ---- Inline helpers ----------------------------------------------------- */

static ALWAYS_INLINE void arch_halt(void)
{
    for (;;)
        __asm__ volatile("wfi");
}

static ALWAYS_INLINE void arch_enable_irqs(void)
{
    __asm__ volatile("msr daifclr, #0xf");
}

static ALWAYS_INLINE void arch_disable_irqs(void)
{
    __asm__ volatile("msr daifset, #0xf");
}

static ALWAYS_INLINE u64 arch_read_sp(void)
{
    u64 sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    return sp;
}

static ALWAYS_INLINE u64 arch_read_pc(void)
{
    u64 pc;
    __asm__ volatile("adr %0, ." : "=r"(pc));
    return pc;
}

static ALWAYS_INLINE u64 arch_current_el(void)
{
    return (READ_SYSREG(CurrentEL) >> 2) & 3;
}

/* MMIO accessors */
static ALWAYS_INLINE void mmio_write32(vaddr_t addr, u32 val)
{
    *(volatile u32 *)addr = val;
    dsb(st);
}

static ALWAYS_INLINE u32 mmio_read32(vaddr_t addr)
{
    u32 val = *(volatile u32 *)addr;
    dsb(ld);
    return val;
}

static ALWAYS_INLINE void mmio_write64(vaddr_t addr, u64 val)
{
    *(volatile u64 *)addr = val;
    dsb(st);
}

static ALWAYS_INLINE u64 mmio_read64(vaddr_t addr)
{
    u64 val = *(volatile u64 *)addr;
    dsb(ld);
    return val;
}

#endif /* NEUTRON_ARCH_AARCH64_H */

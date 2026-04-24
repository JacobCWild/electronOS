/* ===========================================================================
 * Neutron Kernel — Virtual Memory Manager
 * ===========================================================================
 * 4-level page table management for AArch64 (4 KB granule, 48-bit VA).
 *
 * Address space layout:
 *   0x0000_0000_0000 - 0x0000_FFFF_FFFF_FFFF : User space (TTBR0)
 *   0xFFFF_0000_0000 - 0xFFFF_FFFF_FFFF_FFFF : Kernel space (TTBR1)
 *
 * During early boot, we use identity mapping (VA == PA) for simplicity.
 * The kernel later sets up proper page tables with:
 *   - W^X enforcement (no page is both writable and executable)
 *   - Guard pages around stacks
 *   - Read-only kernel text after init
 * =========================================================================== */

#include <neutron/memory.h>
#include <neutron/arch/aarch64.h>
#include <neutron/console.h>
#include <neutron/string.h>

page_table_t kernel_page_table;

/* ---- Page table helpers ------------------------------------------------- */

/* 4-level page table indices for a virtual address */
#define L0_INDEX(va)    (((va) >> 39) & 0x1FF)
#define L1_INDEX(va)    (((va) >> 30) & 0x1FF)
#define L2_INDEX(va)    (((va) >> 21) & 0x1FF)
#define L3_INDEX(va)    (((va) >> 12) & 0x1FF)

#define ENTRIES_PER_TABLE   512
#define TABLE_SIZE          (ENTRIES_PER_TABLE * sizeof(u64))

/* Allocate a zeroed page table page */
static paddr_t alloc_table_page(void)
{
    paddr_t pa = pmm_alloc_page();
    if (pa == 0)
        panic("vmm: out of memory for page tables");
    /* Already zeroed by pmm_alloc_page */
    return pa;
}

/* Get or create a next-level table entry */
static u64 *walk_table(u64 *table, usize index, bool create)
{
    if (table[index] & PTE_VALID) {
        /* Existing table entry — extract address */
        paddr_t next = table[index] & 0x0000FFFFFFFFF000ULL;
        return (u64 *)next;
    }

    if (!create)
        return NULL;

    /* Allocate a new table */
    paddr_t new_table = alloc_table_page();
    table[index] = new_table | PTE_VALID | PTE_TABLE;
    return (u64 *)new_table;
}

/* Convert VM flags to AArch64 PTE attributes */
static u64 flags_to_pte(u32 flags)
{
    u64 pte = PTE_VALID | PTE_PAGE | PTE_AF | PTE_SH_INNER;

    /* Memory type */
    if (flags & VM_DEVICE) {
        pte |= PTE_MATTR(MATTR_DEVICE);
    } else {
        pte |= PTE_MATTR(MATTR_NORMAL);
    }

    /* Access permissions */
    if (flags & VM_USER) {
        if (flags & VM_WRITE)
            pte |= PTE_AP_RW_ALL;
        else
            pte |= PTE_AP_RO_ALL;
    } else {
        if (flags & VM_WRITE)
            pte |= PTE_AP_RW_EL1;
        else
            pte |= PTE_AP_RO_EL1;
    }

    /* W^X enforcement: if writable, mark as no-execute */
    if (flags & VM_WRITE) {
        pte |= PTE_UXN | PTE_PXN;
    }

    /* Execute permissions */
    if (!(flags & VM_EXEC)) {
        pte |= PTE_PXN;
        if (flags & VM_USER)
            pte |= PTE_UXN;
    }

    /* User-accessible pages are never kernel-executable */
    if (flags & VM_USER)
        pte |= PTE_PXN;

    return pte;
}

/* ---- Public API --------------------------------------------------------- */

nk_result_t vmm_init(void)
{
    /* Allocate the kernel L0 page table */
    kernel_page_table.root = alloc_table_page();
    kernel_page_table.mapped_pages = 0;

    kprintf("[neutron] VMM: kernel page table at 0x%lx\n",
            kernel_page_table.root);

    return NK_OK;
}

nk_result_t vmm_create_table(page_table_t *pt)
{
    pt->root = alloc_table_page();
    pt->mapped_pages = 0;
    return NK_OK;
}

void vmm_destroy_table(page_table_t *pt)
{
    /* TODO: walk and free all table pages */
    (void)pt;
}

nk_result_t vmm_map_page(page_table_t *pt, vaddr_t va, paddr_t pa, u32 flags)
{
    u64 *l0 = (u64 *)pt->root;
    u64 *l1 = walk_table(l0, L0_INDEX(va), true);
    u64 *l2 = walk_table(l1, L1_INDEX(va), true);
    u64 *l3 = walk_table(l2, L2_INDEX(va), true);

    usize idx = L3_INDEX(va);

    /* Security: don't silently overwrite existing mappings */
    if (l3[idx] & PTE_VALID) {
        return NK_ERR_EXIST;
    }

    l3[idx] = (pa & 0x0000FFFFFFFFF000ULL) | flags_to_pte(flags);
    pt->mapped_pages++;

    /* Invalidate TLB for this address */
    __asm__ volatile("tlbi vale1is, %0" :: "r"(va >> 12));
    dsb(ish);
    isb();

    return NK_OK;
}

nk_result_t vmm_map_range(page_table_t *pt, vaddr_t va, paddr_t pa,
                          usize size, u32 flags)
{
    usize pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (usize i = 0; i < pages; i++) {
        nk_result_t r = vmm_map_page(pt, va + i * PAGE_SIZE,
                                     pa + i * PAGE_SIZE, flags);
        if (r != NK_OK && r != NK_ERR_EXIST)
            return r;
    }
    return NK_OK;
}

nk_result_t vmm_unmap_page(page_table_t *pt, vaddr_t va)
{
    u64 *l0 = (u64 *)pt->root;
    u64 *l1 = walk_table(l0, L0_INDEX(va), false);
    if (!l1) return NK_ERR_NOENT;

    u64 *l2 = walk_table(l1, L1_INDEX(va), false);
    if (!l2) return NK_ERR_NOENT;

    u64 *l3 = walk_table(l2, L2_INDEX(va), false);
    if (!l3) return NK_ERR_NOENT;

    usize idx = L3_INDEX(va);
    if (!(l3[idx] & PTE_VALID))
        return NK_ERR_NOENT;

    l3[idx] = 0;
    pt->mapped_pages--;

    __asm__ volatile("tlbi vale1is, %0" :: "r"(va >> 12));
    dsb(ish);
    isb();

    return NK_OK;
}

paddr_t vmm_translate(page_table_t *pt, vaddr_t va)
{
    u64 *l0 = (u64 *)pt->root;
    u64 *l1 = walk_table(l0, L0_INDEX(va), false);
    if (!l1) return 0;

    u64 *l2 = walk_table(l1, L1_INDEX(va), false);
    if (!l2) return 0;

    u64 *l3 = walk_table(l2, L2_INDEX(va), false);
    if (!l3) return 0;

    usize idx = L3_INDEX(va);
    if (!(l3[idx] & PTE_VALID))
        return 0;

    return (l3[idx] & 0x0000FFFFFFFFF000ULL) | (va & ~PAGE_MASK);
}

void vmm_switch_table(page_table_t *pt)
{
    WRITE_SYSREG(ttbr0_el1, pt->root);
    isb();
    __asm__ volatile("tlbi vmalle1is");
    dsb(ish);
    isb();
}

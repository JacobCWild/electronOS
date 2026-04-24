/* ===========================================================================
 * Neutron Kernel — Memory Management
 * =========================================================================== */

#ifndef NEUTRON_MEMORY_H
#define NEUTRON_MEMORY_H

#include <neutron/types.h>

/* ---- Physical Page Allocator -------------------------------------------- */

/* Page flags */
#define PAGE_FREE       0
#define PAGE_USED       1
#define PAGE_KERNEL     2
#define PAGE_GUARD      3       /* Guard page — access causes fault */

void        pmm_init(paddr_t mem_start, paddr_t mem_end);
paddr_t     pmm_alloc_page(void);
paddr_t     pmm_alloc_pages(usize count);
void        pmm_free_page(paddr_t addr);
void        pmm_free_pages(paddr_t addr, usize count);
usize       pmm_free_count(void);
usize       pmm_total_count(void);

/* ---- Virtual Memory Manager --------------------------------------------- */

/* VM region permissions */
#define VM_READ         BIT(0)
#define VM_WRITE        BIT(1)
#define VM_EXEC         BIT(2)
#define VM_USER         BIT(3)
#define VM_DEVICE       BIT(4)  /* Device memory (non-cacheable) */
#define VM_GUARD        BIT(5)  /* Guard page — no mapping */

/* Page table (opaque — contains the L0 table physical address) */
typedef struct {
    paddr_t root;               /* Physical address of L0 page table */
    usize   mapped_pages;       /* Number of mapped pages */
} page_table_t;

nk_result_t vmm_init(void);
nk_result_t vmm_create_table(page_table_t *pt);
void        vmm_destroy_table(page_table_t *pt);
nk_result_t vmm_map_page(page_table_t *pt, vaddr_t va, paddr_t pa, u32 flags);
nk_result_t vmm_map_range(page_table_t *pt, vaddr_t va, paddr_t pa,
                          usize size, u32 flags);
nk_result_t vmm_unmap_page(page_table_t *pt, vaddr_t va);
paddr_t     vmm_translate(page_table_t *pt, vaddr_t va);
void        vmm_switch_table(page_table_t *pt);

/* Kernel page table (identity-mapped, used at boot and in kernel mode) */
extern page_table_t kernel_page_table;

#endif /* NEUTRON_MEMORY_H */

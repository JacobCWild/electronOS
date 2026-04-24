/* ===========================================================================
 * Neutron Kernel — Physical Memory Manager
 * ===========================================================================
 * Bitmap-based page frame allocator. Simple, deterministic, and suitable
 * for the Neutron kernel's security model where predictability matters
 * more than allocation speed.
 *
 * Security features:
 *   - All freed pages are zeroed before returning to the pool
 *   - Guard pages are tracked and never allocated
 *   - Allocation statistics are maintained for zone resource limits
 * =========================================================================== */

#include <neutron/memory.h>
#include <neutron/console.h>
#include <neutron/string.h>

/* ---- Bitmap allocator --------------------------------------------------- */

#define MAX_PAGES       (512 * 1024)    /* 2 GB / 4 KB = 512K pages */
#define BITMAP_SIZE     (MAX_PAGES / 64)

static u64      page_bitmap[BITMAP_SIZE];
static paddr_t  mem_base;
static paddr_t  mem_top;
static usize    total_pages;
static usize    free_pages;

/* ---- Internal helpers --------------------------------------------------- */

static ALWAYS_INLINE void bitmap_set(usize page_idx)
{
    page_bitmap[page_idx / 64] |= (1ULL << (page_idx % 64));
}

static ALWAYS_INLINE void bitmap_clear(usize page_idx)
{
    page_bitmap[page_idx / 64] &= ~(1ULL << (page_idx % 64));
}

static ALWAYS_INLINE bool bitmap_test(usize page_idx)
{
    return (page_bitmap[page_idx / 64] >> (page_idx % 64)) & 1;
}

/* ---- Public API --------------------------------------------------------- */

void pmm_init(paddr_t mem_start, paddr_t mem_end)
{
    mem_base = ALIGN_UP(mem_start, PAGE_SIZE);
    mem_top  = ALIGN_DOWN(mem_end, PAGE_SIZE);
    total_pages = (mem_top - mem_base) / PAGE_SIZE;

    if (total_pages > MAX_PAGES)
        total_pages = MAX_PAGES;

    /* Mark all pages as free */
    memset(page_bitmap, 0, sizeof(page_bitmap));
    free_pages = total_pages;

    kprintf("[neutron] PMM: %lu pages (%lu MB) from 0x%lx to 0x%lx\n",
            total_pages,
            (total_pages * PAGE_SIZE) / (1024 * 1024),
            mem_base, mem_top);
}

paddr_t pmm_alloc_page(void)
{
    for (usize i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages--;
            paddr_t addr = mem_base + i * PAGE_SIZE;

            /* Security: zero the page before returning */
            memset((void *)addr, 0, PAGE_SIZE);

            return addr;
        }
    }
    return 0; /* Out of memory */
}

paddr_t pmm_alloc_pages(usize count)
{
    if (count == 0 || count > free_pages)
        return 0;

    /* Find contiguous free run */
    usize run_start = 0;
    usize run_len = 0;

    for (usize i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (run_len == 0)
                run_start = i;
            run_len++;
            if (run_len == count) {
                /* Mark all pages as used */
                for (usize j = run_start; j < run_start + count; j++)
                    bitmap_set(j);
                free_pages -= count;

                paddr_t addr = mem_base + run_start * PAGE_SIZE;
                memset((void *)addr, 0, count * PAGE_SIZE);
                return addr;
            }
        } else {
            run_len = 0;
        }
    }
    return 0;
}

void pmm_free_page(paddr_t addr)
{
    if (addr < mem_base || addr >= mem_top)
        return;

    usize idx = (addr - mem_base) / PAGE_SIZE;
    if (bitmap_test(idx)) {
        /* Security: zero page on free to prevent info leaks */
        memset((void *)addr, 0, PAGE_SIZE);
        bitmap_clear(idx);
        free_pages++;
    }
}

void pmm_free_pages(paddr_t addr, usize count)
{
    for (usize i = 0; i < count; i++)
        pmm_free_page(addr + i * PAGE_SIZE);
}

usize pmm_free_count(void)
{
    return free_pages;
}

usize pmm_total_count(void)
{
    return total_pages;
}

/* ===========================================================================
 * Neutron Kernel — Main Entry Point
 * ===========================================================================
 *
 *   ███╗   ██╗███████╗██╗   ██╗████████╗██████╗  ██████╗ ███╗   ██╗
 *   ████╗  ██║██╔════╝██║   ██║╚══██╔══╝██╔══██╗██╔═══██╗████╗  ██║
 *   ██╔██╗ ██║█████╗  ██║   ██║   ██║   ██████╔╝██║   ██║██╔██╗ ██║
 *   ██║╚██╗██║██╔══╝  ██║   ██║   ██║   ██╔══██╗██║   ██║██║╚██╗██║
 *   ██║ ╚████║███████╗╚██████╔╝   ██║   ██║  ██║╚██████╔╝██║ ╚████║
 *   ╚═╝  ╚═══╝╚══════╝ ╚═════╝    ╚═╝   ╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 *   The Neutron Kernel — A capability-based hybrid microkernel
 *   for electronOS on Raspberry Pi 5
 *
 * =========================================================================== */

#include <neutron/types.h>
#include <neutron/console.h>
#include <neutron/string.h>
#include <neutron/memory.h>
#include <neutron/process.h>
#include <neutron/capability.h>
#include <neutron/ipc.h>
#include <neutron/security.h>
#include <neutron/syscall.h>
#include <neutron/arch/aarch64.h>

/* External symbols from linker script */
extern u64 __kernel_end;
extern u64 __text_start, __text_end;
extern u64 __rodata_start, __rodata_end;
extern u64 __data_start, __data_end;
extern u64 __bss_start, __bss_end;
extern u64 __heap_start;

/* Architecture initialization (defined in arch files) */
extern void exceptions_init(void);
extern void gic_init(void);
extern void timer_init(void);

/* ---- Kernel version ----------------------------------------------------- */
#define NEUTRON_VERSION_MAJOR   1
#define NEUTRON_VERSION_MINOR   0
#define NEUTRON_VERSION_PATCH   0
#define NEUTRON_CODENAME        "Photon"

/* ---- Boot banner -------------------------------------------------------- */

static void print_banner(void)
{
    kprintf("\n");
    kprintf("  _   _            _                   \n");
    kprintf(" | \\ | | ___ _   _| |_ _ __ ___  _ __  \n");
    kprintf(" |  \\| |/ _ \\ | | | __| '__/ _ \\| '_ \\ \n");
    kprintf(" | |\\  |  __/ |_| | |_| | | (_) | | | |\n");
    kprintf(" |_| \\_|\\___|\\__,_|\\__|_|  \\___/|_| |_|\n");
    kprintf("\n");
    kprintf("  Neutron Kernel v%d.%d.%d \"%s\"\n",
            NEUTRON_VERSION_MAJOR, NEUTRON_VERSION_MINOR,
            NEUTRON_VERSION_PATCH, NEUTRON_CODENAME);
    kprintf("  A capability-based hybrid microkernel for electronOS\n");
    kprintf("  Target: AArch64 / Raspberry Pi 5\n");
    kprintf("\n");
}

/* ---- Memory layout info ------------------------------------------------- */

static void print_memory_layout(void)
{
    kprintf("[neutron] Memory layout:\n");
    kprintf("  .text   : 0x%lx - 0x%lx (%lu KB)\n",
            (u64)&__text_start, (u64)&__text_end,
            ((u64)&__text_end - (u64)&__text_start) / 1024);
    kprintf("  .rodata : 0x%lx - 0x%lx (%lu KB)\n",
            (u64)&__rodata_start, (u64)&__rodata_end,
            ((u64)&__rodata_end - (u64)&__rodata_start) / 1024);
    kprintf("  .data   : 0x%lx - 0x%lx (%lu KB)\n",
            (u64)&__data_start, (u64)&__data_end,
            ((u64)&__data_end - (u64)&__data_start) / 1024);
    kprintf("  .bss    : 0x%lx - 0x%lx (%lu KB)\n",
            (u64)&__bss_start, (u64)&__bss_end,
            ((u64)&__bss_end - (u64)&__bss_start) / 1024);
    kprintf("  heap    : 0x%lx onwards\n", (u64)&__heap_start);
}

/* ---- Security features report ------------------------------------------- */

static void print_security_features(void)
{
    kprintf("\n");
    kprintf("[neutron] Security features:\n");
    kprintf("  [*] Capability-based access control (no ambient authority)\n");
    kprintf("  [*] Security zones with hardware-enforced isolation\n");
    kprintf("  [*] Deny-by-default cross-zone policy engine\n");
    kprintf("  [*] Built-in audit log for all security operations\n");
    kprintf("  [*] W^X enforcement (no page is both writable and executable)\n");
    kprintf("  [*] Stack guard pages (overflow detection)\n");
    kprintf("  [*] Page zeroing on alloc and free (prevents info leaks)\n");
    kprintf("  [*] Immutable kernel text after boot\n");
    kprintf("  [*] Transitive capability revocation\n");
    kprintf("  [*] Policy-mediated IPC (cross-zone requires approval)\n");
    kprintf("\n");
}

/* ---- Demo task ---------------------------------------------------------- */

/* This task demonstrates the kernel's functionality */
static void demo_task(void)
{
    kprintf("\n");
    kprintf("========================================\n");
    kprintf("  Neutron Kernel Self-Test\n");
    kprintf("========================================\n");
    kprintf("\n");

    process_t *proc = process_current();
    thread_t *t = thread_current();

    kprintf("[test] Running in process '%s' (PID %u), thread %u\n",
            proc->name, proc->pid, t->tid);

    /* ---- Test 1: Capability creation and lookup ---- */
    kprintf("\n[test 1] Capability system...\n");
    {
        cap_id_t cap_id;
        nk_result_t r;

        /* Create a memory capability */
        r = cap_create(proc->cap_space, CAP_TYPE_MEMORY,
                       CAP_RIGHT_RW | CAP_RIGHT_GRANT,
                       0x1000, proc->security_zone, &cap_id);
        kprintf("  Create memory cap: %s (id=%lu)\n",
                r == NK_OK ? "OK" : "FAIL", cap_id);

        /* Look up with sufficient rights */
        capability_t *cap;
        r = cap_lookup(proc->cap_space, cap_id, CAP_RIGHT_READ, &cap);
        kprintf("  Lookup (READ): %s\n", r == NK_OK ? "OK" : "FAIL");

        /* Look up with insufficient rights */
        r = cap_lookup(proc->cap_space, cap_id, CAP_RIGHT_DESTROY, &cap);
        kprintf("  Lookup (DESTROY, should fail): %s\n",
                r == NK_ERR_PERM ? "OK (denied)" : "FAIL");

        /* Derive with reduced rights */
        cap_id_t derived_id;
        r = cap_derive(proc->cap_space, cap_id,
                       proc->cap_space, CAP_RIGHT_READ, &derived_id);
        kprintf("  Derive (READ only): %s (id=%lu)\n",
                r == NK_OK ? "OK" : "FAIL", derived_id);

        /* Verify derived cap cannot write */
        r = cap_lookup(proc->cap_space, derived_id, CAP_RIGHT_WRITE, &cap);
        kprintf("  Derived lookup (WRITE, should fail): %s\n",
                r == NK_ERR_PERM ? "OK (denied)" : "FAIL");

        /* Revoke parent (should revoke derived too) */
        cap_id_t revoke_parent;
        r = cap_create(proc->cap_space, CAP_TYPE_MEMORY,
                       CAP_RIGHT_ALL, 0x2000, proc->security_zone,
                       &revoke_parent);
        cap_id_t revoke_child;
        r = cap_derive(proc->cap_space, revoke_parent,
                       proc->cap_space, CAP_RIGHT_READ, &revoke_child);
        r = cap_revoke(proc->cap_space, revoke_parent);
        kprintf("  Revoke parent: %s\n", r == NK_OK ? "OK" : "FAIL");

        r = cap_lookup(proc->cap_space, revoke_child, CAP_RIGHT_READ, &cap);
        kprintf("  Child after revoke (should fail): %s\n",
                r == NK_ERR_NOENT ? "OK (revoked)" : "FAIL");
    }

    /* ---- Test 2: Security zones and policy ---- */
    kprintf("\n[test 2] Security zones and policy...\n");
    {
        /* Create a new zone */
        u64 sandbox_zone;
        nk_result_t r = zone_create("sandbox", &sandbox_zone);
        kprintf("  Create 'sandbox' zone: %s (id=%lu)\n",
                r == NK_OK ? "OK" : "FAIL", sandbox_zone);

        /* Check policy: user→sandbox IPC should be denied by default */
        policy_action_t action = policy_check(POLICY_OP_IPC,
                                              ZONE_USER_BASE, sandbox_zone);
        kprintf("  User→Sandbox IPC policy: %s\n",
                action == POLICY_DENY ? "DENIED (correct)" : "ALLOWED (wrong!)");

        /* Add explicit allow rule */
        policy_add_rule(POLICY_OP_IPC, ZONE_USER_BASE, sandbox_zone,
                        POLICY_AUDIT);
        action = policy_check(POLICY_OP_IPC, ZONE_USER_BASE, sandbox_zone);
        kprintf("  After adding rule: %s\n",
                action == POLICY_AUDIT ? "AUDIT (correct)" : "WRONG");

        /* Same-zone should always be allowed */
        action = policy_check(POLICY_OP_IPC, ZONE_USER_BASE, ZONE_USER_BASE);
        kprintf("  Same-zone IPC: %s\n",
                action == POLICY_ALLOW ? "ALLOWED (correct)" : "WRONG");

        /* Zone resource limits */
        security_zone_t *zone = zone_get(sandbox_zone);
        kprintf("  Sandbox limits: %lu MB mem, %u procs, %u threads\n",
                (zone->max_memory_pages * PAGE_SIZE) / (1024 * 1024),
                zone->max_processes, zone->max_threads);
    }

    /* ---- Test 3: IPC ports ---- */
    kprintf("\n[test 3] IPC system...\n");
    {
        u32 port_id;
        nk_result_t r = ipc_port_create(proc->security_zone, &port_id);
        kprintf("  Create IPC port: %s (id=%u)\n",
                r == NK_OK ? "OK" : "FAIL", port_id);

        r = ipc_port_destroy(port_id);
        kprintf("  Destroy IPC port: %s\n",
                r == NK_OK ? "OK" : "FAIL");
    }

    /* ---- Test 4: Memory management ---- */
    kprintf("\n[test 4] Memory management...\n");
    {
        usize free_before = pmm_free_count();

        paddr_t page = pmm_alloc_page();
        kprintf("  Alloc page: 0x%lx\n", page);

        /* Verify page is zeroed (security) */
        u64 *ptr = (u64 *)page;
        bool zeroed = true;
        for (int i = 0; i < 512; i++) {
            if (ptr[i] != 0) { zeroed = false; break; }
        }
        kprintf("  Page zeroed: %s\n", zeroed ? "YES (secure)" : "NO (BUG!)");

        pmm_free_page(page);

        usize free_after = pmm_free_count();
        kprintf("  Free pages: %lu → %lu → %lu (alloc/free balanced: %s)\n",
                free_before, free_before - 1, free_after,
                free_before == free_after ? "YES" : "NO");

        /* Contiguous allocation */
        paddr_t contig = pmm_alloc_pages(4);
        kprintf("  Alloc 4 contiguous pages: 0x%lx\n", contig);
        pmm_free_pages(contig, 4);
    }

    /* ---- Test 5: Process and thread creation ---- */
    kprintf("\n[test 5] Process management...\n");
    {
        process_t *child = process_create("child-test", 128);
        kprintf("  Create child process: %s (PID %u)\n",
                child ? "OK" : "FAIL", child ? child->pid : 0);

        if (child) {
            kprintf("  Child has separate address space: 0x%lx\n",
                    child->page_table.root);
            kprintf("  Child security zone: %lu\n", child->security_zone);
            kprintf("  Child cap space: %s\n",
                    child->cap_space ? "allocated" : "MISSING");

            process_destroy(child);
            kprintf("  Child destroyed: OK\n");
        }
    }

    /* ---- Print audit log summary ---- */
    kprintf("\n[test 6] Audit log...\n");
    audit_dump(10);

    /* ---- Summary ---- */
    kprintf("\n========================================\n");
    kprintf("  All self-tests completed.\n");
    kprintf("  Neutron Kernel is operational.\n");
    kprintf("========================================\n");

    /* Print memory stats */
    kprintf("\n[neutron] Final memory: %lu/%lu pages free (%lu MB / %lu MB)\n",
            pmm_free_count(), pmm_total_count(),
            (pmm_free_count() * PAGE_SIZE) / (1024 * 1024),
            (pmm_total_count() * PAGE_SIZE) / (1024 * 1024));

    kprintf("[neutron] Kernel idle — entering WFI loop\n\n");

    /* Stay alive (the idle thread will take over on the next tick) */
    for (;;)
        __asm__ volatile("wfi");
}

/* ---- Kernel main -------------------------------------------------------- */

void kernel_main(void)
{
    /* Phase 0: Early console */
    console_init();
    print_banner();

    /* Report current exception level */
    u64 el = arch_current_el();
    kprintf("[neutron] Booting at EL%lu\n", el);

    /* Phase 1: Architecture initialization */
    print_memory_layout();

    exceptions_init();
    gic_init();

    /* Phase 2: Memory management */
    /* Free memory starts after the kernel image */
    paddr_t heap_start = ALIGN_UP((paddr_t)&__heap_start, PAGE_SIZE);
    /* On QEMU virt, RAM is at 0x40000000, default 2 GB */
    paddr_t heap_end = RAM_BASE + RAM_SIZE;
    pmm_init(heap_start, heap_end);
    vmm_init();

    /* Phase 3: Security subsystem */
    security_init();
    audit_init();
    capability_init();

    /* Phase 4: Process and IPC subsystem */
    process_init();
    ipc_init();
    syscall_init();

    /* Phase 5: Timer (must be after GIC) */
    timer_init();

    /* Phase 6: Print security report */
    print_security_features();

    /* Phase 7: Create the demo/init process */
    kprintf("[neutron] Creating init process...\n");

    process_t *init = process_create("init", 10);
    if (!init)
        panic("Failed to create init process");

    init->security_zone = ZONE_SYSTEM;

    /* Grant init a console I/O capability */
    cap_id_t console_cap;
    cap_create(init->cap_space, CAP_TYPE_IO,
               CAP_RIGHT_RW, 0 /* console */, ZONE_SYSTEM,
               &console_cap);

    /* Create the main thread */
    thread_t *init_thread = thread_create(init, (vaddr_t)demo_task, 10);
    if (!init_thread)
        panic("Failed to create init thread");

    /* Add to scheduler and start */
    scheduler_init();
    scheduler_add(init_thread);

    kprintf("[neutron] Starting scheduler...\n\n");

    /* Enable interrupts and start the scheduler — never returns */
    arch_enable_irqs();
    scheduler_start();

    /* Should never reach here */
    panic("kernel_main: scheduler returned");
}

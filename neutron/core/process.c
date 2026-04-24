/* ===========================================================================
 * Neutron Kernel — Process and Thread Management
 * ===========================================================================
 * Process isolation is enforced via:
 *   - Separate address spaces (page tables)
 *   - Capability spaces (no ambient authority)
 *   - Security zones (cross-zone policy enforcement)
 *   - Stack guard pages (detect overflow/underflow)
 * =========================================================================== */

#include <neutron/process.h>
#include <neutron/capability.h>
#include <neutron/security.h>
#include <neutron/console.h>
#include <neutron/string.h>

/* ---- Global process/thread tables --------------------------------------- */
static process_t proc_table[MAX_PROCESSES];
static thread_t  thread_table[MAX_THREADS];
static u32       next_pid = 1;
static u32       next_tid = 1;

/* Current running thread (per-CPU, but we're single-core for now) */
static thread_t  *current_thread;
static process_t *current_process;

/* ---- Internal helpers --------------------------------------------------- */

static process_t *alloc_process(void)
{
    for (u32 i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_STATE_FREE) {
            proc_table[i].state = PROC_STATE_CREATING;
            proc_table[i].pid = next_pid++;
            return &proc_table[i];
        }
    }
    return NULL;
}

static thread_t *alloc_thread(void)
{
    for (u32 i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].state == THREAD_STATE_FREE) {
            thread_table[i].tid = next_tid++;
            return &thread_table[i];
        }
    }
    return NULL;
}

/* ---- Kernel thread bootstrap -------------------------------------------- */

/* This function is the entry point for newly created kernel threads.
 * It's placed on the kernel stack so that arch_context_switch "returns" here.
 * x19 = entry function pointer, set up by thread_create. */
static void thread_trampoline(void)
{
    /* Re-enable interrupts for the new thread */
    __asm__ volatile("msr daifclr, #0xf");

    /* The entry point is in x19 (callee-saved, restored by context switch).
     * We can't easily access x19 from C, so we use inline asm. */
    void (*entry)(void);
    __asm__ volatile("mov %0, x19" : "=r"(entry));

    if (entry)
        entry();

    /* If the entry function returns, destroy the thread */
    kprintf("[neutron] Thread %u returned from entry — terminating\n",
            current_thread->tid);

    current_thread->state = THREAD_STATE_DEAD;
    scheduler_yield();

    /* Should not reach here */
    for (;;)
        __asm__ volatile("wfi");
}

/* ---- Public API --------------------------------------------------------- */

void process_init(void)
{
    memset(proc_table, 0, sizeof(proc_table));
    memset(thread_table, 0, sizeof(thread_table));
    current_thread = NULL;
    current_process = NULL;

    kprintf("[neutron] Process subsystem initialized (%d max procs, %d max threads)\n",
            MAX_PROCESSES, MAX_THREADS);
}

process_t *process_create(const char *name, u8 priority)
{
    process_t *proc = alloc_process();
    if (!proc) {
        kprintf("[neutron] process_create: no free process slots\n");
        return NULL;
    }

    strncpy(proc->name, name, sizeof(proc->name) - 1);
    proc->name[sizeof(proc->name) - 1] = '\0';

    /* Create address space */
    if (vmm_create_table(&proc->page_table) != NK_OK) {
        proc->state = PROC_STATE_FREE;
        return NULL;
    }

    /* Create capability space */
    proc->cap_space = (cap_space_t *)pmm_alloc_page();
    if (!proc->cap_space) {
        proc->state = PROC_STATE_FREE;
        return NULL;
    }
    memset(proc->cap_space, 0, sizeof(cap_space_t));

    proc->thread_count = 0;
    proc->exit_code = 0;
    proc->security_zone = ZONE_USER_BASE; /* Default zone */
    proc->parent = current_process;

    proc->state = PROC_STATE_READY;

    /* Audit log */
    audit_log(AUDIT_PROC_CREATE, proc->security_zone,
              proc->pid, 0, (u64)priority, 0, 0, NK_OK);

    kprintf("[neutron] Created process '%s' (PID %u, zone %lu)\n",
            proc->name, proc->pid, proc->security_zone);

    (void)priority;
    return proc;
}

void process_destroy(process_t *proc)
{
    if (!proc || proc->state == PROC_STATE_FREE)
        return;

    kprintf("[neutron] Destroying process '%s' (PID %u)\n",
            proc->name, proc->pid);

    /* Destroy all threads */
    for (u32 i = 0; i < proc->thread_count; i++) {
        if (proc->threads[i])
            thread_destroy(proc->threads[i]);
    }

    /* Free capability space */
    if (proc->cap_space) {
        pmm_free_page((paddr_t)proc->cap_space);
        proc->cap_space = NULL;
    }

    /* Free address space */
    vmm_destroy_table(&proc->page_table);

    audit_log(AUDIT_PROC_DESTROY, proc->security_zone,
              proc->pid, 0, proc->exit_code, 0, 0, NK_OK);

    proc->state = PROC_STATE_FREE;
}

thread_t *thread_create(process_t *proc, vaddr_t entry, u8 priority)
{
    if (proc->thread_count >= 8) {
        kprintf("[neutron] thread_create: process '%s' at thread limit\n",
                proc->name);
        return NULL;
    }

    thread_t *t = alloc_thread();
    if (!t)
        return NULL;

    t->proc = proc;
    t->priority = priority;
    t->state = THREAD_STATE_READY;
    t->time_slice = 10;  /* Default quantum: 10 ticks (100ms at 100Hz) */
    t->total_ticks = 0;
    t->next_ready = NULL;
    t->next_blocked = NULL;
    t->block_token = 0;

    /* Allocate kernel stack with guard page */
    usize stack_pages = THREAD_STACK_SIZE / PAGE_SIZE;
    paddr_t stack_mem = pmm_alloc_pages(stack_pages + 1); /* +1 for guard */
    if (stack_mem == 0) {
        t->state = THREAD_STATE_FREE;
        return NULL;
    }

    /* First page is guard (not mapped / will fault on access) */
    t->kernel_stack_base = stack_mem + PAGE_SIZE; /* Skip guard page */

    /* Set up the initial kernel stack frame so that arch_context_switch
     * will "return" to thread_trampoline with x19 = entry point. */
    u64 stack_top = t->kernel_stack_base + THREAD_STACK_SIZE;

    /* Simulate what arch_context_switch saves (6 stp pre-decrement):
     * Stack layout from sp (low to high):
     *   [0]=x29  [1]=x30  [2]=x27  [3]=x28  [4]=x25  [5]=x26
     *   [6]=x23  [7]=x24  [8]=x21  [9]=x22  [10]=x19 [11]=x20
     */
    u64 *frame = (u64 *)(stack_top - 96);
    memset(frame, 0, 96);

    frame[0]  = 0;                          /* x29 (frame pointer) */
    frame[1]  = (u64)thread_trampoline;     /* x30 (return address for `ret`) */
    frame[10] = (u64)entry;                 /* x19 (entry point for trampoline) */

    t->kernel_sp = (u64)frame;

    /* Register thread with process */
    proc->threads[proc->thread_count++] = t;

    kprintf("[neutron] Created thread %u in process '%s' (entry=0x%lx, prio=%u)\n",
            t->tid, proc->name, entry, priority);

    return t;
}

void thread_destroy(thread_t *thread)
{
    if (!thread || thread->state == THREAD_STATE_FREE)
        return;

    scheduler_remove(thread);

    /* Free kernel stack (including guard page) */
    if (thread->kernel_stack_base) {
        paddr_t guard_base = thread->kernel_stack_base - PAGE_SIZE;
        usize pages = THREAD_STACK_SIZE / PAGE_SIZE + 1;
        pmm_free_pages(guard_base, pages);
    }

    thread->state = THREAD_STATE_FREE;
}

process_t *process_current(void)
{
    return current_process;
}

thread_t *thread_current(void)
{
    return current_thread;
}

/* ---- Scheduler ---------------------------------------------------------- */

/* Simple priority-based round-robin scheduler.
 * Higher priority (lower number) threads run first.
 * Within same priority, round-robin with time slicing. */

static thread_t *ready_queue_head;
static thread_t *idle_thread;

/* Idle thread — runs when nothing else is ready */
static void idle_func(void)
{
    for (;;)
        __asm__ volatile("wfi");
}

void scheduler_init(void)
{
    ready_queue_head = NULL;

    /* Create a dummy process and idle thread */
    process_t *idle_proc = process_create("idle", 255);
    if (!idle_proc)
        panic("scheduler_init: cannot create idle process");
    idle_proc->security_zone = ZONE_KERNEL;

    idle_thread = thread_create(idle_proc, (vaddr_t)idle_func, 255);
    if (!idle_thread)
        panic("scheduler_init: cannot create idle thread");

    kprintf("[neutron] Scheduler initialized (priority round-robin)\n");
}

void scheduler_add(thread_t *thread)
{
    if (!thread || thread->state == THREAD_STATE_FREE)
        return;

    thread->state = THREAD_STATE_READY;
    thread->time_slice = 10;

    /* Insert sorted by priority (lower number = higher priority) */
    if (!ready_queue_head || thread->priority < ready_queue_head->priority) {
        thread->next_ready = ready_queue_head;
        ready_queue_head = thread;
        return;
    }

    thread_t *prev = ready_queue_head;
    while (prev->next_ready &&
           prev->next_ready->priority <= thread->priority) {
        prev = prev->next_ready;
    }
    thread->next_ready = prev->next_ready;
    prev->next_ready = thread;
}

void scheduler_remove(thread_t *thread)
{
    if (!thread)
        return;

    if (ready_queue_head == thread) {
        ready_queue_head = thread->next_ready;
        thread->next_ready = NULL;
        return;
    }

    thread_t *prev = ready_queue_head;
    while (prev && prev->next_ready != thread)
        prev = prev->next_ready;
    if (prev) {
        prev->next_ready = thread->next_ready;
        thread->next_ready = NULL;
    }
}

static thread_t *pick_next(void)
{
    if (ready_queue_head)
        return ready_queue_head;
    return idle_thread;
}

void scheduler_tick(void)
{
    if (!current_thread)
        return;

    current_thread->total_ticks++;

    if (current_thread->time_slice > 0)
        current_thread->time_slice--;

    /* Preempt if time slice expired */
    if (current_thread->time_slice == 0 &&
        current_thread != idle_thread) {
        scheduler_yield();
    }
}

void scheduler_yield(void)
{
    thread_t *old = current_thread;
    thread_t *next;

    if (old && old->state == THREAD_STATE_RUNNING) {
        old->state = THREAD_STATE_READY;
        scheduler_remove(old);
        scheduler_add(old);
    }

    next = pick_next();
    if (next == old)
        return;

    /* Remove from ready queue */
    scheduler_remove(next);

    next->state = THREAD_STATE_RUNNING;
    next->time_slice = 10;

    thread_t *prev_thread = current_thread;
    current_thread = next;
    current_process = next->proc;

    /* Switch address space if processes differ */
    if (!prev_thread || prev_thread->proc != next->proc) {
        vmm_switch_table(&next->proc->page_table);
    }

    /* Context switch */
    if (prev_thread) {
        arch_context_switch(&prev_thread->kernel_sp, next->kernel_sp);
    }
}

void scheduler_block(thread_t *thread, u64 token)
{
    thread->state = THREAD_STATE_BLOCKED;
    thread->block_token = token;
    scheduler_remove(thread);

    if (thread == current_thread)
        scheduler_yield();
}

void scheduler_unblock(u64 token)
{
    for (u32 i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &thread_table[i];
        if (t->state == THREAD_STATE_BLOCKED && t->block_token == token) {
            t->block_token = 0;
            scheduler_add(t);
        }
    }
}

void scheduler_start(void)
{
    thread_t *first = pick_next();
    if (!first)
        panic("scheduler_start: no threads to run");

    scheduler_remove(first);
    first->state = THREAD_STATE_RUNNING;
    current_thread = first;
    current_process = first->proc;

    vmm_switch_table(&first->proc->page_table);

    /* Jump into first thread by loading its saved context.
     * We pass NULL for old_sp since there's no previous thread. */
    u64 dummy_sp;
    arch_context_switch(&dummy_sp, first->kernel_sp);

    /* Should not return */
    panic("scheduler_start: returned from first context switch");
}

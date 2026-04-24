/* ===========================================================================
 * Neutron Kernel — Process and Thread Management
 * =========================================================================== */

#ifndef NEUTRON_PROCESS_H
#define NEUTRON_PROCESS_H

#include <neutron/types.h>
#include <neutron/memory.h>

/* ---- Process/Thread limits ---------------------------------------------- */
#define MAX_PROCESSES       64
#define MAX_THREADS         256
#define MAX_CAPS_PER_PROC   128
#define THREAD_STACK_SIZE   (16 * PAGE_SIZE)    /* 64 KB */
#define THREAD_STACK_GUARD  (1 * PAGE_SIZE)     /* 4 KB guard page */

/* ---- Process states ----------------------------------------------------- */
typedef enum {
    PROC_STATE_FREE     = 0,
    PROC_STATE_CREATING = 1,
    PROC_STATE_READY    = 2,
    PROC_STATE_RUNNING  = 3,
    PROC_STATE_BLOCKED  = 4,
    PROC_STATE_ZOMBIE   = 5,
} proc_state_t;

/* ---- Thread states ------------------------------------------------------ */
typedef enum {
    THREAD_STATE_FREE     = 0,
    THREAD_STATE_READY    = 1,
    THREAD_STATE_RUNNING  = 2,
    THREAD_STATE_BLOCKED  = 3,
    THREAD_STATE_DEAD     = 4,
} thread_state_t;

/* ---- Thread context (saved on stack during context switch) -------------- */
typedef struct {
    u64 x19, x20, x21, x22, x23, x24;
    u64 x25, x26, x27, x28, x29, x30;
    u64 sp;
} thread_ctx_t;

/* Forward declarations */
struct process;
struct capability_space;

/* ---- Thread ------------------------------------------------------------- */
typedef struct thread {
    u32             tid;
    thread_state_t  state;
    u8              priority;           /* 0 = highest, 255 = lowest */
    u64             kernel_sp;          /* Saved kernel stack pointer */
    vaddr_t         kernel_stack_base;  /* Base of kernel stack allocation */
    vaddr_t         user_stack_base;    /* Base of user stack */
    u64             time_slice;         /* Remaining ticks in current quantum */
    u64             total_ticks;        /* Total CPU ticks consumed */
    struct process  *proc;              /* Owning process */
    struct thread   *next_ready;        /* Next in scheduler ready queue */
    struct thread   *next_blocked;      /* Next in blocked queue */
    u64             block_token;        /* What this thread is waiting for */
} thread_t;

/* ---- Process ------------------------------------------------------------ */
typedef struct process {
    u32             pid;
    proc_state_t    state;
    char            name[32];
    page_table_t    page_table;         /* Process address space */
    struct capability_space *cap_space; /* Capability space */
    thread_t        *threads[8];        /* Up to 8 threads per process */
    u32             thread_count;
    u32             exit_code;
    u64             security_zone;      /* Security zone ID */
    struct process  *parent;
} process_t;

/* ---- API ---------------------------------------------------------------- */

void        process_init(void);
process_t  *process_create(const char *name, u8 priority);
void        process_destroy(process_t *proc);
thread_t   *thread_create(process_t *proc, vaddr_t entry, u8 priority);
void        thread_destroy(thread_t *thread);

process_t  *process_current(void);
thread_t   *thread_current(void);

/* Scheduler */
void        scheduler_init(void);
void        scheduler_add(thread_t *thread);
void        scheduler_remove(thread_t *thread);
void        scheduler_tick(void);
void        scheduler_yield(void);
void        scheduler_block(thread_t *thread, u64 token);
void        scheduler_unblock(u64 token);
void        scheduler_start(void) NORETURN;

/* Context switch (assembly) */
void        arch_context_switch(u64 *old_sp, u64 new_sp);
void        arch_enter_userspace(u64 entry, u64 user_sp) NORETURN;

#endif /* NEUTRON_PROCESS_H */

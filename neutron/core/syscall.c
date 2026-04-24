/* ===========================================================================
 * Neutron Kernel — System Call Interface
 * ===========================================================================
 * All userspace access to kernel services goes through system calls.
 * Unlike POSIX, every syscall requires appropriate capabilities — there
 * is no ambient authority (no "root", no UIDs, no DAC).
 *
 * Convention: SVC #0 → x8 = syscall number, x0-x5 = args, x0 = return value.
 * =========================================================================== */

#include <neutron/syscall.h>
#include <neutron/process.h>
#include <neutron/capability.h>
#include <neutron/ipc.h>
#include <neutron/memory.h>
#include <neutron/security.h>
#include <neutron/console.h>
#include <neutron/string.h>

/* Trap frame — must match vectors.S layout */
typedef struct trap_frame {
    u64 x[31];
    u64 sp_el0;
    u64 elr_el1;
    u64 spsr_el1;
} trap_frame_t;

/* Forward declarations */
u64 timer_get_timestamp_ns(void);

/* ---- Syscall statistics ------------------------------------------------- */
static u64 syscall_counts[SYS_MAX];

/* ---- Individual syscall handlers ---------------------------------------- */

static nk_result_t sys_exit(trap_frame_t *frame)
{
    u32 exit_code = (u32)frame->x[0];
    process_t *proc = process_current();

    kprintf("[neutron] Process '%s' (PID %u) exited with code %u\n",
            proc->name, proc->pid, exit_code);

    proc->exit_code = exit_code;
    process_destroy(proc);
    scheduler_yield();

    return NK_OK; /* never reached */
}

static nk_result_t sys_yield(trap_frame_t *frame)
{
    (void)frame;
    scheduler_yield();
    return NK_OK;
}

static nk_result_t sys_write(trap_frame_t *frame)
{
    /* x0 = capability ID, x1 = buffer address, x2 = length */
    cap_id_t cap_id = (cap_id_t)frame->x[0];
    const char *buf = (const char *)frame->x[1];
    usize len = (usize)frame->x[2];

    process_t *proc = process_current();
    if (!proc || !proc->cap_space)
        return NK_ERR_PERM;

    /* Look up the write capability */
    capability_t *cap;
    nk_result_t r = cap_lookup(proc->cap_space, cap_id, CAP_RIGHT_WRITE, &cap);
    if (r != NK_OK)
        return r;

    /* For now, only console writes are implemented */
    if (cap->type == CAP_TYPE_IO && cap->object == 0) {
        /* Console write */
        for (usize i = 0; i < len; i++)
            console_putc(buf[i]);
        return (nk_result_t)len;
    }

    return NK_ERR_INVAL;
}

static nk_result_t sys_read(trap_frame_t *frame)
{
    /* x0 = capability ID, x1 = buffer address, x2 = max length */
    (void)frame;
    return NK_ERR_NOSYS; /* TODO: implement */
}

static nk_result_t sys_ipc_send(trap_frame_t *frame)
{
    cap_id_t port_cap = (cap_id_t)frame->x[0];
    ipc_msg_t *msg = (ipc_msg_t *)frame->x[1];
    return ipc_send(port_cap, msg);
}

static nk_result_t sys_ipc_recv(trap_frame_t *frame)
{
    cap_id_t port_cap = (cap_id_t)frame->x[0];
    ipc_msg_t *msg = (ipc_msg_t *)frame->x[1];
    return ipc_receive(port_cap, msg);
}

static nk_result_t sys_ipc_call(trap_frame_t *frame)
{
    cap_id_t port_cap = (cap_id_t)frame->x[0];
    ipc_msg_t *send_msg = (ipc_msg_t *)frame->x[1];
    ipc_msg_t *recv_msg = (ipc_msg_t *)frame->x[2];
    return ipc_call(port_cap, send_msg, recv_msg);
}

static nk_result_t sys_cap_derive(trap_frame_t *frame)
{
    cap_id_t src_id = (cap_id_t)frame->x[0];
    cap_rights_t new_rights = (cap_rights_t)frame->x[1];
    cap_id_t *out_id = (cap_id_t *)frame->x[2];

    process_t *proc = process_current();
    if (!proc || !proc->cap_space)
        return NK_ERR_PERM;

    return cap_derive(proc->cap_space, src_id,
                      proc->cap_space, new_rights, out_id);
}

static nk_result_t sys_cap_revoke(trap_frame_t *frame)
{
    cap_id_t id = (cap_id_t)frame->x[0];

    process_t *proc = process_current();
    if (!proc || !proc->cap_space)
        return NK_ERR_PERM;

    return cap_revoke(proc->cap_space, id);
}

static nk_result_t sys_cap_delete(trap_frame_t *frame)
{
    cap_id_t id = (cap_id_t)frame->x[0];

    process_t *proc = process_current();
    if (!proc || !proc->cap_space)
        return NK_ERR_PERM;

    return cap_delete(proc->cap_space, id);
}

static nk_result_t sys_mem_map(trap_frame_t *frame)
{
    /* x0 = memory cap ID, x1 = virtual address, x2 = flags */
    (void)frame;
    return NK_ERR_NOSYS; /* TODO: implement */
}

static nk_result_t sys_mem_unmap(trap_frame_t *frame)
{
    (void)frame;
    return NK_ERR_NOSYS; /* TODO: implement */
}

static nk_result_t sys_proc_create(trap_frame_t *frame)
{
    const char *name = (const char *)frame->x[0];
    u8 priority = (u8)frame->x[1];

    process_t *proc = process_create(name, priority);
    if (!proc)
        return NK_ERR_NOMEM;

    return (nk_result_t)proc->pid;
}

static nk_result_t sys_thread_create(trap_frame_t *frame)
{
    vaddr_t entry = (vaddr_t)frame->x[0];
    u8 priority = (u8)frame->x[1];

    process_t *proc = process_current();
    if (!proc)
        return NK_ERR_PERM;

    thread_t *t = thread_create(proc, entry, priority);
    if (!t)
        return NK_ERR_NOMEM;

    scheduler_add(t);
    return (nk_result_t)t->tid;
}

static nk_result_t sys_port_create(trap_frame_t *frame)
{
    u32 *out_port_id = (u32 *)frame->x[0];

    process_t *proc = process_current();
    u64 zone = proc ? proc->security_zone : ZONE_KERNEL;

    return ipc_port_create(zone, out_port_id);
}

static nk_result_t sys_debug_log(trap_frame_t *frame)
{
    const char *msg = (const char *)frame->x[0];
    usize len = (usize)frame->x[1];

    /* Security: only print up to 256 chars */
    if (len > 256) len = 256;

    kprintf("[user] ");
    for (usize i = 0; i < len; i++)
        console_putc(msg[i]);
    console_putc('\n');

    return NK_OK;
}

static nk_result_t sys_getpid(trap_frame_t *frame)
{
    (void)frame;
    process_t *proc = process_current();
    return proc ? (nk_result_t)proc->pid : NK_ERR_PERM;
}

static nk_result_t sys_gettid(trap_frame_t *frame)
{
    (void)frame;
    thread_t *t = thread_current();
    return t ? (nk_result_t)t->tid : NK_ERR_PERM;
}

static nk_result_t sys_clock_get(trap_frame_t *frame)
{
    u64 *out_ns = (u64 *)frame->x[0];
    if (out_ns)
        *out_ns = timer_get_timestamp_ns();
    return NK_OK;
}

static nk_result_t sys_sleep(trap_frame_t *frame)
{
    (void)frame;
    /* TODO: implement sleep with timer callback */
    scheduler_yield();
    return NK_OK;
}

/* ---- Syscall dispatch table --------------------------------------------- */

typedef nk_result_t (*syscall_fn_t)(trap_frame_t *frame);

static const struct {
    syscall_fn_t handler;
    const char  *name;
} syscall_table[SYS_MAX] = {
    [SYS_EXIT]          = { (syscall_fn_t)sys_exit,         "exit" },
    [SYS_YIELD]         = { sys_yield,                      "yield" },
    [SYS_WRITE]         = { (syscall_fn_t)sys_write,        "write" },
    [SYS_READ]          = { (syscall_fn_t)sys_read,         "read" },
    [SYS_IPC_SEND]      = { (syscall_fn_t)sys_ipc_send,     "ipc_send" },
    [SYS_IPC_RECV]      = { (syscall_fn_t)sys_ipc_recv,     "ipc_recv" },
    [SYS_IPC_CALL]      = { (syscall_fn_t)sys_ipc_call,     "ipc_call" },
    [SYS_CAP_DERIVE]    = { (syscall_fn_t)sys_cap_derive,   "cap_derive" },
    [SYS_CAP_REVOKE]    = { (syscall_fn_t)sys_cap_revoke,   "cap_revoke" },
    [SYS_CAP_DELETE]    = { (syscall_fn_t)sys_cap_delete,   "cap_delete" },
    [SYS_MEM_MAP]       = { (syscall_fn_t)sys_mem_map,      "mem_map" },
    [SYS_MEM_UNMAP]     = { (syscall_fn_t)sys_mem_unmap,    "mem_unmap" },
    [SYS_PROC_CREATE]   = { (syscall_fn_t)sys_proc_create,  "proc_create" },
    [SYS_THREAD_CREATE] = { (syscall_fn_t)sys_thread_create,"thread_create" },
    [SYS_PORT_CREATE]   = { (syscall_fn_t)sys_port_create,  "port_create" },
    [SYS_DEBUG_LOG]     = { (syscall_fn_t)sys_debug_log,    "debug_log" },
    [SYS_GETPID]        = { sys_getpid,                     "getpid" },
    [SYS_GETTID]        = { sys_gettid,                     "gettid" },
    [SYS_CLOCK_GET]     = { (syscall_fn_t)sys_clock_get,    "clock_get" },
    [SYS_SLEEP]         = { (syscall_fn_t)sys_sleep,        "sleep" },
};

void syscall_init(void)
{
    memset(syscall_counts, 0, sizeof(syscall_counts));
    kprintf("[neutron] Syscall interface initialized (%d calls)\n", SYS_MAX);
}

void syscall_dispatch(trap_frame_t *frame)
{
    u64 syscall_nr = frame->x[8];

    if (syscall_nr >= SYS_MAX || !syscall_table[syscall_nr].handler) {
        kprintf("[neutron] Unknown syscall %lu from PID %u\n",
                syscall_nr,
                process_current() ? process_current()->pid : 0);
        frame->x[0] = (u64)NK_ERR_NOSYS;
        return;
    }

    syscall_counts[syscall_nr]++;

    /* Audit every syscall */
    process_t *proc = process_current();
    thread_t *t = thread_current();
    audit_log(AUDIT_SYSCALL,
              proc ? proc->security_zone : ZONE_KERNEL,
              proc ? proc->pid : 0,
              t ? t->tid : 0,
              syscall_nr, frame->x[0], frame->x[1], NK_OK);

    /* Dispatch */
    nk_result_t result = syscall_table[syscall_nr].handler(frame);
    frame->x[0] = (u64)result;
}

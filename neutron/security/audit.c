/* ===========================================================================
 * Neutron Kernel — Audit Subsystem
 * ===========================================================================
 * Ring buffer audit log that records all security-relevant kernel operations.
 * Unlike Linux's audit framework (which is bolted onto the kernel), Neutron's
 * audit is built into every subsystem from the ground up.
 *
 * Every capability operation, IPC message, process creation, policy decision,
 * and exception is recorded with nanosecond timestamps.
 * =========================================================================== */

#include <neutron/security.h>
#include <neutron/console.h>
#include <neutron/string.h>

/* Forward declaration */
u64 timer_get_timestamp_ns(void);

/* ---- Ring buffer -------------------------------------------------------- */
static audit_entry_t audit_ring[AUDIT_LOG_SIZE];
static u32 audit_head;         /* Next write position */
static u32 audit_count;        /* Total entries written */
static bool audit_overflow;    /* Ring buffer has wrapped */

/* ---- Public API --------------------------------------------------------- */

void audit_init(void)
{
    memset(audit_ring, 0, sizeof(audit_ring));
    audit_head = 0;
    audit_count = 0;
    audit_overflow = false;

    kprintf("[neutron] Audit: ring buffer initialized (%d entries)\n",
            AUDIT_LOG_SIZE);
}

void audit_log(audit_event_t event, u64 zone, u32 pid, u32 tid,
               u64 arg0, u64 arg1, u64 arg2, nk_result_t result)
{
    audit_entry_t *entry = &audit_ring[audit_head];

    entry->timestamp = timer_get_timestamp_ns();
    entry->event = event;
    entry->zone = zone;
    entry->pid = pid;
    entry->tid = tid;
    entry->arg0 = arg0;
    entry->arg1 = arg1;
    entry->arg2 = arg2;
    entry->result = result;

    audit_head = (audit_head + 1) % AUDIT_LOG_SIZE;
    audit_count++;

    if (audit_count > AUDIT_LOG_SIZE)
        audit_overflow = true;
}

usize audit_read(audit_entry_t *buf, usize max_entries)
{
    usize available;
    u32 start;

    if (audit_overflow) {
        available = AUDIT_LOG_SIZE;
        start = audit_head; /* Oldest entry is right after head */
    } else {
        available = audit_count;
        start = 0;
    }

    usize to_read = MIN(max_entries, available);

    for (usize i = 0; i < to_read; i++) {
        u32 idx = (start + i) % AUDIT_LOG_SIZE;
        buf[i] = audit_ring[idx];
    }

    return to_read;
}

static const char *event_name(audit_event_t event)
{
    switch (event) {
    case AUDIT_CAP_CREATE:    return "CAP_CREATE";
    case AUDIT_CAP_DELETE:    return "CAP_DELETE";
    case AUDIT_CAP_DERIVE:    return "CAP_DERIVE";
    case AUDIT_CAP_REVOKE:    return "CAP_REVOKE";
    case AUDIT_CAP_TRANSFER:  return "CAP_TRANSFER";
    case AUDIT_IPC_SEND:      return "IPC_SEND";
    case AUDIT_IPC_RECEIVE:   return "IPC_RECV";
    case AUDIT_PROC_CREATE:   return "PROC_CREATE";
    case AUDIT_PROC_DESTROY:  return "PROC_DESTROY";
    case AUDIT_ZONE_CREATE:   return "ZONE_CREATE";
    case AUDIT_ZONE_DESTROY:  return "ZONE_DESTROY";
    case AUDIT_POLICY_DENY:   return "POLICY_DENY";
    case AUDIT_SYSCALL:       return "SYSCALL";
    case AUDIT_EXCEPTION:     return "EXCEPTION";
    case AUDIT_MEM_MAP:       return "MEM_MAP";
    default:                  return "UNKNOWN";
    }
}

void audit_dump(usize count)
{
    usize available;

    if (audit_overflow) {
        available = AUDIT_LOG_SIZE;
    } else {
        available = audit_count;
    }

    usize to_dump = MIN(count, available);

    kprintf("\n=== Audit Log (last %lu of %u entries%s) ===\n",
            to_dump, audit_count,
            audit_overflow ? ", overflow" : "");

    /* Dump from most recent */
    for (usize i = 0; i < to_dump; i++) {
        /* Read backwards from head */
        u32 idx;
        if (audit_overflow)
            idx = (audit_head - 1 - i + AUDIT_LOG_SIZE) % AUDIT_LOG_SIZE;
        else
            idx = audit_count - 1 - i;

        audit_entry_t *e = &audit_ring[idx];

        const char *name = event_name(e->event);
        kprintf("  [%lu ms] %s zone=%lu pid=%u tid=%u "
                "args=(%lx, %lx, %lx) rc=%d\n",
                e->timestamp / 1000000,
                name,
                e->zone, e->pid, e->tid,
                e->arg0, e->arg1, e->arg2,
                e->result);
    }

    kprintf("=== End Audit Log ===\n\n");
}

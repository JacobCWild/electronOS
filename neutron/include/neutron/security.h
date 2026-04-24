/* ===========================================================================
 * Neutron Kernel — Security Subsystem
 * ===========================================================================
 * Security Zones: Hardware-enforced isolation domains unique to Neutron.
 * Each zone has its own security policy, resource limits, and audit scope.
 *
 * Zone hierarchy:
 *   - Zone 0: Kernel (highest privilege)
 *   - Zone 1: System services (drivers, init)
 *   - Zone 2-N: Application zones (user-defined isolation boundaries)
 *
 * Cross-zone operations (IPC, cap transfer) require explicit policy rules.
 * All security decisions are audit-logged.
 * =========================================================================== */

#ifndef NEUTRON_SECURITY_H
#define NEUTRON_SECURITY_H

#include <neutron/types.h>

/* ---- Security zones ----------------------------------------------------- */
#define ZONE_KERNEL     0
#define ZONE_SYSTEM     1
#define ZONE_USER_BASE  2
#define MAX_ZONES       32

typedef struct {
    u64     id;
    char    name[32];
    bool    active;

    /* Resource limits */
    usize   max_memory_pages;       /* Max physical pages */
    usize   used_memory_pages;
    u32     max_processes;
    u32     used_processes;
    u32     max_threads;
    u32     used_threads;
    u32     max_ipc_ports;
    u32     used_ipc_ports;

    /* Security policy flags */
    bool    allow_device_access;    /* Can access device I/O */
    bool    allow_irq_handling;     /* Can handle hardware interrupts */
    bool    allow_zone_create;      /* Can create sub-zones */
    bool    allow_audit_read;       /* Can read audit log */
} security_zone_t;

/* ---- Security policy rules ---------------------------------------------- */
typedef enum {
    POLICY_ALLOW    = 0,
    POLICY_DENY     = 1,
    POLICY_AUDIT    = 2,    /* Allow but audit-log the operation */
} policy_action_t;

typedef enum {
    POLICY_OP_IPC           = 0,    /* Cross-zone IPC */
    POLICY_OP_CAP_TRANSFER  = 1,    /* Capability transfer */
    POLICY_OP_MEM_MAP       = 2,    /* Cross-zone memory mapping */
    POLICY_OP_PROC_CREATE   = 3,    /* Process creation */
    POLICY_OP_DEVICE_ACCESS = 4,    /* Device I/O access */
} policy_op_t;

#define MAX_POLICY_RULES    128

typedef struct {
    bool            active;
    policy_op_t     operation;
    u64             src_zone;       /* Source zone (or U64_MAX for any) */
    u64             dst_zone;       /* Destination zone (or U64_MAX for any) */
    policy_action_t action;
} policy_rule_t;

/* ---- Audit log ---------------------------------------------------------- */
typedef enum {
    AUDIT_CAP_CREATE    = 1,
    AUDIT_CAP_DELETE    = 2,
    AUDIT_CAP_DERIVE    = 3,
    AUDIT_CAP_REVOKE    = 4,
    AUDIT_CAP_TRANSFER  = 5,
    AUDIT_IPC_SEND      = 6,
    AUDIT_IPC_RECEIVE   = 7,
    AUDIT_PROC_CREATE   = 8,
    AUDIT_PROC_DESTROY  = 9,
    AUDIT_ZONE_CREATE   = 10,
    AUDIT_ZONE_DESTROY  = 11,
    AUDIT_POLICY_DENY   = 12,
    AUDIT_SYSCALL       = 13,
    AUDIT_EXCEPTION     = 14,
    AUDIT_MEM_MAP       = 15,
} audit_event_t;

#define AUDIT_LOG_SIZE  1024    /* Ring buffer entries */

typedef struct {
    u64             timestamp;
    audit_event_t   event;
    u64             zone;           /* Zone where event occurred */
    u32             pid;
    u32             tid;
    u64             arg0;           /* Event-specific data */
    u64             arg1;
    u64             arg2;
    nk_result_t     result;         /* Operation result */
} audit_entry_t;

/* ---- API ---------------------------------------------------------------- */

/* Zone management */
void            security_init(void);
nk_result_t     zone_create(const char *name, u64 *out_id);
nk_result_t     zone_destroy(u64 zone_id);
security_zone_t *zone_get(u64 zone_id);

/* Policy engine */
void            policy_init(void);
nk_result_t     policy_add_rule(policy_op_t op, u64 src_zone, u64 dst_zone,
                                policy_action_t action);
policy_action_t policy_check(policy_op_t op, u64 src_zone, u64 dst_zone);

/* Audit log */
void            audit_init(void);
void            audit_log(audit_event_t event, u64 zone, u32 pid, u32 tid,
                          u64 arg0, u64 arg1, u64 arg2, nk_result_t result);
usize           audit_read(audit_entry_t *buf, usize max_entries);
void            audit_dump(usize count);

#endif /* NEUTRON_SECURITY_H */

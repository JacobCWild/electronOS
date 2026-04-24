/* ===========================================================================
 * Neutron Kernel — Security Policy Engine
 * ===========================================================================
 * Enforces cross-zone security policies. Every inter-zone operation
 * (IPC, capability transfer, memory mapping) is checked against the
 * policy rule table before being allowed.
 *
 * Default policy: DENY all cross-zone operations unless explicitly allowed.
 * This is the opposite of Linux's DAC model — Neutron is deny-by-default.
 * =========================================================================== */

#include <neutron/security.h>
#include <neutron/process.h>
#include <neutron/ipc.h>
#include <neutron/console.h>
#include <neutron/string.h>

/* ---- Security zones ----------------------------------------------------- */
static security_zone_t zones[MAX_ZONES];

/* ---- Policy rules ------------------------------------------------------- */
static policy_rule_t policy_rules[MAX_POLICY_RULES];
static u32 num_policy_rules;

/* ---- Zone management ---------------------------------------------------- */

void security_init(void)
{
    memset(zones, 0, sizeof(zones));
    memset(policy_rules, 0, sizeof(policy_rules));
    num_policy_rules = 0;

    /* Create built-in zones */
    zones[ZONE_KERNEL].id = ZONE_KERNEL;
    strncpy(zones[ZONE_KERNEL].name, "kernel", sizeof(zones[0].name));
    zones[ZONE_KERNEL].active = true;
    zones[ZONE_KERNEL].max_memory_pages = USIZE_MAX;
    zones[ZONE_KERNEL].max_processes = MAX_PROCESSES;
    zones[ZONE_KERNEL].max_threads = MAX_THREADS;
    zones[ZONE_KERNEL].max_ipc_ports = IPC_MAX_PORTS;
    zones[ZONE_KERNEL].allow_device_access = true;
    zones[ZONE_KERNEL].allow_irq_handling = true;
    zones[ZONE_KERNEL].allow_zone_create = true;
    zones[ZONE_KERNEL].allow_audit_read = true;

    zones[ZONE_SYSTEM].id = ZONE_SYSTEM;
    strncpy(zones[ZONE_SYSTEM].name, "system", sizeof(zones[0].name));
    zones[ZONE_SYSTEM].active = true;
    zones[ZONE_SYSTEM].max_memory_pages = 256 * 1024; /* 1 GB */
    zones[ZONE_SYSTEM].max_processes = 32;
    zones[ZONE_SYSTEM].max_threads = 128;
    zones[ZONE_SYSTEM].max_ipc_ports = 64;
    zones[ZONE_SYSTEM].allow_device_access = true;
    zones[ZONE_SYSTEM].allow_irq_handling = true;
    zones[ZONE_SYSTEM].allow_zone_create = false;
    zones[ZONE_SYSTEM].allow_audit_read = true;

    zones[ZONE_USER_BASE].id = ZONE_USER_BASE;
    strncpy(zones[ZONE_USER_BASE].name, "user", sizeof(zones[0].name));
    zones[ZONE_USER_BASE].active = true;
    zones[ZONE_USER_BASE].max_memory_pages = 128 * 1024; /* 512 MB */
    zones[ZONE_USER_BASE].max_processes = 16;
    zones[ZONE_USER_BASE].max_threads = 64;
    zones[ZONE_USER_BASE].max_ipc_ports = 32;
    zones[ZONE_USER_BASE].allow_device_access = false;
    zones[ZONE_USER_BASE].allow_irq_handling = false;
    zones[ZONE_USER_BASE].allow_zone_create = false;
    zones[ZONE_USER_BASE].allow_audit_read = false;

    /* Default policy: kernel zone can do anything */
    policy_add_rule(POLICY_OP_IPC, ZONE_KERNEL, U64_MAX, POLICY_ALLOW);
    policy_add_rule(POLICY_OP_CAP_TRANSFER, ZONE_KERNEL, U64_MAX, POLICY_ALLOW);
    policy_add_rule(POLICY_OP_MEM_MAP, ZONE_KERNEL, U64_MAX, POLICY_ALLOW);
    policy_add_rule(POLICY_OP_PROC_CREATE, ZONE_KERNEL, U64_MAX, POLICY_ALLOW);
    policy_add_rule(POLICY_OP_DEVICE_ACCESS, ZONE_KERNEL, U64_MAX, POLICY_ALLOW);

    /* System zone can IPC with user zone (audit-logged) */
    policy_add_rule(POLICY_OP_IPC, ZONE_SYSTEM, ZONE_USER_BASE, POLICY_AUDIT);
    policy_add_rule(POLICY_OP_IPC, ZONE_USER_BASE, ZONE_SYSTEM, POLICY_AUDIT);

    /* System zone can create processes */
    policy_add_rule(POLICY_OP_PROC_CREATE, ZONE_SYSTEM, ZONE_SYSTEM, POLICY_ALLOW);
    policy_add_rule(POLICY_OP_PROC_CREATE, ZONE_SYSTEM, ZONE_USER_BASE, POLICY_ALLOW);

    kprintf("[neutron] Security: %d zones, %d policy rules (deny-by-default)\n",
            3, num_policy_rules);
}

nk_result_t zone_create(const char *name, u64 *out_id)
{
    for (u32 i = ZONE_USER_BASE + 1; i < MAX_ZONES; i++) {
        if (!zones[i].active) {
            zones[i].id = i;
            strncpy(zones[i].name, name, sizeof(zones[i].name) - 1);
            zones[i].active = true;
            /* Default restrictive limits for new zones */
            zones[i].max_memory_pages = 64 * 1024;  /* 256 MB */
            zones[i].max_processes = 8;
            zones[i].max_threads = 32;
            zones[i].max_ipc_ports = 16;
            zones[i].allow_device_access = false;
            zones[i].allow_irq_handling = false;
            zones[i].allow_zone_create = false;
            zones[i].allow_audit_read = false;

            if (out_id) *out_id = i;

            audit_log(AUDIT_ZONE_CREATE, ZONE_KERNEL, 0, 0, i, 0, 0, NK_OK);
            kprintf("[neutron] Created security zone '%s' (id=%lu)\n", name, (u64)i);
            return NK_OK;
        }
    }
    return NK_ERR_NOMEM;
}

nk_result_t zone_destroy(u64 zone_id)
{
    if (zone_id <= ZONE_USER_BASE || zone_id >= MAX_ZONES)
        return NK_ERR_PERM;  /* Cannot destroy built-in zones */

    if (!zones[zone_id].active)
        return NK_ERR_NOENT;

    /* Check no active processes in this zone */
    if (zones[zone_id].used_processes > 0)
        return NK_ERR_BUSY;

    zones[zone_id].active = false;
    audit_log(AUDIT_ZONE_DESTROY, ZONE_KERNEL, 0, 0, zone_id, 0, 0, NK_OK);
    return NK_OK;
}

security_zone_t *zone_get(u64 zone_id)
{
    if (zone_id >= MAX_ZONES)
        return NULL;
    if (!zones[zone_id].active)
        return NULL;
    return &zones[zone_id];
}

/* ---- Policy engine ------------------------------------------------------ */

void policy_init(void)
{
    /* Already initialized in security_init */
}

nk_result_t policy_add_rule(policy_op_t op, u64 src_zone, u64 dst_zone,
                            policy_action_t action)
{
    if (num_policy_rules >= MAX_POLICY_RULES)
        return NK_ERR_NOMEM;

    policy_rule_t *rule = &policy_rules[num_policy_rules++];
    rule->active = true;
    rule->operation = op;
    rule->src_zone = src_zone;
    rule->dst_zone = dst_zone;
    rule->action = action;

    return NK_OK;
}

policy_action_t policy_check(policy_op_t op, u64 src_zone, u64 dst_zone)
{
    /* Same-zone operations are always allowed */
    if (src_zone == dst_zone)
        return POLICY_ALLOW;

    /* Search for a matching rule (first match wins) */
    for (u32 i = 0; i < num_policy_rules; i++) {
        policy_rule_t *rule = &policy_rules[i];
        if (!rule->active)
            continue;

        if (rule->operation != op)
            continue;

        /* Check source zone (U64_MAX = wildcard) */
        if (rule->src_zone != U64_MAX && rule->src_zone != src_zone)
            continue;

        /* Check destination zone (U64_MAX = wildcard) */
        if (rule->dst_zone != U64_MAX && rule->dst_zone != dst_zone)
            continue;

        /* Match found */
        if (rule->action == POLICY_AUDIT) {
            audit_log(AUDIT_POLICY_DENY, src_zone, 0, 0,
                      (u64)op, src_zone, dst_zone, NK_OK);
        }

        return rule->action;
    }

    /* Default: DENY */
    audit_log(AUDIT_POLICY_DENY, src_zone, 0, 0,
              (u64)op, src_zone, dst_zone, NK_ERR_PERM);

    return POLICY_DENY;
}

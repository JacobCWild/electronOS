/* ===========================================================================
 * Neutron Kernel — Capability-Based Security System
 * ===========================================================================
 * Every resource in Neutron is accessed through capabilities. A capability
 * is an unforgeable token that grants specific rights to a specific object.
 *
 * Design principles (inspired by seL4):
 *   - All kernel objects are accessed only via capabilities
 *   - Capabilities can be derived (with reduced rights)
 *   - Capabilities can be revoked (transitively)
 *   - No ambient authority — processes start with zero capabilities
 *
 * Unique to Neutron:
 *   - Capabilities are tagged with a security zone
 *   - Cross-zone capability transfer requires explicit policy approval
 *   - All capability operations are audit-logged
 * =========================================================================== */

#ifndef NEUTRON_CAPABILITY_H
#define NEUTRON_CAPABILITY_H

#include <neutron/types.h>

/* ---- Capability object types -------------------------------------------- */
typedef enum {
    CAP_TYPE_NULL       = 0,    /* Empty / invalid capability */
    CAP_TYPE_MEMORY     = 1,    /* Physical memory region */
    CAP_TYPE_THREAD     = 2,    /* Thread control */
    CAP_TYPE_PROCESS    = 3,    /* Process control */
    CAP_TYPE_IPC_PORT   = 4,    /* IPC port (endpoint) */
    CAP_TYPE_IPC_CHAN   = 5,    /* IPC channel (connected pair) */
    CAP_TYPE_IRQ        = 6,    /* Hardware interrupt */
    CAP_TYPE_IO         = 7,    /* Device I/O region */
    CAP_TYPE_AUDIT      = 8,    /* Audit log access */
    CAP_TYPE_ZONE       = 9,    /* Security zone management */
} cap_type_t;

/* ---- Capability rights (bitmask) ---------------------------------------- */
#define CAP_RIGHT_READ      BIT(0)      /* Read / receive */
#define CAP_RIGHT_WRITE     BIT(1)      /* Write / send */
#define CAP_RIGHT_EXEC      BIT(2)      /* Execute / invoke */
#define CAP_RIGHT_GRANT     BIT(3)      /* Grant (derive) to another process */
#define CAP_RIGHT_REVOKE    BIT(4)      /* Revoke derived capabilities */
#define CAP_RIGHT_MAP       BIT(5)      /* Map memory into address space */
#define CAP_RIGHT_DESTROY   BIT(6)      /* Destroy the underlying object */

#define CAP_RIGHT_ALL       (0x7F)      /* All rights */
#define CAP_RIGHT_RW        (CAP_RIGHT_READ | CAP_RIGHT_WRITE)
#define CAP_RIGHT_RX        (CAP_RIGHT_READ | CAP_RIGHT_EXEC)
#define CAP_RIGHT_RWX       (CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_EXEC)

/* ---- Capability entry --------------------------------------------------- */
typedef struct capability {
    cap_id_t        id;             /* Unique capability ID */
    cap_type_t      type;           /* Object type */
    cap_rights_t    rights;         /* Granted rights */
    u64             object;         /* Pointer/ID of the kernel object */
    u64             zone;           /* Security zone this cap belongs to */
    u64             badge;          /* Unforgeable badge (set by creator) */
    u32             generation;     /* Revocation generation counter */
    struct capability *parent;      /* Parent capability (for revocation) */
    struct capability *children;    /* First child (derived capability) */
    struct capability *sibling;     /* Next sibling in derivation tree */
} capability_t;

/* ---- Capability space (per-process) ------------------------------------- */
typedef struct capability_space {
    capability_t    caps[128];      /* Fixed-size capability table */
    u32             count;          /* Number of active capabilities */
    u64             zone;           /* Owning security zone */
} cap_space_t;

/* ---- API ---------------------------------------------------------------- */

void        capability_init(void);

/* Allocate a new capability in a capability space */
nk_result_t cap_create(cap_space_t *space, cap_type_t type,
                       cap_rights_t rights, u64 object, u64 zone,
                       cap_id_t *out_id);

/* Look up a capability by ID and verify rights */
nk_result_t cap_lookup(cap_space_t *space, cap_id_t id,
                       cap_rights_t required, capability_t **out);

/* Derive a new capability with reduced rights */
nk_result_t cap_derive(cap_space_t *src_space, cap_id_t src_id,
                       cap_space_t *dst_space, cap_rights_t new_rights,
                       cap_id_t *out_id);

/* Revoke a capability and all its descendants */
nk_result_t cap_revoke(cap_space_t *space, cap_id_t id);

/* Delete a single capability */
nk_result_t cap_delete(cap_space_t *space, cap_id_t id);

/* Transfer a capability between processes (requires GRANT right + policy) */
nk_result_t cap_transfer(cap_space_t *src, cap_id_t src_id,
                         cap_space_t *dst, cap_id_t *out_id);

/* Check if a capability has specific rights */
bool        cap_has_rights(capability_t *cap, cap_rights_t rights);

/* Print capability info (debug) */
void        cap_dump(cap_space_t *space);

#endif /* NEUTRON_CAPABILITY_H */

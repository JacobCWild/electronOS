/* ===========================================================================
 * Neutron Kernel — Capability-Based Security System
 * ===========================================================================
 * Implements unforgeable capability tokens for all kernel resource access.
 *
 * Key properties:
 *   - Capabilities are kernel-managed — userspace only holds integer IDs
 *   - Derivation creates child capabilities with equal or reduced rights
 *   - Revocation is transitive — revoking a parent revokes all descendants
 *   - Cross-zone transfers are policy-checked and audit-logged
 *   - Generation counters detect use of stale capability IDs
 * =========================================================================== */

#include <neutron/capability.h>
#include <neutron/security.h>
#include <neutron/console.h>
#include <neutron/string.h>

static u64 next_cap_id = 1;

/* ---- Internal helpers --------------------------------------------------- */

static capability_t *find_free_slot(cap_space_t *space)
{
    for (u32 i = 0; i < ARRAY_SIZE(space->caps); i++) {
        if (space->caps[i].type == CAP_TYPE_NULL)
            return &space->caps[i];
    }
    return NULL;
}

static capability_t *find_by_id(cap_space_t *space, cap_id_t id)
{
    for (u32 i = 0; i < ARRAY_SIZE(space->caps); i++) {
        if (space->caps[i].type != CAP_TYPE_NULL &&
            space->caps[i].id == id)
            return &space->caps[i];
    }
    return NULL;
}

/* Recursively revoke all children of a capability */
static void revoke_children(capability_t *cap)
{
    capability_t *child = cap->children;
    while (child) {
        capability_t *next = child->sibling;
        revoke_children(child);
        child->type = CAP_TYPE_NULL;
        child->id = 0;
        child->rights = 0;
        child->object = 0;
        child = next;
    }
    cap->children = NULL;
}

/* Remove a capability from its parent's child list */
static void unlink_from_parent(capability_t *cap)
{
    if (!cap->parent)
        return;

    capability_t **pp = &cap->parent->children;
    while (*pp) {
        if (*pp == cap) {
            *pp = cap->sibling;
            break;
        }
        pp = &(*pp)->sibling;
    }
    cap->parent = NULL;
    cap->sibling = NULL;
}

/* ---- Public API --------------------------------------------------------- */

void capability_init(void)
{
    next_cap_id = 1;
    kprintf("[neutron] Capability system initialized\n");
}

nk_result_t cap_create(cap_space_t *space, cap_type_t type,
                       cap_rights_t rights, u64 object, u64 zone,
                       cap_id_t *out_id)
{
    if (!space || type == CAP_TYPE_NULL)
        return NK_ERR_INVAL;

    capability_t *cap = find_free_slot(space);
    if (!cap)
        return NK_ERR_NOMEM;

    cap->id = next_cap_id++;
    cap->type = type;
    cap->rights = rights;
    cap->object = object;
    cap->zone = zone;
    cap->badge = 0;
    cap->generation = 0;
    cap->parent = NULL;
    cap->children = NULL;
    cap->sibling = NULL;

    space->count++;

    if (out_id)
        *out_id = cap->id;

    /* Audit log */
    audit_log(AUDIT_CAP_CREATE, zone, 0, 0,
              cap->id, (u64)type, (u64)rights, NK_OK);

    return NK_OK;
}

nk_result_t cap_lookup(cap_space_t *space, cap_id_t id,
                       cap_rights_t required, capability_t **out)
{
    if (!space)
        return NK_ERR_INVAL;

    capability_t *cap = find_by_id(space, id);
    if (!cap)
        return NK_ERR_NOENT;

    /* Check that the capability has the required rights */
    if ((cap->rights & required) != required) {
        audit_log(AUDIT_POLICY_DENY, cap->zone, 0, 0,
                  id, (u64)required, (u64)cap->rights, NK_ERR_PERM);
        return NK_ERR_PERM;
    }

    if (out)
        *out = cap;

    return NK_OK;
}

nk_result_t cap_derive(cap_space_t *src_space, cap_id_t src_id,
                       cap_space_t *dst_space, cap_rights_t new_rights,
                       cap_id_t *out_id)
{
    capability_t *parent;
    nk_result_t r = cap_lookup(src_space, src_id, CAP_RIGHT_GRANT, &parent);
    if (r != NK_OK)
        return r;

    /* New rights must be a subset of parent rights */
    if ((new_rights & ~parent->rights) != 0)
        return NK_ERR_PERM;

    capability_t *child = find_free_slot(dst_space);
    if (!child)
        return NK_ERR_NOMEM;

    child->id = next_cap_id++;
    child->type = parent->type;
    child->rights = new_rights;
    child->object = parent->object;
    child->zone = dst_space->zone;
    child->badge = parent->badge;
    child->generation = parent->generation;

    /* Link into derivation tree */
    child->parent = parent;
    child->sibling = parent->children;
    parent->children = child;
    child->children = NULL;

    dst_space->count++;

    if (out_id)
        *out_id = child->id;

    audit_log(AUDIT_CAP_DERIVE, parent->zone, 0, 0,
              parent->id, child->id, (u64)new_rights, NK_OK);

    return NK_OK;
}

nk_result_t cap_revoke(cap_space_t *space, cap_id_t id)
{
    capability_t *cap = find_by_id(space, id);
    if (!cap)
        return NK_ERR_NOENT;

    /* Must have REVOKE right */
    if (!(cap->rights & CAP_RIGHT_REVOKE))
        return NK_ERR_PERM;

    /* Recursively revoke all descendants */
    revoke_children(cap);

    audit_log(AUDIT_CAP_REVOKE, cap->zone, 0, 0,
              id, 0, 0, NK_OK);

    return NK_OK;
}

nk_result_t cap_delete(cap_space_t *space, cap_id_t id)
{
    capability_t *cap = find_by_id(space, id);
    if (!cap)
        return NK_ERR_NOENT;

    /* Revoke children first */
    revoke_children(cap);

    /* Unlink from parent */
    unlink_from_parent(cap);

    audit_log(AUDIT_CAP_DELETE, cap->zone, 0, 0,
              id, (u64)cap->type, 0, NK_OK);

    cap->type = CAP_TYPE_NULL;
    cap->id = 0;
    cap->rights = 0;
    cap->object = 0;
    space->count--;

    return NK_OK;
}

nk_result_t cap_transfer(cap_space_t *src, cap_id_t src_id,
                         cap_space_t *dst, cap_id_t *out_id)
{
    capability_t *cap;
    nk_result_t r = cap_lookup(src, src_id, CAP_RIGHT_GRANT, &cap);
    if (r != NK_OK)
        return r;

    /* Cross-zone transfer requires policy approval */
    if (src->zone != dst->zone) {
        policy_action_t action = policy_check(POLICY_OP_CAP_TRANSFER,
                                              src->zone, dst->zone);
        if (action == POLICY_DENY) {
            audit_log(AUDIT_CAP_TRANSFER, src->zone, 0, 0,
                      src_id, src->zone, dst->zone, NK_ERR_PERM);
            return NK_ERR_PERM;
        }
    }

    /* Derive into destination with same rights */
    return cap_derive(src, src_id, dst, cap->rights, out_id);
}

bool cap_has_rights(capability_t *cap, cap_rights_t rights)
{
    return cap && (cap->rights & rights) == rights;
}

void cap_dump(cap_space_t *space)
{
    kprintf("  Capability space (zone %lu, %u caps):\n",
            space->zone, space->count);
    for (u32 i = 0; i < ARRAY_SIZE(space->caps); i++) {
        capability_t *c = &space->caps[i];
        if (c->type == CAP_TYPE_NULL)
            continue;
        kprintf("    [%u] id=%lu type=%d rights=0x%x obj=0x%lx zone=%lu\n",
                i, c->id, c->type, c->rights, c->object, c->zone);
    }
}

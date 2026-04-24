/* ===========================================================================
 * Neutron Kernel — IPC (Inter-Process Communication)
 * ===========================================================================
 * Synchronous, capability-based message passing.
 *
 * IPC model:
 *   - Ports: single-receiver endpoints (like Mach ports)
 *   - Messages: fixed header + optional inline data
 *   - Capability transfer: capabilities can be sent in messages
 *   - Zero-copy: large transfers use shared memory regions (via caps)
 *
 * Unique to Neutron:
 *   - All IPC is mediated by capabilities (no ambient channels)
 *   - Cross-zone IPC requires explicit policy approval
 *   - Every IPC operation is audit-logged
 * =========================================================================== */

#ifndef NEUTRON_IPC_H
#define NEUTRON_IPC_H

#include <neutron/types.h>
#include <neutron/capability.h>

/* ---- IPC constants ------------------------------------------------------ */
#define IPC_MAX_INLINE_DATA     256     /* Max inline message bytes */
#define IPC_MAX_CAPS            4       /* Max capabilities per message */
#define IPC_MAX_PORTS           256     /* Global port limit */

/* ---- IPC message -------------------------------------------------------- */
typedef struct {
    u64         label;                          /* Message type/label */
    u64         badge;                          /* Sender's capability badge */
    u32         data_len;                       /* Inline data length */
    u32         cap_count;                      /* Number of capabilities */
    u8          data[IPC_MAX_INLINE_DATA];      /* Inline data payload */
    cap_id_t    caps[IPC_MAX_CAPS];             /* Transferred capability IDs */
} ipc_msg_t;

/* ---- IPC port ----------------------------------------------------------- */
typedef struct {
    u32         id;                     /* Port ID */
    bool        active;                 /* Port is alive */
    u64         zone;                   /* Security zone */

    /* Waiting queue */
    struct thread *waiting_sender;      /* First sender blocked on this port */
    struct thread *waiting_receiver;    /* Receiver blocked on this port */

    /* Stats */
    u64         msg_sent;
    u64         msg_received;
} ipc_port_t;

/* ---- API ---------------------------------------------------------------- */

void        ipc_init(void);

/* Port management */
nk_result_t ipc_port_create(u64 zone, u32 *out_port_id);
nk_result_t ipc_port_destroy(u32 port_id);

/* Synchronous send/receive */
nk_result_t ipc_send(cap_id_t port_cap, ipc_msg_t *msg);
nk_result_t ipc_receive(cap_id_t port_cap, ipc_msg_t *msg);
nk_result_t ipc_call(cap_id_t port_cap, ipc_msg_t *send_msg,
                     ipc_msg_t *recv_msg);

/* Non-blocking variants */
nk_result_t ipc_try_send(cap_id_t port_cap, ipc_msg_t *msg);
nk_result_t ipc_try_receive(cap_id_t port_cap, ipc_msg_t *msg);

#endif /* NEUTRON_IPC_H */

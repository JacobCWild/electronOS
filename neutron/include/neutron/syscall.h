/* ===========================================================================
 * Neutron Kernel — System Call Interface
 * ===========================================================================
 * System calls are the only way user-mode code can invoke kernel services.
 * All syscalls require appropriate capabilities — there is no ambient
 * authority. This is a key differentiator from POSIX/Linux.
 *
 * Convention: SVC #0, syscall number in x8, args in x0-x5, return in x0.
 * =========================================================================== */

#ifndef NEUTRON_SYSCALL_H
#define NEUTRON_SYSCALL_H

#include <neutron/types.h>

/* ---- System call numbers ------------------------------------------------ */
#define SYS_EXIT            0   /* Terminate current process */
#define SYS_YIELD           1   /* Yield CPU to scheduler */
#define SYS_WRITE           2   /* Write to a capability (console, file, etc.) */
#define SYS_READ            3   /* Read from a capability */
#define SYS_IPC_SEND        4   /* Send IPC message */
#define SYS_IPC_RECV        5   /* Receive IPC message */
#define SYS_IPC_CALL        6   /* Send + receive (RPC pattern) */
#define SYS_CAP_DERIVE      7   /* Derive a new capability */
#define SYS_CAP_REVOKE      8   /* Revoke a capability tree */
#define SYS_CAP_DELETE      9   /* Delete a single capability */
#define SYS_MEM_MAP         10  /* Map memory capability into address space */
#define SYS_MEM_UNMAP       11  /* Unmap memory from address space */
#define SYS_PROC_CREATE     12  /* Create a new process */
#define SYS_THREAD_CREATE   13  /* Create a new thread */
#define SYS_PORT_CREATE     14  /* Create an IPC port */
#define SYS_DEBUG_LOG       15  /* Debug: write string to kernel console */
#define SYS_GETPID          16  /* Get current process ID */
#define SYS_GETTID          17  /* Get current thread ID */
#define SYS_CLOCK_GET       18  /* Get system time (nanoseconds) */
#define SYS_SLEEP           19  /* Sleep for N nanoseconds */

#define SYS_MAX             20

/* ---- Syscall dispatch (called from exception handler) ------------------- */
struct trap_frame;
void syscall_init(void);
void syscall_dispatch(struct trap_frame *frame);

#endif /* NEUTRON_SYSCALL_H */

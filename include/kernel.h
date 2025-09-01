#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

// Function declarations for kernel initialization and management
void kernel_main(void);
void init_memory(void);
void init_interrupts(void);
void init_scheduler(void);

// Structure for process control block
typedef struct {
    uint32_t pid;          // Process ID
    uint32_t state;        // Process state
    uint32_t priority;     // Process priority
    uint32_t *stack;       // Pointer to process stack
} process_t;

// Function to create a new process
process_t* create_process(void (*function)(void), uint32_t priority);

// Function to schedule processes
void schedule(void);

// Function to handle system calls
void syscall_handler(void);

#endif // KERNEL_H
#include <stdint.h>
#include "kernel.h"

void kernel_main(void) {
    // Initialize the system
    init_uart();
    init_gpio();
    init_timer();

    // Print a welcome message
    uart_puts("Welcome to Raspberry Pi OS!\n");

    // Main loop
    while (1) {
        // Handle system calls and other kernel tasks
        // This is where the kernel would manage processes, memory, etc.
    }
}
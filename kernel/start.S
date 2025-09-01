.global _start

.section .text
_start:
    // Set up the stack pointer
    ldr sp, =0x80000  // Set stack pointer to the top of the RAM

    // Call the kernel main function
    bl kernel_main

    // Infinite loop to prevent returning to the bootloader
hang:
    b hang

.section .bss
    .space 0x1000  // Reserve space for uninitialized data (if needed)
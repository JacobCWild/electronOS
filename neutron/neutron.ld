/* ===========================================================================
 * Neutron Kernel — Linker Script (AArch64)
 * ===========================================================================
 * Memory layout for the Neutron kernel on QEMU virt machine.
 * Kernel is loaded at 0x40080000 (QEMU virt RAM starts at 0x40000000).
 * ===========================================================================
 */

ENTRY(_start)

KERNEL_BASE = 0x40080000;

SECTIONS
{
    . = KERNEL_BASE;

    /* ---- Code ----------------------------------------------------------- */
    .text : ALIGN(4096)
    {
        __text_start = .;
        *(.text.boot)           /* Boot code first */
        *(.text.vectors)        /* Exception vectors (aligned to 2048) */
        *(.text .text.*)
        __text_end = .;
    }

    /* ---- Read-only data ------------------------------------------------- */
    .rodata : ALIGN(4096)
    {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    }

    /* ---- Read-write data ------------------------------------------------ */
    .data : ALIGN(4096)
    {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    }

    /* ---- BSS (zero-initialized) ----------------------------------------- */
    .bss : ALIGN(4096)
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(4096);
    __kernel_end = .;

    /* ---- Kernel stack --------------------------------------------------- */
    . = ALIGN(16);
    __stack_bottom = .;
    . += 0x10000;               /* 64 KB kernel boot stack */
    __stack_top = .;

    /* ---- Heap / free memory starts here --------------------------------- */
    . = ALIGN(4096);
    __heap_start = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}

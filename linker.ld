ENTRY(_start)

SECTIONS
{
    . = 0x80000;   /* Kernel load address for Raspberry Pi 3 */

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(.bss*)
        *(COMMON)
    }
}
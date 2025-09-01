OUTPUT:

MEMORY
{
    RAM (rwx) : ORIGIN = 0x8000, LENGTH = 0x7F8000
}

SECTIONS
{
    . = ORIGIN(RAM);
    
    .text : {
        *(.text)
    } > RAM

    .rodata : {
        *(.rodata)
    } > RAM

    .data : {
        *(.data)
    } > RAM

    .bss : {
        *(.bss)
    } > RAM

    /DISCARD/ : {
        *(.comment)
        *(.note)
    }
}
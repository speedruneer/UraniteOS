/* linker.ld - simple 32-bit flat kernel linker */

OUTPUT_FORMAT(elf32-i386)
ENTRY(start)        /* entry point in entry.s */

SECTIONS
{
    /* start at 1MB (0x100000) for convenience, can change) */
    . = 0x9000;

    /* code section for entry + kernel */
    .text :
    {
        *(.text)       /* all .text sections from all objects */
    }

    /* read-only data */
    .rodata :
    {
        *(.rodata)
    }

    /* data section */
    .data :
    {
        *(.data)
    }

    /* BSS section */
    .bss :
    {
        *(.bss)
        *(COMMON)
    }
}

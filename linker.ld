OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)
SECTIONS {
    . = 1M;
    .text : {
        KEEP(*(.multiboot))
        *(.text*)
    }
    .rodata : { *(.rodata*) }
    .data  : { *(.data*) }
    .bss : ALIGN(16) {
       *(COMMON)
        *(.bss*)
    }
}

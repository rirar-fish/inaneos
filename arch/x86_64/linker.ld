ENTRY(_start)
SECTIONS {
    . = 1M;
    _kernel_start = .;
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
    _kernel_end = .;
}

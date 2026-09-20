# boot asm
.section .multiboot, "a"
.align 4
.long 0x1BADB002    # magic
.long 0x00000003    # flags: want mem info from bootloader
.long -(0x1BADB002 + 0x00000003)    # checksum, must sum to zero

.section .bss
.align 16
stack_bottom:
.skip 16384 # 16k stack
stack_top:
.align 4096
pml4:
    .skip 4096
pdpt:
    .skip 4096
pd:
    .skip 4096


.section .text
.global _start
_start:
    cli
    mov %eax, %ebp # multiboot magic
    mov %ebx, %edx # info address

    mov $stack_top, %esp # temporary 32 bit stack

    #clear page table
    xor %eax,%eax
    mov $pml4,%edi
    mov $3072, %ecx
    rep stosl

    # pml4[0] -> PDPT
    mov $pdpt, %eax
    or $0x3, %eax
    mov %eax, pml4

    # PDPT[0] -> PD
    mov $pd, %eax
    or $0x3, %eax
    mov %eax, pdpt

    mov $pd, %edi
    xor %eax, %eax
    xor %ecx, %ecx

1:
    mov %eax, (%edi)
    mov $0, 4(%edi)

    orl $0x83, (%edi)

    add $0x200000, %eax
    add $8, %edi

    inc %ecx
    cmp $512, %ecx
    jne 1b

    # enable PAE
    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4

    # enable long mode through EFER
    mov $0xC0000080, %ecx
    rdmsr

    or $0x100, %eax

    wrmsr

    # load PML4
    mov $pml4, %eax
    mov %eax, %cr3

    # enable paging
    mov %cr0, %eax
    or $0x80000000, %eax
    mov %eax, %cr0

    # load 64-bit GDT
    lgdt gdt64_ptr

    # Enter long mode
    ljmp $0x08, $long_mode_entry

.code64

long_mode_entry

    # Data segments
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs

    # 64-bit stack
    mov $stack_top, %rsp
    and $-16, %rsp

    # kernel_main
    mov %ebp, %edi
    mov %edx, %esi
    call kernel_main

1:
    hlt
    jmp 1b

.section .rodata

.align 8

gdt64:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF
    .quad 0x00AF92000000FFFF

gdt64_end:

gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .long gdt64

# boot to 64-bit
.code32
.section .multiboot, "a"
.align 4
.long 0x1BADB002    # magic
.long 0x00000003    # flags
.long -(0x1BADB002 + 0x00000003)    # checksum

.section .rodata
.align 8
gdt64:
.quad 0x0000000000000000
.quad 0x00209A0000000000    # code seg
.quad 0x0000920000000000    # data seg
gdt64_ptr:
.word . - gdt64 - 1
.quad gdt64

.section .bss
.align 4096
.globl pml4
pml4:
.skip 4096
pdpt:
.skip 4096
pd:
.skip 4096
.align 16
stack_bottom:
.skip 16384 # stack
stack_top:
saved_magic:
.skip 4
saved_mbi:
.skip 4

.section .text
.global _start
_start:
    cli
    movl %eax, saved_magic
    movl %ebx, saved_mbi

    movl $stack_top, %esp

    movl $pd, %edi
    movl $0x83, %eax
    movl $512, %ecx
1:
    movl %eax, (%edi)
    addl $0x200000, %eax
    addl $8, %edi
    loop 1b

    movl $pdpt, %edi
    movl $pd, %eax
    orl $0x07, %eax
    movl %eax, (%edi)

    movl $pml4, %edi
    movl $pdpt, %eax
    orl $0x07, %eax
    movl %eax, (%edi)

    movl $pml4, %eax
    movl %eax, %cr3

    movl %cr4, %eax
    orl $0x20, %eax
    movl %eax, %cr4

    movl $0xC0000080, %ecx
    rdmsr
    orl $0x100, %eax
    wrmsr

    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0

    lgdt (gdt64_ptr)
    ljmp $0x08, $long_mode

.code64
long_mode:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    movq $stack_top, %rsp
    movl saved_magic(%rip), %edi
    movl saved_mbi(%rip), %esi
    call kernel_main
    cli
1:  hlt
    jmp 1b

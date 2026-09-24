// drop to ring3
.code64
.text
.global jump_usermode
jump_usermode:
    cli
    mov $0x1B, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    push $0x1B
    push %rsi
    push $0x202
    push $0x23
    push %rdi
    iretq

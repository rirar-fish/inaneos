// syscall entry
.code64
.text
.global syscall_entry
.global saved_user_rsp
.global kstack_irq_top
syscall_entry:
    mov %rsp, saved_user_rsp(%rip)
    lea kstack_syscall_top(%rip), %rsp
    push %r15
    push %r14
    push %r13
    push %r12
    push %rbp
    push %rbx
    push %r11
    push %rcx
    push %r9
    push %r8
    push %r10
    push %rdx
    push %rsi
    push %rdi
    push %rax
    mov %rsp, %rdi
    call syscall_dispatcher
    pop %rax
    pop %rdi
    pop %rsi
    pop %rdx
    pop %r10
    pop %r8
    pop %r9
    pop %rcx
    pop %r11
    pop %rbx
    pop %rbp
    pop %r12
    pop %r13
    pop %r14
    pop %r15
    mov saved_user_rsp(%rip), %rsp
    sysretq

.section .bss
.align 16
.skip 8192
kstack_syscall_top:
.skip 8192
kstack_irq_top:
saved_user_rsp:
.skip 8

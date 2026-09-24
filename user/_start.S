// user entry
.code64
.text
.global _start
_start:
    pop %rdi
    mov %rsp, %rsi
    sub $8, %rsp
    call user_main
    mov $5, %rax
    syscall
1:
    hlt
    jmp 1b

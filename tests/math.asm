section .text
global _start
_start:
    mov rax, 60
    mov rdi, 100
    add rdi, 50
    sub rdi, 20
    syscall

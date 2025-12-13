section .text
global _start
_start:
    mov rax, 60
    mov rdi, 10
    imul rdi, 5
    add rdi, 2
    syscall

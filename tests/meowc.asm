; ==========================================
; MEOW C COMPILER (Bootstrap Stage 1 - v0.2)
; Tính năng: Arithmetic (+, -)
; Cú pháp: return 10 + 20 - 5;
; ==========================================

section .bss
    input_buf   resb 10240
    input_end   resq 1
    output_buf  resb 10240
    output_ptr  resq 1
    token_buf   resb 256
    
    ; --- BIẾN TRẠNG THÁI (STATE) ---
    is_first_num resb 1     ; 1 = Số đầu tiên (mov), 0 = Số sau (add/sub)
    pending_op   resb 1     ; Lưu dấu '+' hoặc '-' đang chờ

section .data
    ; ASM Templates
    asm_header  db "section .text", 0xA, "global _start", 0xA, "_start:", 0xA, 0
    asm_exit    db "    mov rax, 60", 0xA, 0
    asm_syscall db "    syscall", 0xA, 0
    
    asm_mov_rdi db "    mov rdi, ", 0
    asm_add_rdi db "    add rdi, ", 0
    asm_sub_rdi db "    sub rdi, ", 0
    newline     db 0xA, 0

    kw_return   db "return", 0
    msg_prompt  db "MeowC v0.2> ", 0

section .text
    global _start

; --- Helper: Copy string to output ---
emit:
    push rsi
    push rbx
    mov rsi, [output_ptr]
.copy:
    mov al, [rdi]
    cmp al, 0
    je .done
    mov [rsi], al
    inc rsi
    inc rdi
    jmp .copy
.done:
    mov [output_ptr], rsi
    pop rbx
    pop rsi
    ret

_start:
    ; 1. UI & Init
    mov rax, 1
    mov rdi, 2
    mov rsi, msg_prompt
    mov rdx, 12
    syscall

    mov r14, output_buf
    mov [output_ptr], r14
    
    mov rdi, asm_header
    call emit

    ; 2. Read Input
    mov rax, 0
    mov rdi, 0
    mov rsi, input_buf
    mov rdx, 10240
    syscall
    
    cmp rax, 0
    jle exit_prog
    
    mov r12, input_buf
    mov [input_end], r12
    add [input_end], rax

    ; Mặc định chưa có toán tử nào
    mov byte [pending_op], 0

lexer_loop:
    cmp r12, [input_end]
    jge done_compiling

    movzx rax, byte [r12]
    
    ; --- XỬ LÝ KÝ TỰ ---
    cmp al, ' '
    je next_char
    cmp al, 0xA
    je next_char
    cmp al, ';'
    je next_char
    cmp al, '{'
    je next_char
    
    cmp al, '+'
    je found_plus
    cmp al, '-'
    je found_minus
    cmp al, '}'
    je found_brace_close

    cmp al, '0'
    jl check_alpha
    cmp al, '9'
    jg check_alpha
    jmp parse_number

check_alpha:
    cmp al, 'a'
    jl next_char
    cmp al, 'z'
    jg next_char
    jmp parse_word

next_char:
    inc r12
    jmp lexer_loop

; --- XỬ LÝ TOÁN TỬ ---
found_plus:
    mov byte [pending_op], '+'
    inc r12
    jmp lexer_loop

found_minus:
    mov byte [pending_op], '-'
    inc r12
    jmp lexer_loop

; --- XỬ LÝ TỪ KHÓA (return) ---
parse_word:
    mov rdi, token_buf
    mov r13, r12
.loop:
    movzx rax, byte [r13]
    cmp al, 'a'
    jl .done
    cmp al, 'z'
    jg .done
    mov [rdi], al
    inc rdi
    inc r13
    jmp .loop
.done:
    mov byte [rdi], 0
    mov r12, r13
    
    ; Check "return"
    mov rdi, token_buf
    mov rbx, kw_return
    call strcmp
    test rax, rax
    je found_return
    jmp lexer_loop

found_return:
    ; Reset trạng thái cho biểu thức mới
    mov byte [is_first_num], 1
    mov byte [pending_op], 0    ; Clear old ops
    
    mov rdi, asm_exit           ; "mov rax, 60"
    call emit
    jmp lexer_loop

; --- XỬ LÝ SỐ & SINH CODE TÍNH TOÁN ---
parse_number:
    mov rdi, token_buf
    mov r13, r12
.loop:
    movzx rax, byte [r13]
    cmp al, '0'
    jl .done
    cmp al, '9'
    jg .done
    mov [rdi], al
    inc rdi
    inc r13
    jmp .loop
.done:
    mov byte [rdi], 0
    mov r12, r13

    ; -- LOGIC SINH CODE --
    cmp byte [is_first_num], 1
    je .emit_mov                ; Số đầu tiên -> dùng MOV

    ; Số tiếp theo -> Kiểm tra dấu
    cmp byte [pending_op], '+'
    je .emit_add
    cmp byte [pending_op], '-'
    je .emit_sub
    
    ; Mặc định (nếu không có dấu mà có số, ví dụ lỗi cú pháp) -> Kệ, cứ MOV
    jmp .emit_mov

.emit_mov:
    mov rdi, asm_mov_rdi
    call emit
    jmp .emit_val

.emit_add:
    mov rdi, asm_add_rdi
    call emit
    jmp .emit_val

.emit_sub:
    mov rdi, asm_sub_rdi
    call emit
    jmp .emit_val

.emit_val:
    mov rdi, token_buf          ; In con số ra (VD: "10")
    call emit
    mov rdi, newline
    call emit
    
    ; Đánh dấu đã xong số đầu tiên
    mov byte [is_first_num], 0
    mov byte [pending_op], 0    ; Reset dấu sau khi dùng
    jmp lexer_loop

found_brace_close:
    mov rdi, asm_syscall
    call emit
    inc r12
    jmp lexer_loop

done_compiling:
    mov rax, 1
    mov rdi, 1
    mov rsi, output_buf
    mov rdx, [output_ptr]
    sub rdx, output_buf
    syscall

exit_prog:
    mov rax, 60
    xor rdi, rdi
    syscall

strcmp:
    push rsi
    push rdi
    push rbx
.loop:
    mov al, [rdi]
    mov dl, [rbx]
    cmp al, dl
    jne .diff
    cmp al, 0
    je .equal
    inc rdi
    inc rbx
    jmp .loop
.diff:
    mov rax, 1
    jmp .done
.equal:
    xor rax, rax
.done:
    pop rbx
    pop rdi
    pop rsi
    ret
; ==========================================
; MEOW C COMPILER (Bootstrap Stage 1 - v0.3)
; Update: Hỗ trợ nhân (*), Comments (#), và tối ưu Lexer
; Cú pháp: return 10 * 5 + 2 - 3;
; ==========================================

section .bss
    input_buf   resb 20480      ; Tăng buffer lên 20KB
    input_end   resq 1
    output_buf  resb 20480
    output_ptr  resq 1
    token_buf   resb 256
    
    ; --- BIẾN TRẠNG THÁI (STATE) ---
    is_first_num resb 1     ; 1 = Số đầu (MOV), 0 = Số sau (ADD/SUB/IMUL)
    pending_op   resb 1     ; Lưu dấu (+, -, *) đang chờ

section .data
    ; ASM Templates
    asm_header  db "section .text", 0xA, "global _start", 0xA, "_start:", 0xA, 0
    asm_exit    db "    mov rax, 60", 0xA, 0
    asm_syscall db "    syscall", 0xA, 0
    
    asm_mov_rdi db "    mov rdi, ", 0
    asm_add_rdi db "    add rdi, ", 0
    asm_sub_rdi db "    sub rdi, ", 0
    asm_mul_rdi db "    imul rdi, ", 0  ; <--- [NEW] Template cho phép nhân
    newline     db 0xA, 0

    kw_return   db "return", 0
    msg_prompt  db "MeowC v0.3 (Supports +, -, *)> ", 0 

section .text
    global _start

; --- Helper: Copy string to output buffer ---
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
    mov rdx, 31
    syscall

    mov r14, output_buf
    mov [output_ptr], r14
    
    mov rdi, asm_header
    call emit

    ; 2. Read Input
    mov rax, 0
    mov rdi, 0
    mov rsi, input_buf
    mov rdx, 20480
    syscall
    
    cmp rax, 0
    jle exit_prog
    
    mov r12, input_buf
    mov [input_end], r12
    add [input_end], rax

    mov byte [pending_op], 0

lexer_loop:
    cmp r12, [input_end]
    jge done_compiling

    movzx rax, byte [r12]
    
    ; --- XỬ LÝ KÝ TỰ ĐẶC BIỆT ---
    cmp al, '#'                 ; <--- [NEW] Bỏ qua comment
    je skip_comment

    cmp al, ' '
    je next_char
    cmp al, 0x9                 ; Tab
    je next_char
    cmp al, 0xA                 ; Newline
    je next_char
    cmp al, ';'
    je next_char
    
    cmp al, '+'
    je found_plus
    cmp al, '-'
    je found_minus
    cmp al, '*'                 ; <--- [NEW] Phát hiện dấu nhân
    je found_star
    
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

; --- XỬ LÝ COMMENT ---
skip_comment:
    inc r12
    cmp r12, [input_end]
    jge done_compiling
    movzx rax, byte [r12]
    cmp al, 0xA                 ; Tìm đến hết dòng
    je next_char
    jmp skip_comment

; --- XỬ LÝ TOÁN TỬ ---
found_plus:
    mov byte [pending_op], '+'
    inc r12
    jmp lexer_loop

found_minus:
    mov byte [pending_op], '-'
    inc r12
    jmp lexer_loop

found_star:                     ; <--- [NEW] Handler cho dấu *
    mov byte [pending_op], '*'
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
    mov byte [is_first_num], 1
    mov byte [pending_op], 0
    
    mov rdi, asm_exit
    call emit
    jmp lexer_loop

; --- XỬ LÝ SỐ & SINH CODE ---
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
    je .emit_mov

    cmp byte [pending_op], '+'
    je .emit_add
    cmp byte [pending_op], '-'
    je .emit_sub
    cmp byte [pending_op], '*'  ; <--- [NEW] Check dấu nhân
    je .emit_mul
    
    jmp .emit_mov               ; Fallback an toàn

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

.emit_mul:                      ; <--- [NEW] Emit lệnh imul
    mov rdi, asm_mul_rdi
    call emit
    jmp .emit_val

.emit_val:
    mov rdi, token_buf          ; In giá trị số
    call emit
    mov rdi, newline
    call emit
    
    mov byte [is_first_num], 0
    mov byte [pending_op], 0
    jmp lexer_loop

done_compiling:
    ; Kết thúc bằng syscall exit để chương trình hợp lệ
    mov rdi, asm_syscall
    call emit

    ; Ghi ra stdout
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

; --- Helper So sánh chuỗi ---
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
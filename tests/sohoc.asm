; ==========================================
; TRÌNH THÔNG DỊCH NGÔN NGỮ "SOHOC" v3.0
; Update: Vòng lặp REPL, Phép Modulo (%), Reset Stack
; ==========================================

section .bss
    input_buffer resb 1024      ; Bộ đệm nhập liệu
    stack_mem    resb 4096      ; Ngăn xếp ảo của máy tính
    print_buffer resb 32        ; Bộ đệm in số

section .data
    prompt      db "SoHoc> ", 0       
    msg_newline db 0xA
    
    ; Thêm cái này cho chuyên nghiệp nè
    msg_welcome db "=== SOHOC v3.0 REPL Started ===", 0xA, 0

section .text
    global _start

_start:
    ; In lời chào mừng (Chỉ 1 lần đầu)
    mov rax, 1
    mov rdi, 1
    mov rsi, msg_welcome
    mov rdx, 32
    syscall

main_loop:
    ; --- BƯỚC 0: RESET TRẠNG THÁI ---
    ; Quan trọng: Mỗi vòng lặp mới phải reset con trỏ stack về đầu
    ; Nếu không stack sẽ phình to mãi -> Segfault (Stack Overflow)
    mov r13, stack_mem          ; R13: Stack Pointer (SP) reset về base

    ; --- BƯỚC 1: HIỆN DẤU NHẮC ---
    mov rax, 1                  ; sys_write
    mov rdi, 1                  ; stdout
    mov rsi, prompt
    mov rdx, 7
    syscall

    ; --- BƯỚC 2: ĐỌC DỮ LIỆU ---
    mov rax, 0                  ; sys_read
    mov rdi, 0                  ; stdin
    mov rsi, input_buffer
    mov rdx, 1024
    syscall

    ; Kiểm tra EOF (Ctrl+D) hoặc lỗi -> Thoát
    cmp rax, 0
    jle exit_program

    ; Chuẩn bị parse
    mov rsi, input_buffer       ; RSI: Instruction Pointer (IP)
    ; Lưu ý: Buffer có thể chứa cả ký tự xuống dòng từ lần nhập trước
    ; nên code parse bên dưới phải xử lý kỹ.

next_char:
    movzx rax, byte [rsi]       ; Load ký tự
    
    ; --- ĐIỀU KIỆN DỪNG CỦA MỘT DÒNG LỆNH ---
    cmp al, 0                   ; Null terminator
    je print_result
    cmp al, 0xA                 ; Newline (Enter)
    je print_result

    ; Bỏ qua Space, Tab, Carriage Return
    cmp al, ' '
    je skip_char
    cmp al, 0xD                 ; CR (Windows style line ending)
    je skip_char

    ; Kiểm tra số (0-9)
    cmp al, '0'
    jl check_operator
    cmp al, '9'
    jg check_operator

    ; ===> PARSE SỐ (Giữ nguyên logic xịn xò của v2.0) <===
    xor rbx, rbx                ; Accumulator
parse_number_loop:
    sub al, '0'
    imul rbx, rbx, 10
    add rbx, rax
    
    inc rsi
    movzx rax, byte [rsi]
    
    cmp al, '0'
    jl end_parse_number
    cmp al, '9'
    jg end_parse_number
    jmp parse_number_loop

end_parse_number:
    mov [r13], rbx              ; PUSH vào stack
    add r13, 8
    jmp next_char_no_inc        ; Xử lý tiếp ký tự hiện tại (vì nó là non-digit)

skip_char:
    inc rsi
    jmp next_char

check_operator:
    ; ===> XỬ LÝ TOÁN TỬ <===
    ; POP 2 giá trị: b (top), a (second)
    ; Stack pointer (r13) đang trỏ vào ô TRỐNG tiếp theo
    
    ; Kiểm tra Stack Underflow (Nếu r13 == stack_mem -> lỗi)
    ; (Ở đây ta tạm bỏ qua check lỗi để code gọn, nhưng thực tế cần check)

    sub r13, 8
    mov rbx, [r13]              ; Pop b (Vế phải)
    
    sub r13, 8
    mov rax, [r13]              ; Pop a (Vế trái)

    cmp byte [rsi], '+'
    je op_add
    cmp byte [rsi], '-'
    je op_sub
    cmp byte [rsi], '*'
    je op_mul
    cmp byte [rsi], '/'
    je op_div
    cmp byte [rsi], '%'         ; <--- NEW FEATURE: MODULO
    je op_mod

    ; Nếu ký tự lạ -> Bỏ qua (hoặc in lỗi)
    jmp finish_op

op_add:
    add rax, rbx
    jmp finish_op

op_sub:
    sub rax, rbx
    jmp finish_op

op_mul:
    imul rax, rbx
    jmp finish_op

op_div:
    xor rdx, rdx                ; Clear RDX (High 64-bit)
    cqo                         ; Sign extension RAX -> RDX:RAX
    idiv rbx                    ; RAX = a / b, RDX = a % b
    jmp finish_op

op_mod:
    xor rdx, rdx                ; Clear RDX
    cqo                         ; Sign extension
    idiv rbx                    ; RAX = a / b, RDX = a % b
    mov rax, rdx                ; <--- QUAN TRỌNG: Lấy RDX (Số dư) làm kết quả
    jmp finish_op

finish_op:
    mov [r13], rax              ; PUSH kết quả
    add r13, 8
    inc rsi
    jmp next_char_no_inc

next_char_no_inc:
    jmp next_char

print_result:
    ; --- IN KẾT QUẢ ---
    ; Kiểm tra stack có rỗng không (người dùng chỉ ấn Enter)
    cmp r13, stack_mem
    je end_print_loop           ; Nếu stack rỗng, không in gì cả, quay lại loop

    sub r13, 8                  ; Peek top stack
    mov rax, [r13]
    
    ; (Logic in số giữ nguyên từ v2.0 - Optimized cho gọn)
    mov rcx, print_buffer
    add rcx, 31
    mov byte [rcx], 0xA         ; Newline
    
    mov rbx, 10
    test rax, rax
    jns convert_loop
    neg rax
    push 1                      ; Flag âm
    jmp start_convert_real
    
convert_loop:
    push 0                      ; Flag dương

start_convert_real:
    xor rdx, rdx
    div rbx
    add dl, '0'
    dec rcx
    mov [rcx], dl
    test rax, rax
    jnz start_convert_real
    
    pop rdx                     ; Check flag dấu
    cmp rdx, 1
    jne do_print
    dec rcx
    mov byte [rcx], '-'

do_print:
    mov rdx, print_buffer
    add rdx, 32
    sub rdx, rcx
    
    mov rax, 1                  ; sys_write
    mov rdi, 1
    mov rsi, rcx
    syscall

end_print_loop:
    ; --- QUAY VỀ VÒNG LẶP CHÍNH (REPL) ---
    jmp main_loop

exit_program:
    mov rax, 60
    xor rdi, rdi
    syscall
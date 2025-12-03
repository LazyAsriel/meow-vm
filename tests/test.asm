; ==========================================
; TRÌNH THÔNG DỊCH NGÔN NGỮ "SOHOC" v2.0
; Tính năng: Nhập liệu, + - * /, Xử lý số đa chữ số
; Cú pháp: RPN (VD: "10 20 + 5 *" -> (10+20)*5 = 150)
; ==========================================

section .bss
    input_buffer resb 1024      ; Bộ đệm để chứa văn bản người dùng nhập
    stack_mem    resb 4096      ; Bộ nhớ cho ngăn xếp của ngôn ngữ SoHoc (chứa các số nguyên 64-bit)
    print_buffer resb 32        ; Bộ đệm tạm để in số ra màn hình

section .data
    prompt db "SoHoc> ", 0      ; Dấu nhắc lệnh
    msg_newline db 0xA          ; Xuống dòng

section .text
    global _start

_start:
    ; --- BƯỚC 1: HIỆN DẤU NHẮC VÀ NHẬP DỮ LIỆU ---
    
    ; In dấu nhắc "SoHoc> "
    mov rax, 1                  ; sys_write
    mov rdi, 1                  ; stdout
    mov rsi, prompt
    mov rdx, 7                  ; độ dài chuỗi prompt
    syscall

    ; Đọc dữ liệu từ bàn phím
    mov rax, 0                  ; sys_read
    mov rdi, 0                  ; stdin
    mov rsi, input_buffer
    mov rdx, 1024               ; Đọc tối đa 1024 byte
    syscall

    ; Kiểm tra nếu không nhập gì (EOF) thì thoát
    cmp rax, 0
    jle exit_program

    ; --- BƯỚC 2: KHỞI TẠO BỘ MÁY ---
    mov rsi, input_buffer       ; RSI: Con trỏ đọc mã nguồn
    mov r13, stack_mem          ; R13: Con trỏ đỉnh ngăn xếp (Custom Stack Pointer)

next_char:
    movzx rax, byte [rsi]       ; Đọc ký tự hiện tại
    
    ; Kiểm tra kết thúc chuỗi hoặc xuống dòng
    cmp al, 0
    je print_result
    cmp al, 0xA                 ; Newline
    je print_result

    ; Bỏ qua khoảng trắng (Space)
    cmp al, ' '
    je skip_char

    ; Kiểm tra xem có phải là số không? (0-9)
    cmp al, '0'
    jl check_operator
    cmp al, '9'
    jg check_operator

    ; ===> PARSE SỐ (Xử lý số nhiều chữ số) <===
    ; Chúng ta sẽ đọc liên tục cho đến khi gặp ký tự không phải số
    xor rbx, rbx                ; RBX sẽ chứa giá trị số đang đọc (Ban đầu = 0)
    
parse_number_loop:
    sub al, '0'                 ; Chuyển ASCII thành số (ví dụ '3' -> 3)
    imul rbx, rbx, 10           ; Nhân số cũ với 10 (dịch hàng đơn vị)
    add rbx, rax                ; Cộng số mới vào
    
    inc rsi                     ; Sang ký tự tiếp theo
    movzx rax, byte [rsi]       ; Đọc lại
    
    cmp al, '0'
    jl end_parse_number         ; Nếu không phải số nữa thì dừng
    cmp al, '9'
    jg end_parse_number
    jmp parse_number_loop

end_parse_number:
    ; Đẩy số đã parse (RBX) vào ngăn xếp
    mov [r13], rbx
    add r13, 8                  ; Tăng con trỏ stack (Mỗi số chiếm 8 byte - 64 bit)
    jmp next_char_no_inc        ; Nhảy về vòng lặp chính (không tăng RSI vì đã tăng trong loop)

skip_char:
    inc rsi
    jmp next_char

check_operator:
    ; ===> XỬ LÝ TOÁN TỬ <===
    ; Các toán tử cần 2 số từ ngăn xếp.
    ; Ta lùi r13 lại 8 byte để lấy số thứ 2 (b), rồi lùi tiếp 8 byte để lấy số thứ 1 (a).
    
    sub r13, 8
    mov rbx, [r13]              ; Lấy số b (vế phải)
    
    sub r13, 8
    mov rax, [r13]              ; Lấy số a (vế trái)

    cmp byte [rsi], '+'
    je op_add
    cmp byte [rsi], '-'
    je op_sub
    cmp byte [rsi], '*'
    je op_mul
    cmp byte [rsi], '/'
    je op_div
    
    ; Nếu ký tự lạ, bỏ qua (hoặc xử lý lỗi)
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
    xor rdx, rdx                ; Xóa RDX trước khi chia (bắt buộc cho idiv)
    cqo                         ; Mở rộng dấu của RAX sang RDX:RAX
    idiv rbx                    ; RAX = RAX / RBX
    jmp finish_op

finish_op:
    mov [r13], rax              ; Lưu kết quả lại vào ngăn xếp
    add r13, 8                  ; Tăng con trỏ stack để giữ kết quả
    inc rsi                     ; Đi tiếp sang ký tự sau toán tử
    jmp next_char_no_inc

next_char_no_inc:
    ; Helper label để quay lại vòng lặp mà không cần tăng RSI thủ công
    jmp next_char

print_result:
    ; --- BƯỚC 3: IN KẾT QUẢ (Integer to String) ---
    ; Kết quả cuối cùng nằm ở đáy stack (r13 - 8)
    sub r13, 8
    mov rax, [r13]              ; Lấy số cần in ra RAX
    
    ; Chuẩn bị buffer để in
    mov rcx, print_buffer       ; RCX trỏ tới đầu buffer
    add rcx, 31                 ; Di chuyển tới cuối buffer (đi ngược)
    mov byte [rcx], 0xA         ; Gán ký tự xuống dòng vào cuối
    
    mov rbx, 10                 ; Số chia là 10
    
    ; Kiểm tra số âm
    test rax, rax
    jns convert_loop            ; Nếu dương, nhảy tới convert
    neg rax                     ; Nếu âm, đảo dấu thành dương để chia
    push 1                      ; Đánh dấu (flag) trên stack hệ thống là có số âm
    jmp start_convert
    
convert_loop:
    push 0                      ; Đánh dấu là số dương

start_convert:
    xor rdx, rdx                ; Xóa phần dư
    div rbx                     ; RAX / 10. Thương -> RAX, Dư -> RDX
    add dl, '0'                 ; Chuyển số dư thành ASCII
    dec rcx                     ; Lùi con trỏ buffer
    mov [rcx], dl               ; Lưu ký tự
    
    test rax, rax               ; Kiểm tra thương còn không?
    jnz start_convert           ; Nếu còn, chia tiếp
    
    ; Kiểm tra dấu âm
    pop rdx                     ; Lấy flag dấu âm
    cmp rdx, 1
    jne do_print
    dec rcx
    mov byte [rcx], '-'         ; Thêm dấu trừ nếu cần

do_print:
    ; Tính độ dài chuỗi cần in
    mov rdx, print_buffer
    add rdx, 32
    sub rdx, rcx                ; Độ dài = End - Start
    
    ; Syscall in ra màn hình
    mov rax, 1                  ; sys_write
    mov rdi, 1                  ; stdout
    mov rsi, rcx                ; Con trỏ bắt đầu chuỗi số
    syscall

exit_program:
    mov rax, 60                 ; sys_exit
    xor rdi, rdi
    syscall

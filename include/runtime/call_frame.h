#pragma once
#include "common/definitions.h"

namespace meow {

struct CallFrame {
    // Con trỏ đến function/module (Metadata)
    function_t function_ = nullptr;
    // module_t module_ = nullptr;
    
    // --- POINTER MAGIC ---
    // Thay vì lưu index, ta lưu thẳng con trỏ đến vùng nhớ
    
    // regs_base_: Trỏ đến thanh ghi R0 của frame này trên Stack
    Value* regs_base_ = nullptr; 
    
    // ret_dest_: Trỏ đến nơi sẽ nhận giá trị trả về (nằm ở frame của caller)
    // Nếu là hàm void hoặc không lấy kết quả, cái này có thể là nullptr hoặc trỏ vào biến tạm
    Value* ret_dest_ = nullptr;

    // Con trỏ lệnh (Instruction Pointer)
    const uint8_t* ip_ = nullptr;

    // Bắt buộc phải có default constructor cho mảng tĩnh
    CallFrame() = default;

    CallFrame(function_t func, Value* regs, Value* ret, const uint8_t* ip)
        : function_(func), regs_base_(regs), ret_dest_(ret), ip_(ip) {
    }
};

}
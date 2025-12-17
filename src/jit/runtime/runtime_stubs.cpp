#include "meow/value.h"
#include <iostream>
#include <cmath>

// Forward declaration của các helpers nếu cần
namespace meow::jit::runtime {

    // Quy ước: v1, v2 được truyền vào dưới dạng raw uint64_t (đã encoded Nanbox)
    // dst là con trỏ tới slot trong mảng registers của VMState
    
    extern "C" void jit_helper_binary_op_generic(int op, uint64_t v1_raw, uint64_t v2_raw, uint64_t* dst) {
        // 1. Reconstruct Value from raw bits
        Value v1, v2;
        v1.set_raw(v1_raw);
        v2.set_raw(v2_raw);

        // 2. Perform Operation using Value's logic
        // (Lưu ý: Để code gọn, ta giả lập switch case. 
        // Thực tế nên gọi lại các handlers/math_ops.h của VM core)
        
        Value result = Value(nullptr); // Default null

        // Demo logic cộng đơn giản (bao gồm cả float và string nếu implement)
        if (op == 0) { // ADD
             if (v1.is_int() && v2.is_int()) {
                 result = Value(v1.as_int() + v2.as_int());
             } else if (v1.is_float() || v2.is_float()) {
                 double d1 = v1.is_int() ? (double)v1.as_int() : v1.as_float();
                 double d2 = v2.is_int() ? (double)v2.as_int() : v2.as_float();
                 result = Value(d1 + d2);
             } else {
                 // String concat? Cần VMState để allocate string mới!
                 // Hiện tại stub này chưa nhận VMState -> Cần sửa protocol call nếu muốn support GC Object.
                 // Tạm thời trả về Null hoặc báo lỗi.
                 // std::cerr << "[JIT Runtime] Unsupported operand types for ADD" << std::endl;
             }
        } 
        else if (op == 1) { // SUB
             if (v1.is_int() && v2.is_int()) {
                 result = Value(v1.as_int() - v2.as_int());
             } else {
                 // Float logic...
             }
        }
        else if (op == 2) { // MUL
             if (v1.is_int() && v2.is_int()) {
                 result = Value(v1.as_int() * v2.as_int());
             }
        }

        // 3. Write back to destination
        *dst = result.raw_tag();
    }

    extern "C" void jit_helper_get_prop_generic(uint64_t obj_raw, uint64_t name_raw, uint64_t* dst) {
        // Stub cho property access
        // Cần truy cập Object prop map... khá phức tạp nếu không có VMState.
        // Đây là lý do tại sao JIT xịn thường pass VMState* vào mọi helper.
        *dst = meow::Value(nullptr).raw_tag();
    }

} // namespace meow::jit::runtime
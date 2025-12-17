#include "meow/value.h"
#include "meow/bytecode/op_codes.h"
#include <iostream>
#include <cmath>

// Sử dụng namespace của VM để truy cập Value, OpCode
using namespace meow;

namespace meow::jit::runtime {

// Helper: Convert raw bits -> Value, thực hiện phép toán, ghi lại kết quả
extern "C" void binary_op_generic(int op, uint64_t v1_bits, uint64_t v2_bits, uint64_t* dst) {
    // 1. Reconstruct Values từ Raw Bits (Nanboxing)
    Value v1 = Value::from_raw(v1_bits);
    Value v2 = Value::from_raw(v2_bits);
    Value result;

    OpCode opcode = static_cast<OpCode>(op);

    // 2. Thực hiện logic (giống Interpreter nhưng viết gọn)
    // Lưu ý: Ở đây ta handle cả số thực (Double) và các case phức tạp khác
    try {
        if (opcode == OpCode::ADD || opcode == OpCode::ADD_B) {
            if (v1.is_int() && v2.is_int()) {
                result = Value(v1.as_int() + v2.as_int());
            } else if (v1.is_float() || v2.is_float()) {
                // Ép kiểu sang float nếu 1 trong 2 là float
                double d1 = v1.is_int() ? (double)v1.as_int() : v1.as_float();
                double d2 = v2.is_int() ? (double)v2.as_int() : v2.as_float();
                result = Value(d1 + d2);
            } else {
                // TODO: Handle String concat hoặc báo lỗi
                // Tạm thời trả về Null nếu lỗi type
                result = Value(); 
            }
        } 
        else if (opcode == OpCode::SUB || opcode == OpCode::SUB_B) {
            if (v1.is_int() && v2.is_int()) {
                result = Value(v1.as_int() - v2.as_int());
            } else {
                double d1 = v1.is_int() ? (double)v1.as_int() : v1.as_float();
                double d2 = v2.is_int() ? (double)v2.as_int() : v2.as_float();
                result = Value(d1 - d2);
            }
        }
        else if (opcode == OpCode::MUL || opcode == OpCode::MUL_B) {
            if (v1.is_int() && v2.is_int()) {
                result = Value(v1.as_int() * v2.as_int());
            } else {
                double d1 = v1.is_int() ? (double)v1.as_int() : v1.as_float();
                double d2 = v2.is_int() ? (double)v2.as_int() : v2.as_float();
                result = Value(d1 * d2);
            }
        }
    } catch (...) {
        // Chống crash nếu có exception
        result = Value();
    }

    // 3. Ghi kết quả (Raw bits) vào địa chỉ đích
    *dst = result.raw();
}

extern "C" void compare_generic(int op, uint64_t v1_bits, uint64_t v2_bits, uint64_t* dst) {
    Value v1 = Value::from_raw(v1_bits);
    Value v2 = Value::from_raw(v2_bits);
    bool res = false;
    OpCode opcode = static_cast<OpCode>(op);

    // Logic so sánh tổng quát
    // Lưu ý: Cần implement operator<, operator== chuẩn cho class Value
    // Ở đây demo logic cơ bản cho số:

    double d1 = v1.is_int() ? (double)v1.as_int() : (v1.is_float() ? v1.as_float() : 0.0);
    double d2 = v2.is_int() ? (double)v2.as_int() : (v2.is_float() ? v2.as_float() : 0.0);
    
    // Nếu không phải số, so sánh raw bits (đối với pointer/bool/null)
    bool is_numeric = (v1.is_int() || v1.is_float()) && (v2.is_int() || v2.is_float());

    switch (opcode) {
        case OpCode::EQ: case OpCode::EQ_B:
            res = (v1 == v2); // Value::operator==
            break;
        case OpCode::NEQ: case OpCode::NEQ_B:
            res = (v1 != v2);
            break;
        case OpCode::LT: case OpCode::LT_B:
            if (is_numeric) res = d1 < d2;
            else res = false; // TODO: String comparison
            break;
        case OpCode::LE: case OpCode::LE_B:
            if (is_numeric) res = d1 <= d2;
            else res = false;
            break;
        case OpCode::GT: case OpCode::GT_B:
            if (is_numeric) res = d1 > d2;
            else res = false;
            break;
        case OpCode::GE: case OpCode::GE_B:
            if (is_numeric) res = d1 >= d2;
            else res = false;
            break;
        default: break;
    }

    // Kết quả trả về là Value(bool)
    Value result_val(res);
    *dst = result_val.raw();
}

} // namespace meow::jit::runtime
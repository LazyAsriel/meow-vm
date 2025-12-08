#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h" // Để gọi impl_PANIC khi lỗi

namespace meow {
namespace handlers {

    // --- ADD ---
    [[gnu::noinline]]
    static const uint8_t* impl_ADD(const uint8_t* ip, VMState* state) {
        uint16_t dst = read_u16(ip);
        uint16_t r1  = read_u16(ip);
        uint16_t r2  = read_u16(ip);

        Value& left  = state->reg(r1);
        Value& right = state->reg(r2);

        if (left.is_int() && right.is_int()) {
            state->reg(dst) = Value(left.as_int() + right.as_int());
        } 
        else if (left.is_float() && right.is_float()) {
            state->reg(dst) = Value(left.as_float() + right.as_float());
        }
        else {
            auto func = OperatorDispatcher::find(OpCode::ADD, left, right);
            if (func) {
                state->reg(dst) = func(&state->heap, left, right);
            } else {
                state->error("Toán hạng không hợp lệ cho phép cộng (+)");
                return impl_PANIC(ip, state);
            }
        }
        return ip;
    }

    // --- LESS THAN (<) ---
    [[gnu::noinline]]
    static const uint8_t* impl_LT(const uint8_t* ip, VMState* state) {
        uint16_t dst = read_u16(ip);
        uint16_t r1  = read_u16(ip);
        uint16_t r2  = read_u16(ip);

        Value& left  = state->reg(r1);
        Value& right = state->reg(r2);

        if (left.is_int() && right.is_int()) {
            state->reg(dst) = Value(left.as_int() < right.as_int());
        } else {
            auto func = OperatorDispatcher::find(OpCode::LT, left, right);
            if (func) {
                state->reg(dst) = func(&state->heap, left, right);
            } else {
                state->error("Toán hạng không hợp lệ cho phép so sánh (<)");
                return impl_PANIC(ip, state);
            }
        }
        return ip;
    }

} // namespace handlers
} // namespace meow
#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h" // Để dùng impl_PANIC

namespace meow::handlers {

// Macro sinh code cho toán tử 2 ngôi (Binary Op)
// Tự động tìm handler trong OperatorDispatcher, nếu không có thì Panic
#define BINARY_OP_IMPL(NAME, OP_CODE) \
    [[always_inline, gnu::hot]] static const uint8_t* impl_##NAME(const uint8_t* ip, VMState* state) { \
        uint16_t dst = read_u16(ip); \
        uint16_t r1  = read_u16(ip); \
        uint16_t r2  = read_u16(ip); \
        auto& left  = state->reg(r1); \
        auto& right = state->reg(r2); \
        if (auto func = OperatorDispatcher::find(OpCode::OP_CODE, left, right)) { \
            state->reg(dst) = func(&state->heap, left, right); \
        } else { \
            state->error("Toán tử không hỗ trợ kiểu dữ liệu này (" #NAME ")"); \
            return impl_PANIC(ip, state); \
        } \
        return ip; \
    }

// ADD có tối ưu riêng cho Int/Float để chạy nhanh hơn Dispatcher
[[always_inline, gnu::hot]] static const uint8_t* impl_ADD(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t r1 = read_u16(ip);
    uint16_t r2 = read_u16(ip);
    auto& left = state->reg(r1);
    auto& right = state->reg(r2);

    if (left.is_int() && right.is_int()) {
        state->reg(dst) = Value(left.as_int() + right.as_int());
    } else if (left.is_float() && right.is_float()) {
        state->reg(dst) = Value(left.as_float() + right.as_float());
    } else {
        if (auto func = OperatorDispatcher::find(OpCode::ADD, left, right)) {
            state->reg(dst) = func(&state->heap, left, right);
        } else {
            state->error("Toán hạng không hợp lệ cho phép cộng (ADD)");
            return impl_PANIC(ip, state);
        }
    }
    return ip;
}

// Sinh code cho các toán tử còn lại
BINARY_OP_IMPL(SUB, SUB)
BINARY_OP_IMPL(MUL, MUL)
BINARY_OP_IMPL(DIV, DIV)
BINARY_OP_IMPL(MOD, MOD)
BINARY_OP_IMPL(POW, POW)

// So sánh
BINARY_OP_IMPL(EQ, EQ)
BINARY_OP_IMPL(NEQ, NEQ)
BINARY_OP_IMPL(GT, GT)
BINARY_OP_IMPL(GE, GE)
BINARY_OP_IMPL(LT, LT)
BINARY_OP_IMPL(LE, LE)

// Bitwise
BINARY_OP_IMPL(BIT_AND, BIT_AND)
BINARY_OP_IMPL(BIT_OR, BIT_OR)
BINARY_OP_IMPL(BIT_XOR, BIT_XOR)
BINARY_OP_IMPL(LSHIFT, LSHIFT)
BINARY_OP_IMPL(RSHIFT, RSHIFT)

// --- Unary Ops (1 ngôi) ---

[[always_inline]] static const uint8_t* impl_NEG(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src = read_u16(ip);
    if (auto func = OperatorDispatcher::find(OpCode::NEG, state->reg(src))) {
        state->reg(dst) = func(&state->heap, state->reg(src));
    } else {
        state->error("Không thể phủ định giá trị này (NEG)");
        return impl_PANIC(ip, state);
    }
    return ip;
}

[[always_inline]] static const uint8_t* impl_NOT(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src = read_u16(ip);
    state->reg(dst) = Value(!to_bool(state->reg(src)));
    return ip;
}

[[always_inline]] static const uint8_t* impl_BIT_NOT(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src = read_u16(ip);
    if (auto func = OperatorDispatcher::find(OpCode::BIT_NOT, state->reg(src))) {
        state->reg(dst) = func(&state->heap, state->reg(src));
    } else {
        state->error("Không thể BIT_NOT giá trị này");
        return impl_PANIC(ip, state);
    }
    return ip;
}

#undef BINARY_OP_IMPL
} // namespace meow::handlers
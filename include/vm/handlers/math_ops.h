#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {

// Macro tối ưu: Gọi thẳng, không kiểm tra nullptr (vì OperatorDispatcher đảm bảo luôn trả về function)
#define BINARY_OP_IMPL(NAME, OP_CODE) \
    HOT_HANDLER impl_##NAME(const uint8_t* ip, VMState* state) { \
        uint16_t dst = read_u16(ip); \
        uint16_t r1  = read_u16(ip); \
        uint16_t r2  = read_u16(ip); \
        /* Truy cập trực tiếp thanh ghi, compiler sẽ tối ưu pointer arithmetic */ \
        auto& left  = state->reg(r1); \
        auto& right = state->reg(r2); \
        /* GỌI THẲNG: Không if, không check. Tin tưởng tuyệt đối vào Dispatcher */ \
        state->reg(dst) = OperatorDispatcher::find(OpCode::OP_CODE, left, right)(&state->heap, left, right); \
        return ip; \
    }

// --- ADD (Siêu tối ưu) ---
// Giữ lại fast-path cho Int/Int và Float/Float vì nó nhanh hơn function call
HOT_HANDLER impl_ADD(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t r1 = read_u16(ip);
    uint16_t r2 = read_u16(ip);
    
    // Prefetch dữ liệu nếu cần (tùy architecture, nhưng để compiler lo)
    auto& left = state->reg(r1);
    auto& right = state->reg(r2);

    // Fast Path: Int + Int (Chiếm 90% trường hợp loop)
    if (left.is_int() && right.is_int()) [[likely]] {
        state->reg(dst) = Value(left.as_int() + right.as_int());
    } 
    // Fast Path: Float + Float
    else if (left.is_float() && right.is_float()) {
        state->reg(dst) = Value(left.as_float() + right.as_float());
    } 
    else {
        // Slow Path: Dispatch trực tiếp
        // Lưu ý: Nếu kiểu không hợp lệ, hàm trap của Dispatcher sẽ chạy.
        // Cần đảm bảo hàm trap đó ném VMError nếu muốn dừng VM, hoặc trả null nếu muốn lờ đi.
        state->reg(dst) = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    }
    return ip;
}

// --- LT (So sánh nhỏ hơn) ---
HOT_HANDLER impl_LT(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t r1 = read_u16(ip);
    uint16_t r2 = read_u16(ip);
    auto& left = state->reg(r1);
    auto& right = state->reg(r2);

    if (left.is_int() && right.is_int()) [[likely]] {
        state->reg(dst) = Value(left.as_int() < right.as_int());
    } else {
        // Dispatch trực tiếp
        state->reg(dst) = OperatorDispatcher::find(OpCode::LT, left, right)(&state->heap, left, right);
    }
    return ip;
}

// Sinh code cho các toán tử còn lại
BINARY_OP_IMPL(SUB, SUB)
BINARY_OP_IMPL(MUL, MUL)
BINARY_OP_IMPL(DIV, DIV)
BINARY_OP_IMPL(MOD, MOD)
BINARY_OP_IMPL(POW, POW)

BINARY_OP_IMPL(EQ, EQ)
BINARY_OP_IMPL(NEQ, NEQ)
BINARY_OP_IMPL(GT, GT)
BINARY_OP_IMPL(GE, GE)
BINARY_OP_IMPL(LE, LE) // LT đã viết tay ở trên

BINARY_OP_IMPL(BIT_AND, BIT_AND)
BINARY_OP_IMPL(BIT_OR, BIT_OR)
BINARY_OP_IMPL(BIT_XOR, BIT_XOR)
BINARY_OP_IMPL(LSHIFT, LSHIFT)
BINARY_OP_IMPL(RSHIFT, RSHIFT)

// --- Unary Ops ---
// NEG và BIT_NOT ít khi có fastpath đơn giản hơn dispatch table, nên gọi thẳng luôn cho gọn
HOT_HANDLER impl_NEG(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src = read_u16(ip);
    auto& val = state->reg(src);
    state->reg(dst) = OperatorDispatcher::find(OpCode::NEG, val)(&state->heap, val);
    return ip;
}

HOT_HANDLER impl_BIT_NOT(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src = read_u16(ip);
    auto& val = state->reg(src);
    state->reg(dst) = OperatorDispatcher::find(OpCode::BIT_NOT, val)(&state->heap, val);
    return ip;
}

HOT_HANDLER impl_NOT(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src = read_u16(ip);
    // NOT logic: không cần dispatcher vì mọi value đều to_bool được
    state->reg(dst) = Value(!to_bool(state->reg(src)));
    return ip;
}

#undef BINARY_OP_IMPL
#undef HOT_HANDLER // Định nghĩa trong utils hoặc flow_ops nếu cần
}
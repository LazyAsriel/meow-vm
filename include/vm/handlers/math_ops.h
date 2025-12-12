#pragma once
#include "vm/handlers/utils.h"

namespace meow::handlers {

// --- Cấu trúc giải mã (Bulk Decoding) ---

// 1. Binary Ops (dst, r1, r2)
// Phiên bản chuẩn: 6 bytes (3 x u16)
struct BinaryArgs { 
    uint16_t dst; 
    uint16_t r1; 
    uint16_t r2; 
} __attribute__((packed)); // <--- Thêm cái này

// Tương tự cho bản nén (nếu có dùng)
struct BinaryArgsB { 
    uint8_t dst; 
    uint8_t r1; 
    uint8_t r2; 
} __attribute__((packed)); // <--- Và cái này
// 2. Unary Ops (dst, src)
struct UnaryArgs { 
    uint16_t dst; 
    uint16_t src; 
};

// --- MACROS ---

// Macro cho phiên bản chuẩn (u16)
#define BINARY_OP_IMPL(NAME, OP_ENUM) \
    HOT_HANDLER impl_##NAME(const uint8_t* ip, Value* regs, Value* constants, VMState* state) { \
        const auto& args = *reinterpret_cast<const BinaryArgs*>(ip); \
        Value& left  = regs[args.r1]; \
        Value& right = regs[args.r2]; \
        regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
        return ip + sizeof(BinaryArgs); \
    }

// Macro cho phiên bản nén (u8)
#define BINARY_OP_B_IMPL(NAME, OP_ENUM) \
    HOT_HANDLER impl_##NAME##_B(const uint8_t* ip, Value* regs, Value* constants, VMState* state) { \
        const auto& args = *reinterpret_cast<const BinaryArgsB*>(ip); \
        Value& left  = regs[args.r1]; \
        Value& right = regs[args.r2]; \
        regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
        return ip + sizeof(BinaryArgsB); \
    }

// --- ADD (Hot Path) ---

HOT_HANDLER impl_ADD(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const BinaryArgs*>(ip);
    
    Value& left = regs[args.r1];
    Value& right = regs[args.r2];

    if (left.is_int() && right.is_int()) [[likely]] {
        regs[args.dst] = Value(left.as_int() + right.as_int());
    } else if (left.is_float() && right.is_float()) {
        regs[args.dst] = Value(left.as_float() + right.as_float());
    } else {
        regs[args.dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    }
    return ip + sizeof(BinaryArgs);
}

HOT_HANDLER impl_ADD_B(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const BinaryArgsB*>(ip);
    
    Value& left = regs[args.r1];
    Value& right = regs[args.r2];

    if (left.is_int() && right.is_int()) [[likely]] {
        regs[args.dst] = Value(left.as_int() + right.as_int());
    } else if (left.is_float() && right.is_float()) {
        regs[args.dst] = Value(left.as_float() + right.as_float());
    } else {
        regs[args.dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    }
    return ip + sizeof(BinaryArgsB);
}

// --- LT (Hot Path) ---

HOT_HANDLER impl_LT(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const BinaryArgs*>(ip);
    
    Value& left = regs[args.r1];
    Value& right = regs[args.r2];

    if (left.is_int() && right.is_int()) [[likely]] {
        regs[args.dst] = Value(left.as_int() < right.as_int());
    } else {
        regs[args.dst] = OperatorDispatcher::find(OpCode::LT, left, right)(&state->heap, left, right);
    }
    return ip + sizeof(BinaryArgs);
}

HOT_HANDLER impl_LT_B(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const BinaryArgsB*>(ip);
    
    Value& left = regs[args.r1];
    Value& right = regs[args.r2];

    if (left.is_int() && right.is_int()) [[likely]] {
        regs[args.dst] = Value(left.as_int() < right.as_int());
    } else {
        regs[args.dst] = OperatorDispatcher::find(OpCode::LT, left, right)(&state->heap, left, right);
    }
    return ip + sizeof(BinaryArgsB);
}

// --- Các phép toán khác ---

// Chuẩn (u16)
BINARY_OP_IMPL(SUB, SUB)
BINARY_OP_IMPL(MUL, MUL)
BINARY_OP_IMPL(DIV, DIV)
BINARY_OP_IMPL(MOD, MOD)
BINARY_OP_IMPL(POW, POW)
BINARY_OP_IMPL(EQ, EQ)
BINARY_OP_IMPL(NEQ, NEQ)
BINARY_OP_IMPL(GT, GT)
BINARY_OP_IMPL(GE, GE)
BINARY_OP_IMPL(LE, LE)
BINARY_OP_IMPL(BIT_AND, BIT_AND)
BINARY_OP_IMPL(BIT_OR, BIT_OR)
BINARY_OP_IMPL(BIT_XOR, BIT_XOR)
BINARY_OP_IMPL(LSHIFT, LSHIFT)
BINARY_OP_IMPL(RSHIFT, RSHIFT)

// Nén (u8)
BINARY_OP_B_IMPL(SUB, SUB)
BINARY_OP_B_IMPL(MUL, MUL)
BINARY_OP_B_IMPL(DIV, DIV)
BINARY_OP_B_IMPL(MOD, MOD)
// Bạn có thể thêm các phiên bản _B cho EQ, NEQ... nếu muốn

// --- Unary Ops (Hiện tại giữ u16 vì ít dùng trong hot loop hơn binary) ---

HOT_HANDLER impl_NEG(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
    Value& val = regs[args.src];
    regs[args.dst] = OperatorDispatcher::find(OpCode::NEG, val)(&state->heap, val);
    return ip + sizeof(UnaryArgs);
}

HOT_HANDLER impl_BIT_NOT(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
    Value& val = regs[args.src];
    regs[args.dst] = OperatorDispatcher::find(OpCode::BIT_NOT, val)(&state->heap, val);
    return ip + sizeof(UnaryArgs);
}

HOT_HANDLER impl_NOT(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
    regs[args.dst] = Value(!to_bool(regs[args.src]));
    return ip + sizeof(UnaryArgs);
}

#undef BINARY_OP_IMPL
#undef BINARY_OP_B_IMPL

} // namespace meow::handlers
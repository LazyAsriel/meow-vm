#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {

// --- MACROS (Updated for decode::args) ---

// Binary Op (Standard 16-bit regs) -> Tổng 6 bytes -> decode đọc u64
#define BINARY_OP_IMPL(NAME, OP_ENUM) \
    HOT_HANDLER impl_##NAME(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
        auto [dst, r1, r2] = decode::args<u16, u16, u16>(ip); \
        Value& left  = regs[r1]; \
        Value& right = regs[r2]; \
        regs[dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
        return ip; \
    }

// Binary Op (Byte 8-bit regs) -> Tổng 3 bytes -> decode đọc u32
#define BINARY_OP_B_IMPL(NAME, OP_ENUM) \
    HOT_HANDLER impl_##NAME##_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
        auto [dst, r1, r2] = decode::args<u8, u8, u8>(ip); \
        Value& left  = regs[r1]; \
        Value& right = regs[r2]; \
        regs[dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
        return ip; \
    }

// --- ADD (Specialized Arithmetic) ---

HOT_HANDLER impl_ADD(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 3 * u16 = 6 bytes. Decoder tự động dùng u64 load.
    auto [dst, r1, r2] = decode::args<u16, u16, u16>(ip);
    
    Value& left = regs[r1];
    Value& right = regs[r2];
    
    if (left.holds_both<int_t>(right)) [[likely]] {
        regs[dst] = left.as_int() + right.as_int();
    }
    else if (left.holds_both<float_t>(right)) {
        regs[dst] = Value(left.as_float() + right.as_float());
    }
    else [[unlikely]] {
        regs[dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    }
    return ip;
}

HOT_HANDLER impl_ADD_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 3 * u8 = 3 bytes. Decoder tự động dùng u32 load.
    auto [dst, r1, r2] = decode::args<u8, u8, u8>(ip);
    
    Value& left = regs[r1];
    Value& right = regs[r2];
    
    if (left.holds_both<int_t>(right)) [[likely]] {
        regs[dst] = left.as_int() + right.as_int();
    }
    else if (left.holds_both<float_t>(right)) {
        regs[dst] = Value(left.as_float() + right.as_float());
    }
    else [[unlikely]] {
        regs[dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    }
    return ip;
}

// --- Arithmetic Ops ---
BINARY_OP_IMPL(SUB, SUB)
BINARY_OP_IMPL(MUL, MUL)
BINARY_OP_IMPL(DIV, DIV)
BINARY_OP_IMPL(MOD, MOD)
BINARY_OP_IMPL(POW, POW)

BINARY_OP_B_IMPL(SUB, SUB)
BINARY_OP_B_IMPL(MUL, MUL)
BINARY_OP_B_IMPL(DIV, DIV)
BINARY_OP_B_IMPL(MOD, MOD)

// --- Bitwise Ops ---
BINARY_OP_IMPL(BIT_AND, BIT_AND)
BINARY_OP_IMPL(BIT_OR, BIT_OR)
BINARY_OP_IMPL(BIT_XOR, BIT_XOR)
BINARY_OP_IMPL(LSHIFT, LSHIFT)
BINARY_OP_IMPL(RSHIFT, RSHIFT)

BINARY_OP_B_IMPL(BIT_AND, BIT_AND)
BINARY_OP_B_IMPL(BIT_OR, BIT_OR)
BINARY_OP_B_IMPL(BIT_XOR, BIT_XOR)
BINARY_OP_B_IMPL(LSHIFT, LSHIFT)
BINARY_OP_B_IMPL(RSHIFT, RSHIFT)

// --- Comparison Ops ---

#define CMP_FAST_IMPL(OP_NAME, OP_ENUM, OPERATOR) \
    HOT_HANDLER impl_##OP_NAME(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
        auto [dst, r1, r2] = decode::args<u16, u16, u16>(ip); \
        Value& left = regs[r1]; \
        Value& right = regs[r2]; \
        if (left.holds_both<int_t>(right)) [[likely]] { \
            regs[dst] = Value(left.as_int() OPERATOR right.as_int()); \
        } else [[unlikely]] { \
            regs[dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
        } \
        return ip; \
    } \
    HOT_HANDLER impl_##OP_NAME##_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
        auto [dst, r1, r2] = decode::args<u8, u8, u8>(ip); \
        Value& left = regs[r1]; \
        Value& right = regs[r2]; \
        if (left.holds_both<int_t>(right)) [[likely]] { \
            regs[dst] = Value(left.as_int() OPERATOR right.as_int()); \
        } else [[unlikely]] { \
            regs[dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
        } \
        return ip; \
    }

CMP_FAST_IMPL(EQ, EQ, ==)
CMP_FAST_IMPL(NEQ, NEQ, !=)
CMP_FAST_IMPL(GT, GT, >)
CMP_FAST_IMPL(GE, GE, >=)
CMP_FAST_IMPL(LT, LT, <)
CMP_FAST_IMPL(LE, LE, <=)

// --- Unary Ops ---

// NEG
HOT_HANDLER impl_NEG(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 2 * u16 = 4 bytes -> Load u32
    auto [dst, src] = decode::args<u16, u16>(ip);
    
    Value& val = regs[src];
    if (val.is_int()) [[likely]] regs[dst] = Value(-val.as_int());
    else if (val.is_float()) regs[dst] = Value(-val.as_float());
    else [[unlikely]] regs[dst] = OperatorDispatcher::find(OpCode::NEG, val)(&state->heap, val);
    return ip;
}

HOT_HANDLER impl_NEG_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 2 * u8 = 2 bytes -> Load u16
    auto [dst, src] = decode::args<u8, u8>(ip);
    
    Value& val = regs[src];
    if (val.is_int()) [[likely]] regs[dst] = Value(-val.as_int());
    else if (val.is_float()) regs[dst] = Value(-val.as_float());
    else [[unlikely]] regs[dst] = OperatorDispatcher::find(OpCode::NEG, val)(&state->heap, val);
    return ip;
}

// NOT (!)
HOT_HANDLER impl_NOT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst, src] = decode::args<u16, u16>(ip);
    Value& val = regs[src];
    if (val.is_bool()) [[likely]] regs[dst] = Value(!val.as_bool());
    else if (val.is_int()) regs[dst] = Value(val.as_int() == 0);
    else if (val.is_null()) regs[dst] = Value(true);
    else [[unlikely]] regs[dst] = Value(!to_bool(val));
    return ip;
}

HOT_HANDLER impl_NOT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst, src] = decode::args<u8, u8>(ip);
    Value& val = regs[src];
    if (val.is_bool()) [[likely]] regs[dst] = Value(!val.as_bool());
    else if (val.is_int()) regs[dst] = Value(val.as_int() == 0);
    else if (val.is_null()) regs[dst] = Value(true);
    else [[unlikely]] regs[dst] = Value(!to_bool(val));
    return ip;
}

// BIT_NOT (~)
HOT_HANDLER impl_BIT_NOT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst, src] = decode::args<u16, u16>(ip);
    Value& val = regs[src];
    if (val.is_int()) [[likely]] regs[dst] = Value(~val.as_int());
    else [[unlikely]] regs[dst] = OperatorDispatcher::find(OpCode::BIT_NOT, val)(&state->heap, val);
    return ip;
}

HOT_HANDLER impl_BIT_NOT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst, src] = decode::args<u8, u8>(ip);
    Value& val = regs[src];
    if (val.is_int()) [[likely]] regs[dst] = Value(~val.as_int());
    else [[unlikely]] regs[dst] = OperatorDispatcher::find(OpCode::BIT_NOT, val)(&state->heap, val);
    return ip;
}

// INC / DEC (Toán hạng 1 register)

// Bản 16-bit
HOT_HANDLER impl_INC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 1 * u16 = 2 bytes
    auto [reg_idx] = decode::args<u16>(ip);
    Value& val = regs[reg_idx];
    
    if (val.is_int()) [[likely]] val = Value(val.as_int() + 1);
    else if (val.is_float()) val = Value(val.as_float() + 1.0);
    else [[unlikely]] { 
        // Đã đọc 2 bytes -> Offset = 2
        return ERROR<2>(ip, regs, constants, state, 1 /*TYPE_ERR*/, "INC requires Number"); 
    }
    return ip;
}

HOT_HANDLER impl_DEC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [reg_idx] = decode::args<u16>(ip);
    Value& val = regs[reg_idx];
    
    if (val.is_int()) [[likely]] val = Value(val.as_int() - 1);
    else if (val.is_float()) val = Value(val.as_float() - 1.0);
    else [[unlikely]] { 
        return ERROR<2>(ip, regs, constants, state, 1 /*TYPE_ERR*/, "DEC requires Number"); 
    }
    return ip;
}

// Bản 8-bit
HOT_HANDLER impl_INC_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 1. Load nhanh: Đọc 1 byte, ip tăng 1. 
    // Struct trả về nằm trong thanh ghi (Register-mapped).
    auto [reg_idx] = decode::args<u8>(ip);
    
    Value& val = regs[reg_idx];

    // 2. Logic xử lý (Optimized Type Check)
    if (val.holds<int64_t>()) [[likely]] {
        // Unsafe set nhanh hơn vì bỏ qua check type lần 2
        val.unsafe_set<int64_t>(val.unsafe_get<int64_t>() + 1);
    } 
    else if (val.holds<double>()) {
        val.unsafe_set<double>(val.unsafe_get<double>() + 1.0);
    } 
    else [[unlikely]] {
        return ERROR<1>(ip, regs, constants, state, 1, "INC requires Number");
    }

    // 3. Return IP đã được tăng sẵn
    return ip;
}

HOT_HANDLER impl_DEC_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [reg_idx] = decode::args<u8>(ip);
    Value& val = regs[reg_idx];
    
    if (val.is_int()) [[likely]] val = Value(val.as_int() - 1);
    else if (val.is_float()) val = Value(val.as_float() - 1.0);
    else [[unlikely]] { 
        return ERROR<1>(ip, regs, constants, state, 1 /*TYPE_ERR*/, "DEC requires Number"); 
    }
    return ip;
}

#undef BINARY_OP_IMPL
#undef BINARY_OP_B_IMPL
#undef CMP_FAST_IMPL

} // namespace meow::handlers
#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {

struct BinaryArgs { uint16_t dst; uint16_t r1; uint16_t r2; } __attribute__((packed));
struct BinaryArgsB { uint8_t dst; uint8_t r1; uint8_t r2; } __attribute__((packed));
struct UnaryArgs { uint16_t dst; uint16_t src; } __attribute__((packed));
struct UnaryArgsB { uint8_t dst; uint8_t src; } __attribute__((packed));

// --- MACROS ---
#define BINARY_OP_IMPL(NAME, OP_ENUM) \
    HOT_HANDLER impl_##NAME(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
        const auto& args = *reinterpret_cast<const BinaryArgs*>(ip); \
        Value& left  = regs[args.r1]; \
        Value& right = regs[args.r2]; \
        regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
        return ip + sizeof(BinaryArgs); \
    }

#define BINARY_OP_B_IMPL(NAME, OP_ENUM) \
    HOT_HANDLER impl_##NAME##_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
        const auto& args = *reinterpret_cast<const BinaryArgsB*>(ip); \
        Value& left  = regs[args.r1]; \
        Value& right = regs[args.r2]; \
        regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
        return ip + sizeof(BinaryArgsB); \
    }

// --- ADD (Arithmetic) ---
HOT_HANDLER impl_ADD(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const BinaryArgs*>(ip);
    Value& left = regs[args.r1];
    Value& right = regs[args.r2];
    if (left.holds_both<int_t>(right)) [[likely]] regs[args.dst] = left.as_int() + right.as_int();
    else if (left.holds_both<float_t>(right)) regs[args.dst] = Value(left.as_float() + right.as_float());
    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    return ip + sizeof(BinaryArgs);
}

HOT_HANDLER impl_ADD_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const BinaryArgsB*>(ip);
    Value& left = regs[args.r1];
    Value& right = regs[args.r2];
    if (left.holds_both<int_t>(right)) [[likely]] regs[args.dst] = left.as_int() + right.as_int();
    else if (left.holds_both<float_t>(right)) regs[args.dst] = Value(left.as_float() + right.as_float());
    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    return ip + sizeof(BinaryArgsB);
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
        const auto& args = *reinterpret_cast<const BinaryArgs*>(ip); \
        Value& left = regs[args.r1]; \
        Value& right = regs[args.r2]; \
        if (left.holds_both<int_t>(right)) [[likely]] { \
            regs[args.dst] = Value(left.as_int() OPERATOR right.as_int()); \
        } else [[unlikely]] { \
            regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
        } \
        return ip + sizeof(BinaryArgs); \
    } \
    HOT_HANDLER impl_##OP_NAME##_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
        const auto& args = *reinterpret_cast<const BinaryArgsB*>(ip); \
        Value& left = regs[args.r1]; \
        Value& right = regs[args.r2]; \
        if (left.holds_both<int_t>(right)) [[likely]] { \
            regs[args.dst] = Value(left.as_int() OPERATOR right.as_int()); \
        } else [[unlikely]] { \
            regs[args.dst] = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
        } \
        return ip + sizeof(BinaryArgsB); \
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
    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
    Value& val = regs[args.src];
    if (val.is_int()) [[likely]] regs[args.dst] = Value(-val.as_int());
    else if (val.is_float()) regs[args.dst] = Value(-val.as_float());
    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::NEG, val)(&state->heap, val);
    return ip + sizeof(UnaryArgs);
}

HOT_HANDLER impl_NEG_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const UnaryArgsB*>(ip);
    Value& val = regs[args.src];
    if (val.is_int()) [[likely]] regs[args.dst] = Value(-val.as_int());
    else if (val.is_float()) regs[args.dst] = Value(-val.as_float());
    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::NEG, val)(&state->heap, val);
    return ip + sizeof(UnaryArgsB);
}

// NOT (!)
HOT_HANDLER impl_NOT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
    Value& val = regs[args.src];
    if (val.is_bool()) [[likely]] regs[args.dst] = Value(!val.as_bool());
    else if (val.is_int()) regs[args.dst] = Value(val.as_int() == 0);
    else if (val.is_null()) regs[args.dst] = Value(true);
    else [[unlikely]] regs[args.dst] = Value(!to_bool(val));
    return ip + sizeof(UnaryArgs);
}

HOT_HANDLER impl_NOT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const UnaryArgsB*>(ip);
    Value& val = regs[args.src];
    if (val.is_bool()) [[likely]] regs[args.dst] = Value(!val.as_bool());
    else if (val.is_int()) regs[args.dst] = Value(val.as_int() == 0);
    else if (val.is_null()) regs[args.dst] = Value(true);
    else [[unlikely]] regs[args.dst] = Value(!to_bool(val));
    return ip + sizeof(UnaryArgsB);
}

// BIT_NOT (~)
HOT_HANDLER impl_BIT_NOT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
    Value& val = regs[args.src];
    if (val.is_int()) [[likely]] regs[args.dst] = Value(~val.as_int());
    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::BIT_NOT, val)(&state->heap, val);
    return ip + sizeof(UnaryArgs);
}

HOT_HANDLER impl_BIT_NOT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const UnaryArgsB*>(ip);
    Value& val = regs[args.src];
    if (val.is_int()) [[likely]] regs[args.dst] = Value(~val.as_int());
    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::BIT_NOT, val)(&state->heap, val);
    return ip + sizeof(UnaryArgsB);
}

// INC / DEC (Toán hạng 1 register)
// Bản 16-bit
[[gnu::always_inline]] static const uint8_t* impl_INC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t reg_idx = read_u16(ip);
    Value& val = regs[reg_idx];
    if (val.is_int()) [[likely]] val = Value(val.as_int() + 1);
    else if (val.is_float()) val = Value(val.as_float() + 1.0);
    else [[unlikely]] { state->error("INC requires Number.", ip); return nullptr; }
    return ip;
}
[[gnu::always_inline]] static const uint8_t* impl_DEC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t reg_idx = read_u16(ip);
    Value& val = regs[reg_idx];
    if (val.is_int()) [[likely]] val = Value(val.as_int() - 1);
    else if (val.is_float()) val = Value(val.as_float() - 1.0);
    else [[unlikely]] { state->error("DEC requires Number.", ip); return nullptr; }
    return ip;
}

// Bản 8-bit
[[gnu::always_inline]] static const uint8_t* impl_INC_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint8_t reg_idx = *ip++;
    Value& val = regs[reg_idx];
    if (val.is_int()) [[likely]] val = Value(val.as_int() + 1);
    else if (val.is_float()) val = Value(val.as_float() + 1.0);
    else [[unlikely]] { state->error("INC requires Number.", ip); return nullptr; }
    return ip;
}
[[gnu::always_inline]] static const uint8_t* impl_DEC_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint8_t reg_idx = *ip++;
    Value& val = regs[reg_idx];
    if (val.is_int()) [[likely]] val = Value(val.as_int() - 1);
    else if (val.is_float()) val = Value(val.as_float() - 1.0);
    else [[unlikely]] { state->error("DEC requires Number.", ip); return nullptr; }
    return ip;
}

#undef BINARY_OP_IMPL
#undef BINARY_OP_B_IMPL
#undef CMP_FAST_IMPL

} // namespace meow::handlers
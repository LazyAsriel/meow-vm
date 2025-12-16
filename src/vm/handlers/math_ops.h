#pragma once
#include "vm/handlers/utils.h"
#include "meow_nanbox_layout.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {

struct BinaryArgs { uint16_t dst; uint16_t r1; uint16_t r2; } __attribute__((packed));
struct BinaryArgsB { uint8_t dst; uint8_t r1; uint8_t r2; } __attribute__((packed));
struct UnaryArgs { uint16_t dst; uint16_t src; };

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

HOT_HANDLER impl_ADD(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const BinaryArgs*>(ip);
    Value& left = regs[args.r1];
    Value& right = regs[args.r2];
    // if (left.is_int() && right.is_int()) [[likely]] {
    //     int64_t res = left.as_int() + right.as_int();
    //     regs[args.dst] = Value::from_raw_u64(
    //         NanboxLayout::QNAN_POS | (uint64_t(ValueType::Int) << NanboxLayout::TAG_SHIFT) | (uint64_t(res) & NanboxLayout::PAYLOAD_MASK)
    //     );
    // }
    if (left.is_int() && right.is_int()) [[likely]] regs[args.dst] = left.as_int() + right.as_int();
    else if (left.is_float() && right.is_float()) regs[args.dst] = Value(left.as_float() + right.as_float());
    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    return ip + sizeof(BinaryArgs);
}

HOT_HANDLER impl_ADD_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const BinaryArgsB*>(ip);
    Value& left = regs[args.r1];
    Value& right = regs[args.r2];
    // if (left.is_int() && right.is_int()) [[likely]] {
    //     int64_t res = left.as_int() + right.as_int();
    //     regs[args.dst] = Value::from_raw_u64(
    //         NanboxLayout::QNAN_POS | (uint64_t(ValueType::Int) << NanboxLayout::TAG_SHIFT) | (uint64_t(res) & NanboxLayout::PAYLOAD_MASK)
    //     );
    // }
    if (left.is_int() && right.is_int()) [[likely]] regs[args.dst] = left.as_int() + right.as_int();
    else if (left.is_float() && right.is_float()) regs[args.dst] = Value(left.as_float() + right.as_float());
    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    return ip + sizeof(BinaryArgsB);
}

BINARY_OP_IMPL(SUB, SUB)
BINARY_OP_IMPL(MUL, MUL)
BINARY_OP_IMPL(DIV, DIV)
BINARY_OP_IMPL(MOD, MOD)
BINARY_OP_IMPL(POW, POW)

BINARY_OP_B_IMPL(SUB, SUB)
BINARY_OP_B_IMPL(MUL, MUL)
BINARY_OP_B_IMPL(DIV, DIV)
BINARY_OP_B_IMPL(MOD, MOD)

#define CMP_FAST_IMPL(OP_NAME, OP_ENUM, OPERATOR) \
    HOT_HANDLER impl_##OP_NAME(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
        const auto& args = *reinterpret_cast<const BinaryArgs*>(ip); \
        Value& left = regs[args.r1]; \
        Value& right = regs[args.r2]; \
        if (left.is_int() && right.is_int()) [[likely]] { \
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
        if (left.is_int() && right.is_int()) [[likely]] { \
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
BINARY_OP_IMPL(BIT_AND, BIT_AND)
BINARY_OP_IMPL(BIT_OR, BIT_OR)
BINARY_OP_IMPL(BIT_XOR, BIT_XOR)
BINARY_OP_IMPL(LSHIFT, LSHIFT)
BINARY_OP_IMPL(RSHIFT, RSHIFT)

HOT_HANDLER impl_NEG(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
    Value& val = regs[args.src];
    if (val.is_int()) [[likely]] regs[args.dst] = Value(-val.as_int());
    else if (val.is_float()) regs[args.dst] = Value(-val.as_float());
    else [[unlikely]] regs[args.dst] = OperatorDispatcher::find(OpCode::NEG, val)(&state->heap, val);
    return ip + sizeof(UnaryArgs);
}

HOT_HANDLER impl_BIT_NOT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
    Value& val = regs[args.src];
    regs[args.dst] = OperatorDispatcher::find(OpCode::BIT_NOT, val)(&state->heap, val);
    return ip + sizeof(UnaryArgs);
}

HOT_HANDLER impl_NOT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const auto& args = *reinterpret_cast<const UnaryArgs*>(ip);
    regs[args.dst] = Value(!to_bool(regs[args.src]));
    return ip + sizeof(UnaryArgs);
}

[[gnu::always_inline]] static const uint8_t* impl_INC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t reg_idx = read_u16(ip);
    Value& val = regs[reg_idx];

    if (val.is_int()) [[likely]] {
        val = Value(val.as_int() + 1);
    } 
    else if (val.is_float()) {
        val = Value(val.as_float() + 1.0);
    }
    else [[unlikely]] {
        state->error("INC: Toán hạng phải là số (Int/Real).");
        return impl_PANIC(ip, regs, constants, state);
    }
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_DEC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t reg_idx = read_u16(ip);
    Value& val = regs[reg_idx];

    if (val.is_int()) [[likely]] {
        val = Value(val.as_int() - 1);
    } 
    else if (val.is_float()) {
        val = Value(val.as_float() - 1.0);
    } 
    else [[unlikely]] {
        state->error("DEC: Toán hạng phải là số (Int/Real).");
        return impl_PANIC(ip, regs, constants, state);
    }
    return ip;
}

#undef BINARY_OP_IMPL
#undef BINARY_OP_B_IMPL
#undef CMP_FAST_IMPL

} // namespace meow::handlers
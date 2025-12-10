#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {


#define BINARY_OP_IMPL(NAME) \
    HOT_HANDLER impl_##NAME(const uint8_t* ip, VMState* state) { \
        uint16_t dst = read_u16(ip); \
        uint16_t r1  = read_u16(ip); \
        uint16_t r2  = read_u16(ip); \
        auto& left  = state->reg(r1); \
        auto& right = state->reg(r2); \
        state->reg(dst) = OperatorDispatcher::find(OpCode::NAME, left, right)(&state->heap, left, right); \
        return ip; \
    }


HOT_HANDLER impl_ADD(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t r1 = read_u16(ip);
    uint16_t r2 = read_u16(ip);
    
    auto& left = state->reg(r1);
    auto& right = state->reg(r2);

    if (left.is_int() && right.is_int()) [[likely]] {
        state->reg(dst) = Value(left.as_int() + right.as_int());
    } else if (left.is_float() && right.is_float()) {
        state->reg(dst) = Value(left.as_float() + right.as_float());
    } else {
        state->reg(dst) = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    }
    return ip;
}

HOT_HANDLER impl_LT(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t r1 = read_u16(ip);
    uint16_t r2 = read_u16(ip);
    auto& left = state->reg(r1);
    auto& right = state->reg(r2);

    if (left.is_int() && right.is_int()) [[likely]] {
        state->reg(dst) = Value(left.as_int() < right.as_int());
    } else {
        state->reg(dst) = OperatorDispatcher::find(OpCode::LT, left, right)(&state->heap, left, right);
    }
    return ip;
}

BINARY_OP_IMPL(SUB)
BINARY_OP_IMPL(MUL)
BINARY_OP_IMPL(DIV)
BINARY_OP_IMPL(MOD)
BINARY_OP_IMPL(POW)

BINARY_OP_IMPL(EQ)
BINARY_OP_IMPL(NEQ)
BINARY_OP_IMPL(GT)
BINARY_OP_IMPL(GE)
BINARY_OP_IMPL(LE)

BINARY_OP_IMPL(BIT_AND)
BINARY_OP_IMPL(BIT_OR)
BINARY_OP_IMPL(BIT_XOR)
BINARY_OP_IMPL(LSHIFT)
BINARY_OP_IMPL(RSHIFT)

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
    state->reg(dst) = Value(!to_bool(state->reg(src)));
    return ip;
}

#undef BINARY_OP_IMPL
#undef HOT_HANDLER
}
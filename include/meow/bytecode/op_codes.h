/**
 * @file op_codes.h
 */
#pragma once
#include <cstdint>
#include <meow_enum.h> 

namespace meow {

enum class OpCode : unsigned char {
    LOAD_CONST, LOAD_NULL, LOAD_TRUE, LOAD_FALSE, LOAD_INT, LOAD_FLOAT, MOVE,
    INC, DEC,
    __BEGIN_OPERATOR__,
    ADD, SUB, MUL, DIV, MOD, POW,
    EQ, NEQ, GT, GE, LT, LE,
    NEG, NOT,
    BIT_AND, BIT_OR, BIT_XOR, BIT_NOT, LSHIFT, RSHIFT,
    __END_OPERATOR__,
    GET_GLOBAL, SET_GLOBAL,
    GET_UPVALUE, SET_UPVALUE,
    CLOSURE, CLOSE_UPVALUES,
    JUMP, JUMP_IF_FALSE, JUMP_IF_TRUE,
    CALL, CALL_VOID, RETURN, HALT,
    NEW_ARRAY, NEW_HASH, GET_INDEX, SET_INDEX, GET_KEYS, GET_VALUES,
    NEW_CLASS, NEW_INSTANCE, GET_PROP, SET_PROP,
    SET_METHOD, INHERIT, GET_SUPER,
    INVOKE,
    THROW, SETUP_TRY, POP_TRY,
    IMPORT_MODULE, EXPORT, GET_EXPORT, IMPORT_ALL,

    TAIL_CALL,

    // Optimized Byte-operand instructions
    ADD_B, SUB_B, MUL_B, DIV_B, MOD_B,
    EQ_B, NEQ_B, GT_B, GE_B, LT_B, LE_B,
    JUMP_IF_TRUE_B, JUMP_IF_FALSE_B,
    MOVE_B, LOAD_INT_B,
    BIT_AND_B, BIT_OR_B, BIT_XOR_B, 
    LSHIFT_B, RSHIFT_B,

    // --- Compare & Jump Fused Opcodes (New) ---
    // Cấu trúc: [OpCode] [LHS: u16] [RHS: u16] [Offset: u16]
    JUMP_IF_EQ, JUMP_IF_NEQ, 
    JUMP_IF_GT, JUMP_IF_GE, 
    JUMP_IF_LT, JUMP_IF_LE,

    TOTAL_OPCODES
};

// Specialize traits cho OpCode vì nó là unsigned char (0-255)
template <>
struct enum_traits<OpCode> {
    static constexpr int min_val = 0;
    static constexpr int max_val = 255;
};

struct OpInfo {
    uint8_t arity;         // Số register cần parse (cho Assembler)
    uint8_t operand_bytes; // Số byte tham số cần skip (cho Loader/Disassembler)
};

// Hàm constexpr tra cứu thông tin (Single Source of Truth)
constexpr OpInfo get_op_info(OpCode op) {
    using enum OpCode;
    switch (op) {
        // --- 0 Args ---
        case HALT: case POP_TRY: 
            return {0, 0};

        // --- 1 Register (2 bytes) ---
        case INC: case DEC: case CLOSE_UPVALUES: case IMPORT_ALL: 
        case THROW: case RETURN: 
        case LOAD_NULL: case LOAD_TRUE: case LOAD_FALSE:
            return {1, 2};

        // --- 2 Registers (4 bytes) ---
        case LOAD_CONST: case MOVE: 
        case NEG: case NOT: case BIT_NOT:
        case GET_UPVALUE: case SET_UPVALUE: 
        case CLOSURE:
        case NEW_CLASS: case NEW_INSTANCE:
        case IMPORT_MODULE: case EXPORT:
        case GET_KEYS: case GET_VALUES:
        case GET_SUPER: 
        case GET_GLOBAL: case SET_GLOBAL:
        case INHERIT:
            return {2, 4};

        // --- 3 Registers (6 bytes) ---
        case GET_EXPORT:
        case ADD: case SUB: case MUL: case DIV: case MOD: case POW:
        case EQ: case NEQ: case GT: case GE: case LT: case LE:
        case BIT_AND: case BIT_OR: case BIT_XOR:
        case LSHIFT: case RSHIFT:
        case NEW_ARRAY: case NEW_HASH:
        case GET_INDEX: case SET_INDEX:
        case SET_METHOD: case CALL_VOID: 
            return {3, 6};

        // --- Compare & Jump Fused (New) ---
        // Arity = 2 (LHS, RHS)
        // Operand Bytes = 6 (LHS:2 + RHS:2 + Offset:2)
        case JUMP_IF_EQ: case JUMP_IF_NEQ:
        case JUMP_IF_GT: case JUMP_IF_GE:
        case JUMP_IF_LT: case JUMP_IF_LE:
            return {2, 6};

        // --- 4 Registers (8 bytes) ---
        case CALL: case TAIL_CALL: 
            return {4, 8 + 16}; 

        // --- Special Cases & Variable Layouts ---
        
        case LOAD_INT: case LOAD_FLOAT:
            return {1, 2 + 8}; // 2 byte Reg + 8 byte Value

        case JUMP: case SETUP_TRY: case JUMP_IF_FALSE: case JUMP_IF_TRUE:
            return {0, 2}; 

        case GET_PROP: case SET_PROP:
            return {3, 6 + 48}; 

        case INVOKE:
            return {5, 79};

        // Optimized Byte instructions (operand 1 byte)
        case ADD_B: case SUB_B: case MUL_B: case DIV_B: case MOD_B: 
        case EQ_B: case NEQ_B: case GT_B: case GE_B: case LT_B: case LE_B:
        case BIT_AND_B: case BIT_OR_B: case BIT_XOR_B:
        case LSHIFT_B: case RSHIFT_B:
            return {3, 3}; 

        case JUMP_IF_TRUE_B: case JUMP_IF_FALSE_B:
            return {0, 2}; 

        case MOVE_B:
            return {2, 2}; 

        default: 
            return {0, 0};
    }
}

}
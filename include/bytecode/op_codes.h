/**
 * @file op_codes.h
 * @author LazyPaws
 * @brief Declaration of operating code in TrangMeo
 * @copyright Copyright (c) 2025 LazyPaws
 * @license All rights reserved. Unauthorized copying of this file, in any form
 * or medium, is strictly prohibited
 */

#pragma once

namespace meow {
enum class OpCode : unsigned char {
    LOAD_CONST, LOAD_NULL, LOAD_TRUE, LOAD_FALSE, LOAD_INT, LOAD_FLOAT, MOVE,
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
    THROW, SETUP_TRY, POP_TRY,
    IMPORT_MODULE, EXPORT, GET_EXPORT, IMPORT_ALL, // IMPORT_ALL cần giữ để tránh lệch ID dù không dùng
    
    TOTAL_OPCODES
};
}
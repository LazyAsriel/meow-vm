#pragma once
#include <cstdint>
#include <array>
#include <string_view>
#include <meow_enum.h>

namespace meow {

// --- 1. THE OPCODE ENUM ---
enum class OpCode : uint8_t {
    // --- Core & Constants ---
    NOP, HALT,
    LOAD_CONST, LOAD_NULL, LOAD_TRUE, LOAD_FALSE,
    LOAD_INT, LOAD_FLOAT,
    MOVE,

    // --- Math (Standard 16-bit) ---
    ADD, SUB, MUL, DIV, MOD, POW,
    INC, DEC, NEG,
    
    // --- Logic & Bitwise (Standard 16-bit) ---
    NOT,
    BIT_AND, BIT_OR, BIT_XOR, BIT_NOT,
    LSHIFT, RSHIFT,
    EQ, NEQ, GT, GE, LT, LE,

    // --- Flow Control ---
    JUMP, 
    JUMP_IF_TRUE, JUMP_IF_FALSE,
    CALL, CALL_VOID, RETURN, TAIL_CALL,
    THROW, SETUP_TRY, POP_TRY,

    // --- Data Structures ---
    NEW_ARRAY, NEW_HASH,
    GET_INDEX, SET_INDEX,
    GET_KEYS, GET_VALUES,

    // --- OOP ---
    NEW_CLASS, NEW_INSTANCE,
    GET_PROP, SET_PROP, SET_METHOD,
    INHERIT, GET_SUPER,
    INVOKE,

    // --- Scope & Module ---
    GET_GLOBAL, SET_GLOBAL,
    GET_UPVALUE, SET_UPVALUE,
    CLOSURE, CLOSE_UPVALUES,
    IMPORT_MODULE, EXPORT, GET_EXPORT, IMPORT_ALL,
    
    // Math _B (8-bit regs)
    ADD_B, SUB_B, MUL_B, DIV_B, MOD_B,
    INC_B, DEC_B, NEG_B, NOT_B,

    // Bitwise _B
    BIT_AND_B, BIT_OR_B, BIT_XOR_B, BIT_NOT_B,
    LSHIFT_B, RSHIFT_B,
    
    // Compare _B
    EQ_B, NEQ_B, GT_B, GE_B, LT_B, LE_B,

    // Misc _B
    MOVE_B, 
    LOAD_CONST_B,
    LOAD_INT_B,
    LOAD_FLOAT_B, // Mới thêm
    LOAD_NULL_B,  // Mới thêm
    LOAD_TRUE_B,  // Mới thêm
    LOAD_FALSE_B, // Mới thêm
    
    // Standard (16-bit regs)
    JUMP_IF_EQ, JUMP_IF_NEQ,
    JUMP_IF_GT, JUMP_IF_GE,
    JUMP_IF_LT, JUMP_IF_LE,

    // Byte (8-bit regs)
    JUMP_IF_TRUE_B, JUMP_IF_FALSE_B,
    JUMP_IF_EQ_B, JUMP_IF_NEQ_B,
    JUMP_IF_GT_B, JUMP_IF_GE_B,
    JUMP_IF_LT_B, JUMP_IF_LE_B,

    TOTAL_OPCODES
};

template <>
struct enum_traits<OpCode> {
    static constexpr int min_val = 0;
    static constexpr int max_val = 255;
};

// --- 2. ARGUMENT TYPES ---
enum class ArgType : uint8_t {
    NONE = 0,
    REG8,       // Register index 1 byte (0-255)
    REG16,      // Register index 2 bytes (0-65535)
    U16,        // Generic uint16 (Count, ArgStart...)
    U32,        // Generic uint32
    I64,        // Raw int64 (8 bytes) - Inline value
    F64,        // Raw double (8 bytes) - Inline value
    OFFSET16,   // Jump offset (relative)
    OFFSET32,   // Jump offset (relative - long)
    CONST_IDX   // Index vào Constant Pool (2 bytes)
};

struct OpInfo {
    uint8_t arity;
    uint8_t operand_bytes;
};

struct OpSchema {
    std::string_view name;
    std::array<ArgType, 5> args;
    uint8_t count;

    template <typename... Ts>
    constexpr OpSchema(std::string_view n, Ts... ts) : name(n), args{ts...}, count(sizeof...(ts)) {}

    constexpr OpSchema() : name("UNKNOWN"), args{}, count(0) {}
    
    constexpr uint8_t get_operand_bytes() const {
        uint8_t size = 0;
        for (int i = 0; i < count; ++i) {
            switch (args[i]) {
                case ArgType::REG8:  size += 1; break;
                case ArgType::REG16: case ArgType::U16: 
                case ArgType::OFFSET16: case ArgType::CONST_IDX: size += 2; break;
                case ArgType::U32: case ArgType::OFFSET32: size += 4; break;
                case ArgType::I64: case ArgType::F64: size += 8; break;
                default: break;
            }
        }
        return size;
    }
};

const OpSchema& get_op_schema(OpCode op);
OpInfo get_op_info(OpCode op);

}
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>

// OpCode enum (Đồng bộ với VM)
enum class OpCode : uint8_t {
    // Load/Store
    LOAD_CONST, LOAD_NULL, LOAD_TRUE, LOAD_FALSE,
    LOAD_INT, LOAD_FLOAT, MOVE,
    __BEGIN_OPERATOR__,
    // Binary
    ADD, SUB, MUL, DIV, MOD, POW,
    EQ, NEQ, GT, GE, LT, LE,
    // Unary
    NEG, NOT,
    // Bitwise
    BIT_AND, BIT_OR, BIT_XOR, BIT_NOT, LSHIFT, RSHIFT,
    __END_OPERATOR__,
    // Variables
    GET_GLOBAL, SET_GLOBAL,     // <-- CẬP NHẬT: Nhận Index (u32) thay vì String Const
    GET_UPVALUE, SET_UPVALUE,
    CLOSURE, CLOSE_UPVALUES,
    // Control Flow
    JUMP, JUMP_IF_FALSE, JUMP_IF_TRUE,
    CALL, CALL_VOID, RETURN, HALT,
    // Data Structures
    NEW_ARRAY, NEW_HASH, GET_INDEX, SET_INDEX, GET_KEYS, GET_VALUES,
    // OOP
    NEW_CLASS, NEW_INSTANCE, GET_PROP, SET_PROP, SET_METHOD,
    INHERIT, GET_SUPER,
    // Exceptions
    THROW, SETUP_TRY, POP_TRY,
    // Modules
    IMPORT_MODULE, EXPORT, GET_EXPORT, IMPORT_ALL,
    
    TOTAL_OPCODES
};

extern std::unordered_map<std::string_view, OpCode> OP_MAP;
void init_op_map();

enum class TokenType {
    DIR_FUNC, DIR_ENDFUNC, DIR_REGISTERS, DIR_UPVALUES, DIR_UPVALUE, DIR_CONST,
    LABEL_DEF, IDENTIFIER, OPCODE,
    NUMBER_INT, NUMBER_FLOAT, STRING,
    END_OF_FILE, UNKNOWN
};

struct Token {
    TokenType type;
    std::string_view lexeme; // Tối ưu: Zero-copy
    size_t line;
};

enum class ConstType { NULL_T, INT_T, FLOAT_T, STRING_T, PROTO_REF_T };

struct Constant {
    ConstType type;
    int64_t val_i64 = 0;
    double val_f64 = 0.0;
    std::string val_str;
    uint32_t proto_index = 0; 
};

struct UpvalueInfo {
    bool is_local;
    uint32_t index;
};

struct Prototype {
    std::string name;
    uint32_t num_regs = 0;
    uint32_t num_upvalues = 0;
    
    std::vector<Constant> constants;
    std::vector<UpvalueInfo> upvalues;
    std::vector<uint8_t> bytecode;

    std::unordered_map<std::string, size_t> labels;
    std::vector<std::pair<size_t, std::string>> jump_patches;
    std::vector<std::pair<size_t, std::string>> try_patches;
};
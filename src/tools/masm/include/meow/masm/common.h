#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>
#include <meow/compiler/op_codes.h> // Dùng chung OpCode với VM

namespace meow::masm {

// Init map ánh xạ string -> OpCode
extern std::unordered_map<std::string_view, meow::OpCode> OP_MAP;
void init_op_map();

enum class TokenType {
    DIR_FUNC, DIR_ENDFUNC, DIR_REGISTERS, DIR_UPVALUES, DIR_UPVALUE, DIR_CONST,
    LABEL_DEF, IDENTIFIER, OPCODE,
    NUMBER_INT, NUMBER_FLOAT, STRING,
    END_OF_FILE, UNKNOWN
};

struct Token {
    TokenType type;
    std::string_view lexeme;
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

} // namespace meow::masm
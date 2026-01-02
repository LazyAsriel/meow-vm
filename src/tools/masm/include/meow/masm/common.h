#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <variant> 
#include <meow_variant.h> 
#include <meow/bytecode/op_codes.h>

namespace meow::masm {

// --- 1. Zero-Overhead Error System (8 Bytes) ---

enum class ErrorCode : uint8_t {
    OK = 0,
    UNEXPECTED_TOKEN, EXPECTED_FUNC_NAME, EXPECTED_NUMBER, EXPECTED_U16,
    EXPECTED_INT64, EXPECTED_DOUBLE, EXPECTED_TYPE, EXPECTED_SLOT,
    EXPECTED_STRING, UNKNOWN_OPCODE, UNKNOWN_ANNOTATION, UNKNOWN_CONSTANT,
    UNDEFINED_LABEL, UNDEFINED_PROTO_REF, OUTSIDE_FUNC, LABEL_REDEFINITION,
    FILE_OPEN_FAILED, WRITE_ERROR, READ_ERROR, INDEX_OUT_OF_BOUNDS,
    
    REG_INDEX_TOO_LARGE, UNKNOWN_ARG_TYPE
};

struct Status {
    uint64_t raw = 0; 

    [[gnu::always_inline]] static constexpr Status ok() { return {0}; }
    
    [[gnu::always_inline]] static constexpr Status error(ErrorCode code, uint32_t line, uint32_t col) {
        Status s;
        s.raw = static_cast<uint64_t>(code) 
              | (static_cast<uint64_t>(line) << 8)
              | (static_cast<uint64_t>(col) << 40);
        return s;
    }

    [[gnu::always_inline]] bool is_ok() const { return raw == 0; }
    [[gnu::always_inline]] bool is_err() const { return raw != 0; }
    [[gnu::always_inline]] ErrorCode code() const { return static_cast<ErrorCode>(raw & 0xFF); }
    [[gnu::always_inline]] uint32_t line() const { return static_cast<uint32_t>((raw >> 8) & 0xFFFFFFFF); }
    [[gnu::always_inline]] uint32_t col() const { return static_cast<uint32_t>((raw >> 40) & 0xFFFF); }
};

#define MASM_CHECK(stmt) { Status _s = (stmt); if (_s.raw != 0) [[unlikely]] return _s; }

// --- 2. Token Structures ---

enum class TokenType : uint8_t { 
    DIR_FUNC, DIR_ENDFUNC, DIR_REGISTERS, DIR_UPVALUES, DIR_UPVALUE, DIR_CONST,
    LABEL_DEF, IDENTIFIER, OPCODE,
    NUMBER_INT, NUMBER_FLOAT, STRING,
    DEBUG_INFO, ANNOTATION,
    END_OF_FILE, UNKNOWN
};

struct Token {
    TokenType type;
    std::string_view lexeme; 
    uint32_t line;
    uint32_t col;
    uint16_t payload = 0;
};

// --- 3. IR DEFINITIONS (Intermediate Representation) ---

struct Reg { uint16_t id; bool operator==(const Reg& o) const { return id == o.id; } };
struct StrIdx { uint32_t id; };
struct ConstIdx { uint32_t id; };
struct LabelIdx { uint32_t id; };
struct JumpOffset { int32_t val; };

using IrArg = meow::variant<
    Reg, int64_t, double, 
    StrIdx, ConstIdx, LabelIdx, JumpOffset
>;

struct Arg {
    static IrArg None() { return {}; }
    static IrArg R(uint16_t id) { return Reg{id}; }
    static IrArg Int(int64_t v) { return v; }
    static IrArg F(double v) { return v; }
    static IrArg Str(uint32_t id) { return StrIdx{id}; }
    static IrArg Const(uint32_t id) { return ConstIdx{id}; }
    static IrArg Label(uint32_t id) { return LabelIdx{id}; }
    static IrArg Off(int32_t v) { return JumpOffset{v}; }
};

struct IrInstruction {
    meow::OpCode op;
    uint8_t arg_count;
    uint32_t line;
    IrArg args[4]; 

    IrInstruction() : op(meow::OpCode::NOP), arg_count(0), line(0) {
        for(auto& a : args) a = Arg::None();
    }
};

// --- 4. Assembler Structures ---

extern std::unordered_map<std::string_view, meow::OpCode> OP_MAP;
void init_op_map();

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

struct LineInfo {
    uint32_t offset;
    uint32_t line;
    uint32_t col;
    uint32_t file_idx;
};

// --- 5. ProtoFlags & Operators ---

enum class ProtoFlags : uint8_t {
    NONE = 0, HAS_DEBUG_INFO = 1 << 0, IS_VARARG = 1 << 1
};

[[nodiscard]] constexpr ProtoFlags operator|(ProtoFlags a, ProtoFlags b) { return (ProtoFlags)((uint8_t)a | (uint8_t)b); }
[[nodiscard]] constexpr ProtoFlags operator&(ProtoFlags a, ProtoFlags b) { return (ProtoFlags)((uint8_t)a & (uint8_t)b); }
[[nodiscard]] constexpr ProtoFlags operator^(ProtoFlags a, ProtoFlags b) { return (ProtoFlags)((uint8_t)a ^ (uint8_t)b); }
[[nodiscard]] constexpr ProtoFlags operator~(ProtoFlags a) { return (ProtoFlags)(~(uint8_t)a); }

[[nodiscard]] constexpr bool has_flag(ProtoFlags flags, ProtoFlags check) { return (flags & check) != ProtoFlags::NONE; }

struct Prototype {
    std::string name;
    uint32_t num_regs = 0;
    uint32_t num_upvalues = 0;
    ProtoFlags flags = ProtoFlags::NONE; 

    std::vector<Constant> constants;
    std::vector<UpvalueInfo> upvalues;
    std::vector<uint8_t> bytecode;

    std::vector<IrInstruction> ir_code;
    std::vector<std::string> string_pool; 
    
    std::vector<LineInfo> lines;
    std::vector<std::string> source_files;
    std::unordered_map<std::string, uint32_t> file_map; 
    
    std::unordered_map<std::string_view, size_t> labels;
    std::vector<std::pair<size_t, std::string_view>> jump_patches;
    std::vector<std::pair<size_t, std::string_view>> try_patches;

    uint32_t add_file(const std::string& file) {
        if (auto it = file_map.find(file); it != file_map.end()) return it->second;
        uint32_t idx = static_cast<uint32_t>(source_files.size());
        source_files.push_back(file);
        file_map[file] = idx;
        return idx;
    }
    
    size_t current_code_size() const {
        return bytecode.size(); 
    }
};

} // namespace meow::masm
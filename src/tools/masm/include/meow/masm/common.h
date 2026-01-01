#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <meow/bytecode/op_codes.h>

namespace meow::masm {

// --- Zero-Overhead Error System (8 Bytes) ---

enum class ErrorCode : uint8_t {
    OK = 0,
    UNEXPECTED_TOKEN,
    EXPECTED_FUNC_NAME,
    EXPECTED_NUMBER,
    EXPECTED_U16,
    EXPECTED_INT64,
    EXPECTED_DOUBLE,
    EXPECTED_TYPE,
    EXPECTED_SLOT,
    EXPECTED_STRING,
    UNKNOWN_OPCODE,
    UNKNOWN_ANNOTATION,
    UNKNOWN_CONSTANT,
    UNDEFINED_LABEL,
    UNDEFINED_PROTO_REF,
    OUTSIDE_FUNC,
    LABEL_REDEFINITION,
    FILE_OPEN_FAILED,
    WRITE_ERROR,
    READ_ERROR,           
    INDEX_OUT_OF_BOUNDS   
};

// Layout 64-bit packed:
// [0-7]   : ErrorCode (8 bits)
// [8-39]  : Line (32 bits)
// [40-55] : Col (16 bits)
// [56-63] : Reserved (8 bits)
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

// Macro check lỗi không branch (hoặc branch rất ít tốn kém)
#define MASM_CHECK(stmt) \
    { Status _s = (stmt); if (_s.raw != 0) [[unlikely]] return _s; }

// --- Token Structures ---

enum class TokenType : uint8_t { 
    DIR_FUNC, DIR_ENDFUNC, DIR_REGISTERS, DIR_UPVALUES, DIR_UPVALUE, DIR_CONST,
    LABEL_DEF, IDENTIFIER, OPCODE,
    NUMBER_INT, NUMBER_FLOAT, STRING,
    DEBUG_INFO, ANNOTATION,
    END_OF_FILE, UNKNOWN
};

struct Token {
    TokenType type;
    std::string_view lexeme; // 16 bytes
    uint32_t line;
    uint32_t col;
    uint16_t payload = 0;
};

// --- Assembler Structures ---

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

enum class ProtoFlags : uint8_t {
    NONE = 0,
    HAS_DEBUG_INFO = 1 << 0,
    IS_VARARG      = 1 << 1
};

inline ProtoFlags operator|(ProtoFlags a, ProtoFlags b) { return static_cast<ProtoFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b)); }
inline ProtoFlags operator&(ProtoFlags a, ProtoFlags b) { return static_cast<ProtoFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b)); }
inline ProtoFlags operator~(ProtoFlags a) { return static_cast<ProtoFlags>(~static_cast<uint8_t>(a)); }
inline bool has_flag(ProtoFlags flags, ProtoFlags check) { return (flags & check) != ProtoFlags::NONE; }

struct Prototype {
    std::string name;
    uint32_t num_regs = 0;
    uint32_t num_upvalues = 0;
    ProtoFlags flags = ProtoFlags::NONE; 

    std::vector<Constant> constants;
    std::vector<UpvalueInfo> upvalues;
    std::vector<uint8_t> bytecode;

    // Debug Info
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
};

} // namespace meow::masm
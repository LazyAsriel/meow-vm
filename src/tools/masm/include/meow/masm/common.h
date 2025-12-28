#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>
#include <meow/bytecode/op_codes.h>

namespace meow::masm {

extern std::unordered_map<std::string_view, meow::OpCode> OP_MAP;
void init_op_map();

enum class TokenType {
    DIR_FUNC, DIR_ENDFUNC, DIR_REGISTERS, DIR_UPVALUES, DIR_UPVALUE, DIR_CONST,
    LABEL_DEF, IDENTIFIER, OPCODE,
    NUMBER_INT, NUMBER_FLOAT, STRING,
    
    // --- Token mới ---
    DEBUG_INFO, // #^ "file" line:col
    ANNOTATION, // #@ directive

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

// Cấu trúc lưu vị trí dòng code
struct LineInfo {
    uint32_t offset;    // Bytecode offset
    uint32_t line;
    uint32_t col;
    uint32_t file_idx;  // Index vào bảng tên file
};

// Enum cờ cho Prototype (Dùng bitmask)
enum class ProtoFlags : uint8_t {
    NONE = 0,
    HAS_DEBUG_INFO = 1 << 0, // Bit 0: Có debug info
    IS_VARARG      = 1 << 1  // Bit 1: Hàm variadic (Dành cho tương lai)
};

// Operator overloading để dùng phép bitwise với enum class
inline ProtoFlags operator|(ProtoFlags a, ProtoFlags b) {
    return static_cast<ProtoFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline ProtoFlags operator&(ProtoFlags a, ProtoFlags b) {
    return static_cast<ProtoFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline ProtoFlags operator~(ProtoFlags a) {
    return static_cast<ProtoFlags>(~static_cast<uint8_t>(a));
}
inline bool has_flag(ProtoFlags flags, ProtoFlags check) {
    return (flags & check) != ProtoFlags::NONE;
}

struct Prototype {
    std::string name;
    uint32_t num_regs = 0;
    uint32_t num_upvalues = 0;
    
    // --- Flags ---
    ProtoFlags flags = ProtoFlags::NONE; 

    std::vector<Constant> constants;
    std::vector<UpvalueInfo> upvalues;
    std::vector<uint8_t> bytecode;

    // --- Debug Info Storage ---
    std::vector<LineInfo> lines;
    std::vector<std::string> source_files;
    std::unordered_map<std::string, uint32_t> file_map; 

    std::unordered_map<std::string, size_t> labels;
    std::vector<std::pair<size_t, std::string>> jump_patches;
    std::vector<std::pair<size_t, std::string>> try_patches;

    // Helper: Thêm file và trả về index (có deduplicate)
    uint32_t add_file(const std::string& file) {
        if (file_map.count(file)) return file_map[file];
        uint32_t idx = source_files.size();
        source_files.push_back(file);
        file_map[file] = idx;
        return idx;
    }
};

} // namespace meow::masm
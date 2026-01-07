#include "bytecode/loader.h"
#include <meow/memory/memory_manager.h>
#include <meow/memory/gc_disable_guard.h>
#include <meow/core/string.h>
#include <meow/core/function.h>
#include <meow/core/module.h>
#include <meow/value.h>
#include <meow/bytecode/chunk.h>
#include <meow/bytecode/op_codes.h>
#include <cstring>
#include <bit>

#define CHECK(expr) { auto _res = (expr); if (_res.failed()) return _res.error(); }
#define TRY_VAL(var, expr) auto var##_res = (expr); if (var##_res.failed()) return var##_res.error(); auto var = var##_res.value();

namespace meow {

constexpr uint32_t MAGIC_NUMBER = 0x4D454F57; 
constexpr uint32_t VER_WITH_FLAGS = 2;  

enum class ConstantTag : uint8_t { NULL_T, INT_T, FLOAT_T, STRING_T, PROTO_REF_T };
enum class ProtoFlags : uint8_t { NONE = 0, HAS_DEBUG_INFO = 1 << 0 };

Loader::Loader(MemoryManager* heap, const std::vector<uint8_t>& data, std::string_view filename)
    : heap_(heap), data_(data), cursor_(0) {
    ctx_.load(filename);
}

Status<LoaderErrorCode> Loader::error(LoaderErrorCode code) const {
    uint32_t line = static_cast<uint32_t>(cursor_ >> 16);
    uint32_t col = static_cast<uint32_t>(cursor_ & 0xFFFF);
    return Status<LoaderErrorCode>::make(code, ctx_.file_id, line, col);
}

Result<void, LoaderErrorCode> Loader::check_can_read(size_t bytes) {
    if (cursor_ + bytes > data_.size()) return error(LoaderErrorCode::UNEXPECTED_EOF);
    return {};
}

Result<uint8_t, LoaderErrorCode> Loader::read_u8() {
    CHECK(check_can_read(1));
    return data_[cursor_++];
}

Result<uint16_t, LoaderErrorCode> Loader::read_u16() {
    CHECK(check_can_read(2));
    uint16_t val = static_cast<uint16_t>(data_[cursor_]) | (static_cast<uint16_t>(data_[cursor_ + 1]) << 8);
    cursor_ += 2;
    return val;
}

Result<uint32_t, LoaderErrorCode> Loader::read_u32() {
    CHECK(check_can_read(4));
    uint32_t val = static_cast<uint32_t>(data_[cursor_]) | (static_cast<uint32_t>(data_[cursor_ + 1]) << 8) |
                   (static_cast<uint32_t>(data_[cursor_ + 2]) << 16) | (static_cast<uint32_t>(data_[cursor_ + 3]) << 24);
    cursor_ += 4;
    return val;
}

Result<uint64_t, LoaderErrorCode> Loader::read_u64() {
    CHECK(check_can_read(8));
    uint64_t val; std::memcpy(&val, &data_[cursor_], 8); cursor_ += 8;
    return val; 
}

Result<double, LoaderErrorCode> Loader::read_f64() {
    TRY_VAL(raw, read_u64());
    return std::bit_cast<double>(raw);
}

Result<string_t, LoaderErrorCode> Loader::read_string() {
    TRY_VAL(length, read_u32());
    CHECK(check_can_read(length));
    std::string str(reinterpret_cast<const char*>(data_.data() + cursor_), length);
    cursor_ += length;
    return heap_->new_string(str);
}

Result<Value, LoaderErrorCode> Loader::read_constant(size_t current_proto_idx, size_t current_const_idx) {
    TRY_VAL(tag_raw, read_u8());
    switch (static_cast<ConstantTag>(tag_raw)) {
        case ConstantTag::NULL_T:   return Value(null_t{});
        case ConstantTag::INT_T:    { TRY_VAL(val, read_u64()); return Value(static_cast<int64_t>(val)); }
        case ConstantTag::FLOAT_T:  { TRY_VAL(val, read_f64()); return Value(val); }
        case ConstantTag::STRING_T: { TRY_VAL(val, read_string()); return Value(val); }
        case ConstantTag::PROTO_REF_T: {
            TRY_VAL(target, read_u32());
            patches_.push_back({current_proto_idx, current_const_idx, target});
            return Value(null_t{});
        }
        default: return error(LoaderErrorCode::UNKNOWN_CONSTANT_TAG);
    }
}

Result<proto_t, LoaderErrorCode> Loader::read_prototype(size_t current_proto_idx) {
    TRY_VAL(num_registers, read_u32());
    TRY_VAL(num_upvalues, read_u32());
    bool has_debug_info = false;
    if (file_version_ >= VER_WITH_FLAGS) {
        TRY_VAL(raw_flags, read_u8());
        has_debug_info = (raw_flags & static_cast<uint8_t>(ProtoFlags::HAS_DEBUG_INFO)) != 0;
    }

    TRY_VAL(name_idx, read_u32());
    TRY_VAL(pool_size, read_u32());
    
    std::vector<Value> constants; constants.reserve(pool_size);
    for (uint32_t i = 0; i < pool_size; ++i) {
        TRY_VAL(c, read_constant(current_proto_idx, i));
        constants.push_back(c);
    }
    
    string_t name = nullptr;
    if (name_idx < constants.size() && constants[name_idx].is_string()) name = constants[name_idx].as_string();

    TRY_VAL(upval_count, read_u32());
    std::vector<UpvalueDesc> upvalue_descs; upvalue_descs.reserve(upval_count);
    for (uint32_t i = 0; i < upval_count; ++i) {
        TRY_VAL(is_local, read_u8()); TRY_VAL(idx, read_u32());
        upvalue_descs.emplace_back(is_local == 1, idx);
    }

    TRY_VAL(bc_size, read_u32());
    CHECK(check_can_read(bc_size));
    std::vector<uint8_t> bytecode(data_.data() + cursor_, data_.data() + cursor_ + bc_size);
    cursor_ += bc_size;
    
    std::vector<std::string> source_files;
    std::vector<LineInfo> lines;
    if (has_debug_info) {
        TRY_VAL(num_files, read_u32());
        source_files.reserve(num_files);
        for(uint32_t i = 0; i < num_files; ++i) {
            TRY_VAL(len, read_u32()); CHECK(check_can_read(len));
            source_files.emplace_back(reinterpret_cast<const char*>(data_.data() + cursor_), len);
            cursor_ += len;
        }
        TRY_VAL(num_lines, read_u32());
        lines.reserve(num_lines);
        for(uint32_t i = 0; i < num_lines; ++i) {
            LineInfo info;
            TRY_VAL(off, read_u32()); info.offset = off;
            TRY_VAL(ln, read_u32()); info.line = ln;
            TRY_VAL(cl, read_u32()); info.col = cl;
            TRY_VAL(fi, read_u32()); info.file_idx = fi;
            lines.push_back(info);
        }
    }
    Chunk chunk(std::move(bytecode), std::move(constants), std::move(source_files), std::move(lines));
    return heap_->new_proto(num_registers, num_upvalues, name, std::move(chunk), std::move(upvalue_descs));
}

Result<void, LoaderErrorCode> Loader::check_magic() {
    TRY_VAL(magic, read_u32());
    if (magic != MAGIC_NUMBER) return error(LoaderErrorCode::MAGIC_MISMATCH);
    TRY_VAL(ver, read_u32());
    file_version_ = ver;
    return (ver == 1 || ver == VER_WITH_FLAGS) ? Result<void, LoaderErrorCode>{} : error(LoaderErrorCode::UNSUPPORTED_VERSION);
}

Result<void, LoaderErrorCode> Loader::link_prototypes() {    
    for (const auto& patch : patches_) {
        if (patch.proto_idx >= loaded_protos_.size() || patch.target_idx >= loaded_protos_.size()) 
            return error(LoaderErrorCode::INVALID_PROTO_INDEX);
        Chunk& chunk = loaded_protos_[patch.proto_idx]->get_chunk(); 
        if (patch.const_idx >= chunk.get_pool_size()) return error(LoaderErrorCode::INVALID_CONST_INDEX);
        chunk.get_constant_ref(patch.const_idx) = Value(loaded_protos_[patch.target_idx]);
    }
    return {};
}

Result<proto_t, LoaderErrorCode> Loader::load_module() {
    GCDisableGuard guard(heap_);
    CHECK(check_magic());
    
    TRY_VAL(main_idx, read_u32());
    TRY_VAL(proto_count, read_u32());
    if (proto_count == 0) return error(LoaderErrorCode::NO_PROTOTYPES_FOUND);
    
    loaded_protos_.reserve(proto_count);
    for (uint32_t i = 0; i < proto_count; ++i) {
        TRY_VAL(p, read_prototype(i));
        loaded_protos_.push_back(p);
    }
    
    if (main_idx >= loaded_protos_.size()) return error(LoaderErrorCode::MAIN_PROTO_INVALID);
    CHECK(link_prototypes());
    return loaded_protos_[main_idx];
}

// --- Recursive Global Patching ---
static Result<void, LoaderErrorCode> patch_chunk_globals_recursive(module_t mod, proto_t proto, std::unordered_set<proto_t>& visited) {
    if (!proto || visited.contains(proto)) return {};
    visited.insert(proto);

    Chunk& chunk = proto->get_chunk();
    const uint8_t* code = chunk.get_code();
    size_t size = chunk.get_code_size();
    size_t ip = 0;

    for (size_t i = 0; i < chunk.get_pool_size(); ++i) {
        if (chunk.get_constant(i).is_proto()) CHECK(patch_chunk_globals_recursive(mod, chunk.get_constant(i).as_proto(), visited));
    }

    while (ip < size) {
        OpCode op = static_cast<OpCode>(code[ip]);
        
        // GET_GLOBAL(reg16, idx) -> idx ở byte 3,4 (ip+3)
        // SET_GLOBAL(idx, reg16) -> idx ở byte 1,2 (ip+1)
        if (op == OpCode::GET_GLOBAL || op == OpCode::SET_GLOBAL) {
            size_t operand_offset = (op == OpCode::GET_GLOBAL) ? (ip + 3) : (ip + 1);
            if (operand_offset + 2 <= size) {
                uint16_t name_idx = static_cast<uint16_t>(code[operand_offset]) | (static_cast<uint16_t>(code[operand_offset + 1]) << 8);
                if (name_idx < chunk.get_pool_size()) {
                    Value name_val = chunk.get_constant(name_idx);
                    if (name_val.is_string()) {
                        uint32_t global_idx = mod->intern_global(name_val.as_string());
                        if (global_idx > 0xFFFF) return Status<LoaderErrorCode>::make(LoaderErrorCode::TOO_MANY_GLOBALS, 0, 0, 0);
                        chunk.patch_u16(operand_offset, static_cast<uint16_t>(global_idx));
                    }
                }
            }
        }
        
        // Dùng info từ op_codes.cpp mới để nhảy
        const auto info = meow::get_op_info(op);
        ip += 1 + info.operand_bytes;

        // Safety break
        if (info.operand_bytes == 0 && info.arity == 0 && op != OpCode::HALT && op != OpCode::RETURN && op != OpCode::POP_TRY && op != OpCode::CLOSE_UPVALUES) 
            ip += 1;
    }
    return {};
}

Result<void, LoaderErrorCode> Loader::link_module(module_t module) {
    if (!module || !module->is_has_main()) return {};
    std::unordered_set<proto_t> visited;
    return patch_chunk_globals_recursive(module, module->get_main_proto(), visited);
}

} // namespace meow
#undef CHECK
#undef TRY_VAL
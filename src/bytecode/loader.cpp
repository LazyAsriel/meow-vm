#include "bytecode/loader.h"
#include "memory/memory_manager.h"
#include "core/objects/string.h"
#include "core/objects/function.h"
#include "core/objects/module.h"
#include "core/value.h"
#include "bytecode/chunk.h"
#include "bytecode/op_codes.h"
#include <unordered_set>

namespace meow {

constexpr uint32_t MAGIC_NUMBER = 0x4D454F57; // "MEOW"
constexpr uint32_t FORMAT_VERSION = 1;

enum class ConstantTag : uint8_t {
    NULL_T,
    INT_T,
    FLOAT_T,
    STRING_T,
    PROTO_REF_T
};

Loader::Loader(MemoryManager* heap, const std::vector<uint8_t>& data)
    : heap_(heap), data_(data), cursor_(0) {}

void Loader::check_can_read(size_t bytes) {
    if (cursor_ + bytes > data_.size()) {
        throw LoaderError("Unexpected end of file. File is truncated or corrupt.");
    }
}

uint8_t Loader::read_u8() {
    check_can_read(1);
    return data_[cursor_++];
}

uint16_t Loader::read_u16() {
    check_can_read(2);
    // Little-endian load
    uint16_t val = static_cast<uint16_t>(data_[cursor_]) |
                   (static_cast<uint16_t>(data_[cursor_ + 1]) << 8);
    cursor_ += 2;
    return val;
}

uint32_t Loader::read_u32() {
    check_can_read(4);
    uint32_t val = static_cast<uint32_t>(data_[cursor_]) |
                   (static_cast<uint32_t>(data_[cursor_ + 1]) << 8) |
                   (static_cast<uint32_t>(data_[cursor_ + 2]) << 16) |
                   (static_cast<uint32_t>(data_[cursor_ + 3]) << 24);
    cursor_ += 4;
    return val;
}

uint64_t Loader::read_u64() {
    check_can_read(8);
    uint64_t val;
    std::memcpy(&val, &data_[cursor_], 8); 
    cursor_ += 8;
    return val; // Giả định little-endian machine (x86/arm64)
}

double Loader::read_f64() {
    return std::bit_cast<double>(read_u64());
}

string_t Loader::read_string() {
    uint32_t length = read_u32();
    check_can_read(length);
    std::string str(reinterpret_cast<const char*>(data_.data() + cursor_), length);
    cursor_ += length;
    return heap_->new_string(str);
}

Value Loader::read_constant(size_t current_proto_idx, size_t current_const_idx) {
    ConstantTag tag = static_cast<ConstantTag>(read_u8());
    switch (tag) {
        case ConstantTag::NULL_T:   return Value(null_t{});
        case ConstantTag::INT_T:    return Value(static_cast<int64_t>(read_u64()));
        case ConstantTag::FLOAT_T:  return Value(read_f64());
        case ConstantTag::STRING_T: return Value(read_string());
        
        case ConstantTag::PROTO_REF_T: {
            uint32_t target_proto_index = read_u32();
            // Lưu lại vị trí để vá sau khi load xong hết proto
            patches_.push_back({current_proto_idx, current_const_idx, target_proto_index});
            return Value(null_t{}); 
        }
        default:
            throw LoaderError("Unknown constant tag in binary file.");
    }
}

proto_t Loader::read_prototype(size_t current_proto_idx) {
    uint32_t num_registers = read_u32();
    uint32_t num_upvalues = read_u32();
    uint32_t name_idx_in_pool = read_u32();

    uint32_t constant_pool_size = read_u32();
    std::vector<Value> constants;
    constants.reserve(constant_pool_size);
    
    for (uint32_t i = 0; i < constant_pool_size; ++i) {
        constants.push_back(read_constant(current_proto_idx, i));
    }
    
    string_t name = nullptr;
    if (name_idx_in_pool < constants.size() && constants[name_idx_in_pool].is_string()) {
        name = constants[name_idx_in_pool].as_string();
    }

    // Upvalues
    uint32_t upvalue_desc_count = read_u32();
    if (upvalue_desc_count != num_upvalues) {
         throw LoaderError("Upvalue count mismatch.");
    }
    std::vector<UpvalueDesc> upvalue_descs;
    upvalue_descs.reserve(upvalue_desc_count);
    for (uint32_t i = 0; i < upvalue_desc_count; ++i) {
        bool is_local = (read_u8() == 1);
        uint32_t index = read_u32();
        upvalue_descs.emplace_back(is_local, index);
    }

    // Bytecode
    uint32_t bytecode_size = read_u32();
    check_can_read(bytecode_size);
    std::vector<uint8_t> bytecode(data_.data() + cursor_, data_.data() + cursor_ + bytecode_size);
    cursor_ += bytecode_size;
    
    Chunk chunk(std::move(bytecode), std::move(constants));
    return heap_->new_proto(num_registers, num_upvalues, name, std::move(chunk), std::move(upvalue_descs));
}

void Loader::check_magic() {
    if (read_u32() != MAGIC_NUMBER) {
        throw LoaderError("Not a valid Meow bytecode file (magic number mismatch).");
    }
    uint32_t version = read_u32();
    if (version != FORMAT_VERSION) {
        throw LoaderError(std::format("Bytecode version mismatch. File is v{}, VM supports v{}.", version, FORMAT_VERSION));
    }
}

void Loader::link_prototypes() {    
    for (const auto& patch : patches_) {
        if (patch.proto_idx >= loaded_protos_.size() || patch.target_idx >= loaded_protos_.size()) {
            throw LoaderError("Invalid prototype reference indices.");
        }

        proto_t parent_proto = loaded_protos_[patch.proto_idx];
        proto_t child_proto = loaded_protos_[patch.target_idx];

        // Hack const_cast để sửa constant pool (được phép lúc load)
        Chunk& chunk = const_cast<Chunk&>(parent_proto->get_chunk()); 
        
        if (patch.const_idx >= chunk.get_pool_size()) {
             throw LoaderError("Internal Error: Patch constant index out of bounds.");
        }

        chunk.get_constant_ref(patch.const_idx) = Value(child_proto);
    }
}

proto_t Loader::load_module() {
    check_magic();
    
    uint32_t main_proto_index = read_u32();
    uint32_t prototype_count = read_u32();
    
    if (prototype_count == 0) throw LoaderError("No prototypes found.");
    
    loaded_protos_.reserve(prototype_count);
    for (uint32_t i = 0; i < prototype_count; ++i) {
        loaded_protos_.push_back(read_prototype(i));
    }
    
    if (main_proto_index >= loaded_protos_.size()) throw LoaderError("Main proto index invalid.");
    
    link_prototypes();
    
    return loaded_protos_[main_proto_index];
}

// ----------------------------------------------------------------------------
// [OPTIMIZATION] Linker Phase
// ----------------------------------------------------------------------------

static void patch_chunk_globals_recursive(module_t mod, proto_t proto, std::unordered_set<proto_t>& visited) {
    if (!proto || visited.contains(proto)) return;
    visited.insert(proto);

    Chunk& chunk = const_cast<Chunk&>(proto->get_chunk());
    const uint8_t* code = chunk.get_code();
    size_t size = chunk.get_code_size();
    size_t ip = 0;

    // 1. Đệ quy xuống các proto con
    for (size_t i = 0; i < chunk.get_pool_size(); ++i) {
        if (chunk.get_constant(i).is_proto()) {
            patch_chunk_globals_recursive(mod, chunk.get_constant(i).as_proto(), visited);
        }
    }

    // 2. Quét bytecode và patch GET_GLOBAL / SET_GLOBAL
    while (ip < size) {
        OpCode op = static_cast<OpCode>(code[ip]);
        
        // --- PATCHING ---
        if (op == OpCode::GET_GLOBAL || op == OpCode::SET_GLOBAL) {
            // [Op:1] [Dst/Src:2] [NameIdx:2]
            size_t operand_offset = ip + 3;
            
            if (operand_offset + 2 <= size) {
                // Đọc Name Index (u16)
                uint16_t name_idx = static_cast<uint16_t>(code[operand_offset]) | 
                                    (static_cast<uint16_t>(code[operand_offset + 1]) << 8);
                
                if (name_idx < chunk.get_pool_size()) {
                    Value name_val = chunk.get_constant(name_idx);
                    if (name_val.is_string()) {
                        // Tìm/Tạo index cho global này trong module
                        // Hàm này (trong module.h) sẽ trả về index (u32)
                        // Ta ép về u16 để ghi vào bytecode (giới hạn 65535 globals/module)
                        uint32_t global_idx = mod->intern_global(name_val.as_string());
                        
                        if (global_idx > 0xFFFF) {
                            throw LoaderError("Module has too many globals (> 65535).");
                        }

                        // Ghi đè trực tiếp vào bytecode
                        chunk.patch_u16(operand_offset, static_cast<uint16_t>(global_idx));
                    }
                }
            }
        }

        // --- SKIP INSTRUCTION ---
        // Cần nhảy qua các byte tham số để đến OpCode tiếp theo.
        // Logic này cần đồng bộ với Assembler/VM args size.
        ip += 1; // Op
        switch (op) {
            // 1 byte args (none)
            case OpCode::HALT: case OpCode::POP_TRY: 
                break;

            // 1 u16 arg (Total 2 bytes + 1 op = 3)
            case OpCode::CLOSE_UPVALUES: case OpCode::IMPORT_ALL: case OpCode::THROW: 
            case OpCode::RETURN: 
                ip += 2; break;
                
            // 2 u16 args (Total 4 bytes + 1 op = 5)
            case OpCode::LOAD_CONST: case OpCode::MOVE: 
            case OpCode::NEG: case OpCode::NOT: case OpCode::BIT_NOT: 
            case OpCode::GET_UPVALUE: case OpCode::SET_UPVALUE: 
            case OpCode::CLOSURE:
            case OpCode::NEW_CLASS: case OpCode::NEW_INSTANCE: 
            case OpCode::IMPORT_MODULE: case OpCode::EXPORT: 
            case OpCode::GET_KEYS: case OpCode::GET_VALUES:
            case OpCode::GET_SUPER: 
            case OpCode::GET_GLOBAL: case OpCode::SET_GLOBAL: // Target
            case OpCode::LOAD_NULL: case OpCode::LOAD_TRUE: case OpCode::LOAD_FALSE:
                ip += 4; break;

            // 3 u16 args (Total 6 bytes + 1 op = 7)
            case OpCode::GET_EXPORT: 
            case OpCode::ADD: case OpCode::SUB: case OpCode::MUL: case OpCode::DIV:
            case OpCode::MOD: case OpCode::POW: case OpCode::EQ: case OpCode::NEQ:
            case OpCode::GT: case OpCode::GE: case OpCode::LT: case OpCode::LE:
            case OpCode::BIT_AND: case OpCode::BIT_OR: case OpCode::BIT_XOR:
            case OpCode::LSHIFT: case OpCode::RSHIFT: 
            case OpCode::NEW_ARRAY: case OpCode::NEW_HASH: 
            case OpCode::GET_INDEX: case OpCode::SET_INDEX: 
            case OpCode::GET_PROP: case OpCode::SET_PROP:
            case OpCode::SET_METHOD: case OpCode::CALL_VOID:
            case OpCode::INHERIT:
                ip += 6; break;
            
            // 4 u16 args (Total 8 bytes + 1 op = 9)
            case OpCode::CALL: 
                ip += 8; break;
            
            // 1 u16 + 1 u64 arg (Total 10 bytes + 1 op = 11)
            case OpCode::LOAD_INT: case OpCode::LOAD_FLOAT:
                ip += 10; break;

            // 12 bytes cache (GET_PROP/SET_PROP cache slot - Tùy implementation)
            // Lưu ý: Trong assembler.cpp cũ, GET_PROP/SET_PROP emit thêm 12 byte cache
            // Nếu dùng Inline Cache, cần +12 byte ở đây.
            // Check lại assembler: emit_u64(0); emit_u32(0); -> 12 byte
            // => GET_PROP/SET_PROP size = 1 + 6 (args) + 12 (cache) = 19 bytes?
            // Ở phiên bản đơn giản hiện tại, ta tạm bỏ qua cache size trong switch này
            // Nhưng nếu Assembler emit cache, Loader phải skip nó.
            
            // Jumps (u16 offset / reg+offset)
            case OpCode::JUMP: ip += 2; break; // Target
            case OpCode::SETUP_TRY: ip += 4; break; // Target + Reg
            case OpCode::JUMP_IF_FALSE: case OpCode::JUMP_IF_TRUE: ip += 4; break; // Reg + Target

            // Bytecode versions (Optimized)
            case OpCode::ADD_B: case OpCode::SUB_B: case OpCode::MUL_B: 
            case OpCode::DIV_B: case OpCode::MOD_B: case OpCode::LT_B:
            case OpCode::JUMP_IF_TRUE_B: case OpCode::JUMP_IF_FALSE_B:
                ip += 3; break;

            default: break;
        }
        
        // Fix đặc biệt cho GET_PROP/SET_PROP nếu dùng Inline Cache
        // Nếu Assembler emit cache (như file asm cậu gửi), ta phải skip thêm 12 bytes
        if (op == OpCode::GET_PROP || op == OpCode::SET_PROP) {
             ip += 12; 
        }
    }
}

void Loader::link_module(module_t module) {
    if (!module || !module->is_has_main()) return;
    std::unordered_set<proto_t> visited;
    // Bắt đầu đệ quy từ main_proto của module
    patch_chunk_globals_recursive(module, module->get_main_proto(), visited);
}

}
#include "bytecode/loader.h"
#include "memory/memory_manager.h"
#include "core/objects/string.h"
#include "core/objects/function.h"
#include "core/value.h"
#include "bytecode/chunk.h"

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

    if constexpr (std::endian::native == std::endian::big) {
        return std::byteswap(val);
    } else {
        return val;
    }
}

double Loader::read_f64() {
    return std::bit_cast<double>(read_u64());
}

string_t Loader::read_string() {
    uint32_t length = read_u32();
    check_can_read(length);
    // TODO: Validate utf-8 here if needed
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
    
    if (name_idx_in_pool >= constants.size() || !constants[name_idx_in_pool].is_string()) {
        throw LoaderError("Invalid function prototype name index (must be a string).");
    }
    string_t name = constants[name_idx_in_pool].as_string();

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
        if (patch.proto_idx >= loaded_protos_.size()) {
            throw LoaderError("Internal Error: Patch parent proto index out of bounds.");
        }
        if (patch.target_idx >= loaded_protos_.size()) {
            throw LoaderError("Invalid prototype reference: Target proto index out of bounds.");
        }

        proto_t parent_proto = loaded_protos_[patch.proto_idx];
        proto_t child_proto = loaded_protos_[patch.target_idx];

        // 3. Truy cập Chunk của cha để sửa Constant Pool
        // Lưu ý: Constant pool trong Chunk không phải const, ta có thể dùng get_constant_ref (cần implement trong Chunk)
        // hoặc const_cast nếu hàm get trả về const reference (nhưng Chunk nên có hàm set/get_ref).
        // MeowVM Chunk hiện tại có: get_constant_ref(size_t index) -> value_t&
        
        Chunk& chunk = const_cast<Chunk&>(parent_proto->get_chunk()); 
        
        if (patch.const_idx >= chunk.get_pool_size()) {
             throw LoaderError("Internal Error: Patch constant index out of bounds.");
        }

        // 4. Vá! Gán đè giá trị null placeholder bằng Proto Object
        chunk.get_constant_ref(patch.const_idx) = Value(child_proto);
    }
}

proto_t Loader::load_module() {
    check_magic();
    
    uint32_t main_proto_index = read_u32();
    uint32_t prototype_count = read_u32();
    
    if (prototype_count == 0) {
        throw LoaderError("No prototypes found in bytecode file.");
    }
    
    loaded_protos_.reserve(prototype_count);
    for (uint32_t i = 0; i < prototype_count; ++i) {
        loaded_protos_.push_back(read_prototype(i));
    }
    
    if (main_proto_index >= loaded_protos_.size()) {
        throw LoaderError("Main prototype index is out of bounds.");
    }
    
    link_prototypes();
    
    return loaded_protos_[main_proto_index];
}

}
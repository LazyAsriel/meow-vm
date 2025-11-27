#include "bytecode/binary_loader.h"
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

BinaryLoader::BinaryLoader(MemoryManager* heap, const std::vector<uint8_t>& data)
    : heap_(heap), data_(data), cursor_(0) {}

void BinaryLoader::check_can_read(size_t bytes) {
    if (cursor_ + bytes > data_.size()) {
        throw BinaryLoaderError("Unexpected end of file. File is truncated or corrupt.");
    }
}

uint8_t BinaryLoader::read_u8() {
    check_can_read(1);
    return data_[cursor_++];
}

uint16_t BinaryLoader::read_u16() {
    check_can_read(2);
    uint16_t val = static_cast<uint16_t>(data_[cursor_]) |
                   (static_cast<uint16_t>(data_[cursor_ + 1]) << 8);
    cursor_ += 2;
    return val;
}

uint32_t BinaryLoader::read_u32() {
    check_can_read(4);
    uint32_t val = static_cast<uint32_t>(data_[cursor_]) |
                   (static_cast<uint32_t>(data_[cursor_ + 1]) << 8) |
                   (static_cast<uint32_t>(data_[cursor_ + 2]) << 16) |
                   (static_cast<uint32_t>(data_[cursor_ + 3]) << 24);
    cursor_ += 4;
    return val;
}

uint64_t BinaryLoader::read_u64() {
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

double BinaryLoader::read_f64() {
    return std::bit_cast<double>(read_u64());
}

string_t BinaryLoader::read_string() {
    uint32_t length = read_u32();
    check_can_read(length);
    // TODO: Validate utf-8 here if needed
    std::string str(reinterpret_cast<const char*>(data_.data() + cursor_), length);
    cursor_ += length;
    return heap_->new_string(str);
}

Value BinaryLoader::read_constant(size_t current_proto_idx, size_t current_const_idx) {
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
            throw BinaryLoaderError("Unknown constant tag in binary file.");
    }
}

proto_t BinaryLoader::read_prototype(size_t current_proto_idx) {
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
        throw BinaryLoaderError("Invalid function prototype name index (must be a string).");
    }
    string_t name = constants[name_idx_in_pool].as_string();

    // Upvalues
    uint32_t upvalue_desc_count = read_u32();
    if (upvalue_desc_count != num_upvalues) {
         throw BinaryLoaderError("Upvalue count mismatch.");
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

void BinaryLoader::check_magic() {
    if (read_u32() != MAGIC_NUMBER) {
        throw BinaryLoaderError("Not a valid Meow bytecode file (magic number mismatch).");
    }
    uint32_t version = read_u32();
    if (version != FORMAT_VERSION) {
        throw BinaryLoaderError(std::format("Bytecode version mismatch. File is v{}, VM supports v{}.", version, FORMAT_VERSION));
    }
}

void BinaryLoader::link_prototypes() {    
    for (const auto& patch : patches_) {
        if (patch.proto_idx >= loaded_protos_.size()) {
            throw BinaryLoaderError("Internal Error: Patch parent proto index out of bounds.");
        }
        if (patch.target_idx >= loaded_protos_.size()) {
            throw BinaryLoaderError("Invalid prototype reference: Target proto index out of bounds.");
        }

        proto_t parent_proto = loaded_protos_[patch.proto_idx];
        proto_t child_proto = loaded_protos_[patch.target_idx];

        // 3. Truy cập Chunk của cha để sửa Constant Pool
        // Lưu ý: Constant pool trong Chunk không phải const, ta có thể dùng get_constant_ref (cần implement trong Chunk)
        // hoặc const_cast nếu hàm get trả về const reference (nhưng Chunk nên có hàm set/get_ref).
        // MeowVM Chunk hiện tại có: get_constant_ref(size_t index) -> value_t&
        
        Chunk& chunk = const_cast<Chunk&>(parent_proto->get_chunk()); 
        
        if (patch.const_idx >= chunk.get_pool_size()) {
             throw BinaryLoaderError("Internal Error: Patch constant index out of bounds.");
        }

        // 4. Vá! Gán đè giá trị null placeholder bằng Proto Object
        chunk.get_constant_ref(patch.const_idx) = Value(child_proto);
    }
}

proto_t BinaryLoader::load_module() {
    check_magic();
    
    uint32_t main_proto_index = read_u32();
    uint32_t prototype_count = read_u32();
    
    if (prototype_count == 0) {
        throw BinaryLoaderError("No prototypes found in bytecode file.");
    }
    
    loaded_protos_.reserve(prototype_count);
    for (uint32_t i = 0; i < prototype_count; ++i) {
        loaded_protos_.push_back(read_prototype(i));
    }
    
    if (main_proto_index >= loaded_protos_.size()) {
        throw BinaryLoaderError("Main prototype index is out of bounds.");
    }
    
    link_prototypes();
    
    return loaded_protos_[main_proto_index];
}

}


// #include "bytecode/binary_loader.h"
// #include "memory/memory_manager.h"
// #include "core/objects/string.h"
// #include "core/objects/function.h"
// #include "core/value.h"
// #include "bytecode/chunk.h"

// #include <bit>      // std::bit_cast, std::endian
// #include <cstring>  // std::memcpy
// #include <format>   // std::format (C++20/23)

// namespace meow {

// // Magic & Version
// constexpr uint32_t MAGIC_NUMBER = 0x4D454F57; // "MEOW"
// constexpr uint32_t FORMAT_VERSION = 1;

// enum class ConstantTag : uint8_t {
//     NULL_T    = 0,
//     INT_T     = 1,
//     FLOAT_T   = 2,
//     STRING_T  = 3,
//     PROTO_REF_T = 4
// };

// // --- Macro Helper để xử lý std::expected gọn gàng ---
// // Nếu expr trả về lỗi, return lỗi ngay lập tức. Nếu thành công, lấy giá trị gán vào var.
// #define MEOW_TRY(var, expr) \
//     auto var##_res = (expr); \
//     if (!var##_res) return std::unexpected(var##_res.error()); \
//     auto var = *var##_res;

// // Phiên bản cho hàm trả về void
// #define MEOW_CHECK(expr) \
//     { auto res = (expr); if (!res) return std::unexpected(res.error()); }


// BinaryLoader::BinaryLoader(MemoryManager* heap, const std::vector<uint8_t>& data)
//     : heap_(heap), data_(data), cursor_(0) {}

// LoaderError BinaryLoader::make_error(const std::string& msg) const {
//     return LoaderError{ msg, cursor_ };
// }

// std::expected<void, LoaderError> BinaryLoader::check_boundary(size_t bytes) const {
//     if (cursor_ + bytes > data_.size()) {
//         return std::unexpected(make_error("Unexpected end of file (Truncated data)."));
//     }
//     return {};
// }

// // --- Primitive Readers ---

// BinaryLoader::ReadResult<uint8_t> BinaryLoader::read_u8() {
//     MEOW_CHECK(check_boundary(1));
//     return data_[cursor_++];
// }

// BinaryLoader::ReadResult<uint16_t> BinaryLoader::read_u16() {
//     MEOW_CHECK(check_boundary(2));
//     uint16_t val = static_cast<uint16_t>(data_[cursor_]) |
//                    (static_cast<uint16_t>(data_[cursor_ + 1]) << 8);
//     cursor_ += 2;
//     return val;
// }

// BinaryLoader::ReadResult<uint32_t> BinaryLoader::read_u32() {
//     MEOW_CHECK(check_boundary(4));
//     uint32_t val = static_cast<uint32_t>(data_[cursor_]) |
//                    (static_cast<uint32_t>(data_[cursor_ + 1]) << 8) |
//                    (static_cast<uint32_t>(data_[cursor_ + 2]) << 16) |
//                    (static_cast<uint32_t>(data_[cursor_ + 3]) << 24);
//     cursor_ += 4;
//     return val;
// }

// BinaryLoader::ReadResult<uint64_t> BinaryLoader::read_u64() {
//     MEOW_CHECK(check_boundary(8));
//     uint64_t val;
//     std::memcpy(&val, &data_[cursor_], 8);
//     cursor_ += 8;

//     if constexpr (std::endian::native == std::endian::big) {
//         val = std::byteswap(val);
//     }
//     return val;
// }

// BinaryLoader::ReadResult<double> BinaryLoader::read_f64() {
//     MEOW_TRY(val_u64, read_u64());
//     return std::bit_cast<double>(val_u64);
// }

// BinaryLoader::ReadResult<string_t> BinaryLoader::read_string() {
//     MEOW_TRY(length, read_u32());
//     MEOW_CHECK(check_boundary(length));

//     // Copy string từ buffer (Có thể thêm validate UTF-8 tại đây nếu cần)
//     std::string str(reinterpret_cast<const char*>(data_.data() + cursor_), length);
//     cursor_ += length;

//     return heap_->new_string(str);
// }

// // --- Complex Readers ---

// BinaryLoader::ReadResult<Value> BinaryLoader::read_constant(size_t current_proto_idx, size_t current_const_idx) {
//     MEOW_TRY(tag_byte, read_u8());
//     auto tag = static_cast<ConstantTag>(tag_byte);

//     switch (tag) {
//         case ConstantTag::NULL_T: return Value(null_t{});
        
//         case ConstantTag::INT_T: {
//             MEOW_TRY(val, read_u64());
//             return Value(static_cast<int64_t>(val));
//         }
//         case ConstantTag::FLOAT_T: {
//             MEOW_TRY(val, read_f64());
//             return Value(val);
//         }
//         case ConstantTag::STRING_T: {
//             MEOW_TRY(val, read_string());
//             return Value(val);
//         }
//         case ConstantTag::PROTO_REF_T: {
//             // Đọc index của Proto con
//             MEOW_TRY(target_proto_index, read_u32());
//             // Lưu lại thông tin để vá sau khi đã load hết danh sách Proto
//             patches_.push_back({current_proto_idx, current_const_idx, target_proto_index});
//             return Value(null_t{}); // Placeholder
//         }
//         default:
//             return std::unexpected(make_error(std::format("Unknown constant tag: {}", tag_byte)));
//     }
// }

// BinaryLoader::ReadResult<proto_t> BinaryLoader::read_prototype(size_t current_proto_idx) {
//     MEOW_TRY(num_regs, read_u32());
//     MEOW_TRY(num_upvals, read_u32());
//     MEOW_TRY(name_idx, read_u32()); // Index của tên hàm trong pool (sẽ được đọc sau)

//     // 1. Đọc Constant Pool
//     MEOW_TRY(pool_size, read_u32());
//     std::vector<Value> constants;
//     constants.reserve(pool_size);

//     for (uint32_t i = 0; i < pool_size; ++i) {
//         MEOW_TRY(val, read_constant(current_proto_idx, i));
//         constants.push_back(val);
//     }

//     // 2. Validate tên hàm
//     if (name_idx >= constants.size() || !constants[name_idx].is_string()) {
//         return std::unexpected(make_error("Invalid prototype name index (not a string)."));
//     }
//     string_t name = constants[name_idx].as_string();

//     // 3. Đọc Upvalue Descriptors
//     MEOW_TRY(upval_count, read_u32());
//     if (upval_count != num_upvals) {
//         return std::unexpected(make_error(std::format("Upvalue count mismatch: Declared {}, Found {}.", num_upvals, upval_count)));
//     }

//     std::vector<UpvalueDesc> upvalue_descs;
//     upvalue_descs.reserve(upval_count);
    
//     for (uint32_t i = 0; i < upval_count; ++i) {
//         MEOW_TRY(is_local_byte, read_u8());
//         MEOW_TRY(index, read_u32());
//         upvalue_descs.emplace_back(is_local_byte == 1, index);
//     }

//     // 4. Đọc Bytecode
//     MEOW_TRY(bytecode_len, read_u32());
//     MEOW_CHECK(check_boundary(bytecode_len));

//     std::vector<uint8_t> bytecode(data_.begin() + cursor_, data_.begin() + cursor_ + bytecode_len);
//     cursor_ += bytecode_len;

//     // Tạo Chunk và Proto Object
//     Chunk chunk(std::move(bytecode), std::move(constants));
//     return heap_->new_proto(num_regs, num_upvals, name, std::move(chunk), std::move(upvalue_descs));
// }

// std::expected<void, LoaderError> BinaryLoader::check_header(uint32_t& out_globals_count) {
//     MEOW_TRY(magic, read_u32());
//     if (magic != MAGIC_NUMBER) {
//         return std::unexpected(make_error("Invalid Magic Number. Not a MeowVM binary."));
//     }

//     MEOW_TRY(version, read_u32());
//     if (version != FORMAT_VERSION) {
//         return std::unexpected(make_error(std::format("Version mismatch: File is v{}, VM supports v{}.", version, FORMAT_VERSION)));
//     }
    
//     // [CẬP NHẬT] Đọc số lượng biến Global đã được Assembler ghi vào
//     MEOW_TRY(globals, read_u32());
//     out_globals_count = globals;

//     return {};
// }

// std::expected<void, LoaderError> BinaryLoader::link_prototypes() {
//     for (const auto& patch : patches_) {
//         // Validate index
//         if (patch.proto_idx >= loaded_protos_.size()) {
//              return std::unexpected(make_error("Internal Error: Patch parent proto index out of bounds."));
//         }
//         if (patch.target_idx >= loaded_protos_.size()) {
//              return std::unexpected(make_error("Invalid prototype reference: Target index out of bounds."));
//         }

//         proto_t parent = loaded_protos_[patch.proto_idx];
//         proto_t child = loaded_protos_[patch.target_idx];

//         // Lấy quyền ghi vào Chunk (Chunk là dữ liệu runtime, có thể sửa đổi khi load)
//         Chunk& chunk = const_cast<Chunk&>(parent->get_chunk());
        
//         if (patch.const_idx >= chunk.get_pool_size()) {
//             return std::unexpected(make_error("Internal Error: Patch constant index out of bounds."));
//         }

//         // Thực hiện vá: Thay thế placeholder bằng function object thực sự
//         chunk.get_constant_ref(patch.const_idx) = Value(child);
//     }
//     return {};
// }

// LoaderResult BinaryLoader::load() {
//     uint32_t globals_count = 0;
//     MEOW_CHECK(check_header(globals_count));

//     MEOW_TRY(main_idx, read_u32());
//     MEOW_TRY(proto_count, read_u32());

//     if (proto_count == 0) {
//         return std::unexpected(make_error("Binary file contains no prototypes."));
//     }

//     loaded_protos_.reserve(proto_count);
//     for (uint32_t i = 0; i < proto_count; ++i) {
//         MEOW_TRY(proto, read_prototype(i));
//         loaded_protos_.push_back(proto);
//     }

//     if (main_idx >= loaded_protos_.size()) {
//         return std::unexpected(make_error("Main prototype index out of bounds."));
//     }

//     // Liên kết các hàm lồng nhau (Closures/Inner functions)
//     MEOW_CHECK(link_prototypes());

//     return ModuleData{ loaded_protos_[main_idx], globals_count };
// }

// } // namespace meow
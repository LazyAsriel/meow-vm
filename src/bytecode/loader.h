#pragma once

#include <vector>
#include <string_view>
#include <meow/common.h>
#include <meow/core/module.h>
#include <meow_zerr.h>

namespace meow {

class MemoryManager;

// Định nghĩa các mã lỗi cụ thể cho Loader
enum class LoaderErrorCode : uint8_t {
    NONE = 0,
    UNEXPECTED_EOF,         // Hết file bất ngờ
    MAGIC_MISMATCH,         // Sai Magic number
    UNSUPPORTED_VERSION,    // Phiên bản không hỗ trợ
    UNKNOWN_CONSTANT_TAG,   // Tag constant không xác định
    INVALID_PROTO_INDEX,    // Index prototype sai
    INVALID_CONST_INDEX,    // Index constant sai
    TOO_MANY_GLOBALS,       // Quá nhiều biến global
    NO_PROTOTYPES_FOUND,    // Không tìm thấy prototype nào
    MAIN_PROTO_INVALID,     // Main prototype invalid
    INTERNAL_ERROR          // Lỗi nội bộ
};

class Loader {
public:
    Loader(MemoryManager* heap, const std::vector<uint8_t>& data, std::string_view filename);
    Result<proto_t, LoaderErrorCode> load_module();
    static Result<void, LoaderErrorCode> link_module(module_t module);

private:
    MemoryManager* heap_;
    const std::vector<uint8_t>& data_;
    size_t cursor_ = 0;
    
    Context ctx_; 
    
    uint32_t file_version_ = 0;

    struct ProtoPatch {
        size_t proto_idx;
        size_t const_idx;
        uint32_t target_idx;
    };
    std::vector<ProtoPatch> patches_;
    std::vector<proto_t> loaded_protos_;

    // --- Helpers ---
    
    Status<LoaderErrorCode> error(LoaderErrorCode code) const;
    Result<void, LoaderErrorCode> check_can_read(size_t bytes);
    
    Result<uint8_t, LoaderErrorCode>  read_u8();
    Result<uint16_t, LoaderErrorCode> read_u16();
    Result<uint32_t, LoaderErrorCode> read_u32();
    Result<uint64_t, LoaderErrorCode> read_u64();
    Result<double, LoaderErrorCode>   read_f64();
    Result<string_t, LoaderErrorCode> read_string();
    
    Result<Value, LoaderErrorCode>    read_constant(size_t current_proto_idx, size_t current_const_idx);
    Result<proto_t, LoaderErrorCode>  read_prototype(size_t current_proto_idx);
    
    Result<void, LoaderErrorCode> check_magic();
    Result<void, LoaderErrorCode> link_prototypes();
};

} // namespace meow
#pragma once

#include "pch.h"
#include <meow/definitions.h>
#include <meow/bytecode/chunk.h>
#include <meow/core/function.h>
#include <meow/core/module.h> // Cần để dùng module_t

namespace meow {

class MemoryManager;

/// Lỗi ném ra khi loading bytecode thất bại
class LoaderError : public std::runtime_error {
public:
    explicit LoaderError(const std::string& msg) : std::runtime_error(msg) {}
};

class Loader {
public:
    /// Constructor nhận vào heap và buffer dữ liệu raw bytecode
    Loader(MemoryManager* heap, const std::vector<uint8_t>& data);

    /// Load main module từ bytecode
    proto_t load_module();

    /// [QUAN TRỌNG] Static Linker: Duyệt và vá (patch) lại bytecode để tối ưu hóa
    /// Chuyển đổi GET_GLOBAL/SET_GLOBAL từ string_lookup sang direct_index O(1)
    static void link_module(module_t module);

private:
    struct Patch {
        size_t proto_idx;      // Proto cha
        size_t const_idx;      // Vị trí constant cần vá
        uint32_t target_idx;   // Index của Proto con
    };

    MemoryManager* heap_;
    const std::vector<uint8_t>& data_;
    size_t cursor_ = 0;

    std::vector<proto_t> loaded_protos_;
    std::vector<Patch> patches_;

    // --- Helper đọc dữ liệu ---
    void check_can_read(size_t bytes);
    uint8_t  read_u8();
    uint16_t read_u16();
    uint32_t read_u32();
    uint64_t read_u64();
    double   read_f64();
    string_t read_string();
    
    // --- Helper load cấu trúc ---
    Value read_constant(size_t current_proto_idx, size_t current_const_idx);
    proto_t read_prototype(size_t current_proto_idx);
    
    void check_magic();
    void link_prototypes();
};

}
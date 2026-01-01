#pragma once

#include "pch.h"
#include <meow/core/module.h>
#include <meow/common.h>
#include <meow_zerr.h> // Sử dụng hệ thống Result/Status

namespace meow {
    class Machine;
    class MemoryManager;
    struct GCVisitor;
}

namespace meow {

// Các mã lỗi đặc thù cho ModuleManager
enum class ModuleErrorCode : uint8_t {
    NONE = 0,
    INVALID_PATH,           // Đường dẫn null hoặc rỗng
    FILE_NOT_FOUND,         // Không tìm thấy file (cả native lẫn bytecode)
    FILE_READ_ERROR,        // Lỗi đọc file (IO)
    
    NATIVE_LOAD_FAILED,     // dlopen/LoadLibrary thất bại
    NATIVE_SYMBOL_MISSING,  // dlsym/GetProcAddress thất bại
    NATIVE_FACTORY_FAILED,  // Hàm tạo module native trả về null
    NATIVE_EXCEPTION,       // Ngoại lệ C++ từ bên trong module native
    
    BYTECODE_LOAD_FAILED,   // Loader thất bại (wrapper cho LoaderErrorCode)
    LINKING_FAILED,         // Lỗi khi link globals
    
    CIRCULAR_DEPENDENCY,    // (Dự phòng) phát hiện vòng lặp import
    INTERNAL_ERROR
};

class ModuleManager {
public:
    explicit ModuleManager(MemoryManager* heap, Machine* vm) noexcept;
    
    // Xóa copy, giữ move
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;
    ModuleManager(ModuleManager&&) = default;
    ModuleManager& operator=(ModuleManager&&) = default;
    ~ModuleManager() = default;

    // Trả về Result chứa module_t hoặc lỗi
    Result<module_t, ModuleErrorCode> load_module(string_t module_path, string_t importer_path);

    inline void reset_cache() noexcept {
        module_cache_.clear();
    }

    inline void add_cache(string_t name, module_t mod) {
        module_cache_[name] = mod;
    }

    void trace(GCVisitor& visitor) const noexcept;

private:
    std::unordered_map<string_t, module_t> module_cache_;
    MemoryManager* heap_;
    Machine* vm_;
    string_t entry_path_;

    // Context dùng để báo lỗi (File ID)
    Context ctx_;

    // Helper tạo lỗi nhanh
    Status<ModuleErrorCode> error(ModuleErrorCode code, uint32_t line = 0, uint32_t col = 0) const;
};

} // namespace meow
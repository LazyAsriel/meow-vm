#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "bytecode/chunk.h"
#include "core/objects/function.h"

namespace meow {
class MemoryManager;
class BinaryLoaderError : public std::runtime_error {
public:
    explicit BinaryLoaderError(const std::string& msg) : std::runtime_error(msg) {}
};

class BinaryLoader {
public:
    BinaryLoader(MemoryManager* heap, const std::vector<uint8_t>& data);
    proto_t load_module();

private:
    // --- Patching Structure ---
    struct Patch {
        size_t proto_idx;      // Proto đang chứa constant cần vá (Parent)
        size_t const_idx;      // Vị trí (index) trong Constant Pool cần vá
        uint32_t target_idx;   // Index của Proto đích (Child) mà nó trỏ tới
    };

    MemoryManager* heap_;
    const std::vector<uint8_t>& data_;
    size_t cursor_ = 0;

    std::vector<proto_t> loaded_protos_;
    std::vector<Patch> patches_;

    // --- Readers ---
    void check_can_read(size_t bytes);
    uint8_t  read_u8();
    uint16_t read_u16();
    uint32_t read_u32();
    uint64_t read_u64();
    double   read_f64();
    string_t read_string();
    
    Value read_constant(size_t current_proto_idx, size_t current_const_idx);
    proto_t read_prototype(size_t current_proto_idx);
    
    void check_magic();
    void link_prototypes();
};

}

// #pragma once

// #include "common/pch.h"
// #include "common/definitions.h"
// #include "bytecode/chunk.h"
// #include "core/objects/function.h"
// #include <expected> // C++23 Power!

// namespace meow {

// class MemoryManager;

// // Cấu trúc báo lỗi chi tiết, không dùng exception
// struct LoaderError {
//     std::string message;
//     size_t offset; // Vị trí byte gây lỗi trong file để dễ debug
// };

// // Cấu trúc dữ liệu trả về sau khi load thành công
// struct ModuleData {
//     proto_t main_function;
//     uint32_t globals_count; // Số lượng biến toàn cục cần cấp phát
// };

// // Alias cho kiểu trả về: Hoặc là ModuleData, hoặc là Lỗi
// using LoaderResult = std::expected<ModuleData, LoaderError>;

// class BinaryLoader {
// public:
//     BinaryLoader(MemoryManager* heap, const std::vector<uint8_t>& data);
    
//     // API chính: Không ném ngoại lệ, trả về Result
//     LoaderResult load();

// private:
//     // --- Internal Types ---
//     struct Patch {
//         size_t proto_idx;      // Parent proto
//         size_t const_idx;      // Vị trí trong Constant Pool cần vá
//         uint32_t target_idx;   // Index của Child Proto
//     };

//     // Helper alias cho các hàm đọc nội bộ
//     template <typename T>
//     using ReadResult = std::expected<T, LoaderError>;

//     MemoryManager* heap_;
//     const std::vector<uint8_t>& data_;
//     size_t cursor_ = 0;

//     std::vector<proto_t> loaded_protos_;
//     std::vector<Patch> patches_;

//     // --- Core Readers ---
//     // Kiểm tra buffer an toàn
//     [[nodiscard]] std::expected<void, LoaderError> check_boundary(size_t bytes) const;
    
//     // Các hàm đọc primitive trả về Result
//     ReadResult<uint8_t>  read_u8();
//     ReadResult<uint16_t> read_u16();
//     ReadResult<uint32_t> read_u32();
//     ReadResult<uint64_t> read_u64();
//     ReadResult<double>   read_f64();
//     ReadResult<string_t> read_string();

//     // Các hàm đọc cấu trúc phức tạp
//     ReadResult<Value>   read_constant(size_t current_proto_idx, size_t current_const_idx);
//     ReadResult<proto_t> read_prototype(size_t current_proto_idx);

//     // Logic liên kết
//     [[nodiscard]] std::expected<void, LoaderError> check_header(uint32_t& out_globals_count);
//     [[nodiscard]] std::expected<void, LoaderError> link_prototypes();
    
//     // Helper tạo lỗi nhanh
//     LoaderError make_error(const std::string& msg) const;
// };

// } // namespace meow
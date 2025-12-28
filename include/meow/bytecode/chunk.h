#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <algorithm> // Cần cho std::upper_bound
#include <meow/common.h>
#include <meow/value.h>

namespace meow {

// Cấu trúc lưu thông tin dòng (Khớp với MASM)
struct LineInfo {
    uint32_t offset;    // Bytecode offset
    uint32_t line;
    uint32_t col;
    uint32_t file_idx;  // Index vào mảng source_files
};

class Chunk {
public:
    Chunk() = default;

    // Constructor mới: Nhận thêm source_files và lines
    Chunk(std::vector<uint8_t>&& code, std::vector<Value>&& constants,
          std::vector<std::string>&& source_files, std::vector<LineInfo>&& lines) noexcept 
        : code_(std::move(code)), 
          constant_pool_(std::move(constants)),
          source_files_(std::move(source_files)),
          lines_(std::move(lines)) {}

    // --- Code modifiers (Giữ nguyên) ---
    inline void write_byte(uint8_t byte) {
        code_.push_back(byte);
    }

    inline void write_u16(uint16_t value) {
        code_.push_back(static_cast<uint8_t>(value & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    inline void write_u32(uint32_t value) {
        code_.push_back(static_cast<uint8_t>(value & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }

    inline void write_u64(uint64_t value) {
        for (int i = 0; i < 8; ++i) 
            code_.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }

    inline void write_f64(double value) {
        write_u64(std::bit_cast<uint64_t>(value));
    }

    // --- Accessors ---
    inline const uint8_t* get_code() const noexcept {
        return code_.data();
    }
    inline size_t get_code_size() const noexcept {
        return code_.size();
    }
    inline bool is_code_empty() const noexcept {
        return code_.empty();
    }

    // --- Constant pool ---
    inline size_t get_pool_size() const noexcept {
        return constant_pool_.size();
    }
    inline bool is_pool_empty() const noexcept {
        return constant_pool_.empty();
    }
    inline size_t add_constant(param_t value) {
        constant_pool_.push_back(value);
        return constant_pool_.size() - 1;
    }
    inline return_t get_constant(size_t index) const noexcept {
        return constant_pool_[index];
    }
    inline value_t& get_constant_ref(size_t index) noexcept {
        return constant_pool_[index];
    }
    inline const Value* get_constants_raw() const noexcept {
        return constant_pool_.data();
    }

    inline bool patch_u16(size_t offset, uint16_t value) noexcept {
        if (offset + 1 >= code_.size()) return false;
        code_[offset] = static_cast<uint8_t>(value & 0xFF);
        code_[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        return true;
    }

    // --- DEBUG INFO HELPERS ---
    
    // Lấy tên file từ index
    inline const std::string* get_file_name(uint32_t idx) const {
        if (idx < source_files_.size()) return &source_files_[idx];
        return nullptr;
    }

    // Tìm thông tin dòng code dựa trên offset (PC)
    // Dùng binary search vì lines_ đã được sắp xếp theo offset từ lúc compile
    inline const LineInfo* get_line_info(size_t offset) const {
        if (lines_.empty()) return nullptr;
        
        // Tìm phần tử đầu tiên có offset LỚN HƠN offset hiện tại
        auto it = std::upper_bound(lines_.begin(), lines_.end(), offset,
            [](size_t val, const LineInfo& info) {
                return val < info.offset;
            });
            
        // Nếu tất cả các mốc offset đều lớn hơn (vô lý nếu file hợp lệ) -> nullptr
        if (it == lines_.begin()) return nullptr;

        // LineInfo đúng là phần tử ngay trước đó (khoảng offset bao trùm)
        return &(*std::prev(it));
    }

private:
    std::vector<uint8_t> code_;
    std::vector<Value> constant_pool_;
    
    // Dữ liệu debug mới
    std::vector<std::string> source_files_;
    std::vector<LineInfo> lines_;
};
}
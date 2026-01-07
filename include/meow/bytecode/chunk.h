#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cassert>
#include <iterator>
#include <utility>
#include <span> 

#include <meow/common.h>
#include <meow/value.h>
#include <meow/bytecode/op_codes.h>

namespace meow {

struct LineInfo {
    uint32_t offset;
    uint32_t line;
    uint32_t col;
    uint32_t file_idx;
};

class Chunk {
public:
    static constexpr size_t PADDING_SIZE = 64;

    Chunk() = default;

    Chunk(std::vector<uint8_t>&& code, std::vector<Value>&& constants,
          std::vector<std::string>&& source_files, std::vector<LineInfo>&& lines) noexcept 
        : code_(std::move(code)), 
          constant_pool_(std::move(constants)),
          source_files_(std::move(source_files)),
          lines_(std::move(lines)) 
    {}

    void write_byte(uint8_t byte) {
        assert(!finalized_ && "Cannot write to a finalized chunk");
        code_.push_back(byte);
    }
    
    void write_u16(uint16_t value) {
        assert(!finalized_);
        code_.push_back(static_cast<uint8_t>(value & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    void write_u32(uint32_t value) {
        assert(!finalized_);
        code_.push_back(static_cast<uint8_t>(value & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }

    void write_u64(uint64_t value) {
        assert(!finalized_);
        for (int i = 0; i < 8; ++i) 
            code_.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }

    void write_f64(double value) {
        write_u64(std::bit_cast<uint64_t>(value));
    }

    void write_opcode(OpCode value) {
        write_byte(std::to_underlying(value));
    }

    size_t add_constant(const Value& value) {
        constant_pool_.push_back(value);
        return constant_pool_.size() - 1;
    }

    template <typename Self>
    [[nodiscard]] auto&& get_constant(this Self&& self, size_t index) noexcept {
        assert(index < self.constant_pool_.size());
        return std::forward<Self>(self).constant_pool_[index];
    }
    
    [[nodiscard]] Value& get_constant_ref(size_t index) noexcept {
        assert(index < constant_pool_.size());
        return constant_pool_[index];
    }

    [[nodiscard]] const uint8_t* get_code() const noexcept {
        return code_.data();
    }
    
    [[nodiscard]] std::span<const uint8_t> get_code_span() const noexcept {
        return {code_.data(), get_code_size()};
    }

    [[nodiscard]] size_t get_code_size() const noexcept {
        if (finalized_ && code_.size() >= PADDING_SIZE) {
            return code_.size() - PADDING_SIZE;
        }
        return code_.size();
    }

    [[nodiscard]] bool is_code_empty() const noexcept {
        return get_code_size() == 0;
    }
    
    [[nodiscard]] size_t get_raw_size() const noexcept { return code_.size(); }

    [[nodiscard]] size_t get_pool_size() const noexcept { return constant_pool_.size(); }
    [[nodiscard]] bool is_pool_empty() const noexcept { return constant_pool_.empty(); }
    [[nodiscard]] const Value* get_constants_raw() const noexcept { return constant_pool_.data(); }

    [[nodiscard]] const std::string* get_file_name(uint32_t idx) const {
        if (idx < source_files_.size()) return &source_files_[idx];
        return nullptr;
    }

    bool patch_u16(size_t offset, uint16_t value) noexcept {
        if (offset + 1 >= get_code_size()) [[unlikely]] return false;
        
        code_[offset] = static_cast<uint8_t>(value & 0xFF);
        code_[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        return true;
    }

    [[nodiscard]] const LineInfo* get_line_info(size_t offset) const {
        if (lines_.empty()) return nullptr;
        auto it = std::upper_bound(lines_.begin(), lines_.end(), offset,
            [](size_t val, const LineInfo& info) { return val < info.offset; });
        
        if (it == lines_.begin()) return nullptr;
        return &(*std::prev(it));
    }

    void finalize() noexcept {
        if (finalized_) return;
        
        code_.insert(code_.end(), PADDING_SIZE, static_cast<uint8_t>(OpCode::HALT));
        
        finalized_ = true;
    }

    void reset() noexcept {
        code_.clear();
        constant_pool_.clear();
        lines_.clear();
        finalized_ = false;
    }

    void unfinalize() noexcept {
        if (!finalized_) return;
        if (code_.size() >= PADDING_SIZE) {
            code_.resize(code_.size() - PADDING_SIZE);
        }
        finalized_ = false;
    }

private:
    std::vector<uint8_t> code_;
    std::vector<Value> constant_pool_;
    std::vector<std::string> source_files_;
    std::vector<LineInfo> lines_;

    bool finalized_ = false;
};

} // namespace meow
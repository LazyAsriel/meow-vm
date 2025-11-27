#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "core/value.h"

namespace meow {
class Chunk {
public:
    Chunk() = default;
    Chunk(std::vector<uint8_t>&& code, std::vector<Value>&& constants) noexcept : code_(std::move(code)), constant_pool_(std::move(constants)) {}

    // --- Modifiers ---
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
        code_.push_back(static_cast<uint8_t>(value & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
    }

    inline void write_f64(double value) {
        write_u64(std::bit_cast<uint64_t>(value));
    }
    // --- Code buffer ---
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

    inline bool patch_u16(size_t offset, uint16_t value) noexcept {
        if (offset + 1 >= code_.size()) return false;

        code_[offset] = static_cast<uint8_t>(value & 0xFF);
        code_[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);

        return true;
    }

private:
    std::vector<uint8_t> code_;
    std::vector<Value> constant_pool_;
};
}
#pragma once
#include <cstdint>

namespace meow {

struct NanboxLayout {
    static constexpr uint64_t EXP_MASK     = 0x7FF0000000000000ULL;
    static constexpr uint64_t TAG_MASK     = 0x0007000000000000ULL;
    static constexpr uint64_t PAYLOAD_MASK = 0x0000FFFFFFFFFFFFULL;
    
    // QNAN_POS: 0111 1111 1111 1...
    static constexpr uint64_t QNAN_POS     = 0x7FF8000000000000ULL; 
    // QNAN_NEG: 1111 1111 1111 1...
    static constexpr uint64_t QNAN_NEG     = 0xFFF8000000000000ULL; 
    
    static constexpr uint64_t VALUELESS    = 0xFFFFFFFFFFFFFFFFULL;

    // --- Shift offset ---
    static constexpr unsigned TAG_SHIFT    = 48;

    [[nodiscard]] static consteval uint64_t make_tag(uint64_t index) noexcept {
        return QNAN_POS | (index << TAG_SHIFT);
    }
};

} // namespace meow
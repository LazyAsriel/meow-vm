#pragma once
#include <cstdint>

namespace meow {

// [STANDARD] Tiêu chuẩn Nanbox 64-bit công khai
// Đây là "Shared Knowledge" cho toàn bộ hệ thống MeowVM (Variant, JIT, GC...)
struct NanboxLayout {
    // --- Các Mask quan trọng ---
    static constexpr uint64_t EXP_MASK     = 0x7FF0000000000000ULL;
    static constexpr uint64_t TAG_MASK     = 0x0007000000000000ULL;
    static constexpr uint64_t PAYLOAD_MASK = 0x0000FFFFFFFFFFFFULL;
    
    // --- Các giá trị đặc biệt (Signaling) ---
    // QNAN_POS: 0111 1111 1111 1...
    static constexpr uint64_t QNAN_POS     = 0x7FF8000000000000ULL; 
    // QNAN_NEG: 1111 1111 1111 1...
    static constexpr uint64_t QNAN_NEG     = 0xFFF8000000000000ULL; 
    
    static constexpr uint64_t VALUELESS    = 0xFFFFFFFFFFFFFFFFULL;

    // --- Shift offset ---
    static constexpr unsigned TAG_SHIFT    = 48;

    // --- Helper tính toán Tag ---
    // index: 0 (null), 1 (bool), 2 (int)...
    [[nodiscard]] static consteval uint64_t make_tag(uint64_t index) {
        // Tag chuẩn = QNAN_POS | (Index << 48)
        return QNAN_POS | (index << TAG_SHIFT);
    }
};

} // namespace meow
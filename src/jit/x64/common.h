/**
 * @file common.h
 * @brief Shared definitions for x64 Backend (Registers, Conditions)
 */

#pragma once

#include <cstdint>

namespace meow::jit::x64 {

    // --- x64 Hardware Registers ---
    enum Reg : uint8_t {
        RAX = 0, RCX = 1, RDX = 2, RBX = 3, 
        RSP = 4, RBP = 5, RSI = 6, RDI = 7,
        R8  = 8, R9  = 9, R10 = 10, R11 = 11,
        R12 = 12, R13 = 13, R14 = 14, R15 = 15,
        INVALID_REG = 0xFF
    };

    // --- CPU Condition Codes (EFLAGS) ---
    enum Condition : uint8_t {
        O  = 0,  NO = 1,  // Overflow
        B  = 2,  AE = 3,  // Below / Above or Equal (Unsigned)
        E  = 4,  NE = 5,  // Equal / Not Equal
        BE = 6,  A  = 7,  // Below or Equal / Above (Unsigned)
        S  = 8,  NS = 9,  // Sign
        P  = 10, NP = 11, // Parity
        L  = 12, GE = 13, // Less / Greater or Equal (Signed)
        LE = 14, G  = 15  // Less or Equal / Greater (Signed)
    };

    // --- MeowVM Calling Convention ---
    // Quy định thanh ghi nào giữ con trỏ quan trọng của VM

    // Thanh ghi chứa con trỏ `VMState*` (được truyền vào từ C++)
    // Theo System V AMD64 ABI (Linux/Mac), tham số đầu tiên là RDI.
    // Theo MS x64 (Windows), tham số đầu tiên là RCX.
    #if defined(_WIN32)
        static constexpr Reg REG_STATE = RCX;
        static constexpr Reg REG_TMP1  = RDX; // Scratch register
    #else
        static constexpr Reg REG_STATE = RDI;
        static constexpr Reg REG_TMP1  = RSI; // Lưu ý: RSI thường dùng cho param 2
    #endif

    // Các thanh ghi Callee-saved (được phép dùng lâu dài, phải restore khi exit)
    // Ta dùng để map các register ảo của VM (r0, r1...)
    static constexpr Reg VM_LOCALS_BASE = RBX; 
    // Chúng ta sẽ định nghĩa map cụ thể trong CodeGen sau.

    // Tagging constants (cho Nan-boxing)
    // Giá trị này phải khớp với meow::Value::NANBOX_INT_TAG
    // Giả sử layout traits tag = 2 (như trong compiler cũ)
    static constexpr uint64_t NANBOX_INT_TAG = 0xFFFE000000000000ULL; // Ví dụ, cần check lại value.h thực tế

} // namespace meow::jit::x64
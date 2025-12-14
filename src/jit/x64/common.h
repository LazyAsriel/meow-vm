#pragma once
#include <cstdint>

namespace meow::jit::x64 {

// Các thanh ghi x64 (theo thứ tự mã hóa phần cứng)
enum Reg : uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8 = 8,  R9 = 9,  R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    INVALID_REG = 0xFF
};

// Điều kiện nhảy (Jcc) và so sánh (SETcc)
enum Condition : uint8_t {
    O = 0x0, NO = 0x1, B = 0x2, AE = 0x3,
    E = 0x4, NE = 0x5, BE = 0x6, A = 0x7,
    S = 0x8, NS = 0x9, P = 0xA, NP = 0xB,
    L = 0xC, GE = 0xD, LE = 0xE, G = 0xF
};

} // namespace meow::jit::x64

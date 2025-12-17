/**
 * @file jit_config.h
 * @brief Configuration constants for MeowVM JIT Compiler
 */

#pragma once

#include <cstddef>

namespace meow::jit {

    // --- Tuning Parameters ---

    // Số lần một hàm/loop được thực thi trước khi kích hoạt JIT (Hot Threshold)
    // Để 0 hoặc 1 nếu muốn JIT luôn chạy (Eager JIT) để test.
    static constexpr size_t JIT_THRESHOLD = 100;

    // Kích thước bộ đệm chứa mã máy (Executable Memory)
    // 1MB là đủ cho rất nhiều code meow nhỏ.
    static constexpr size_t JIT_CACHE_SIZE = 1024 * 1024; 

    // --- Optimization Flags ---

    // Bật tính năng Inline Caching (Tăng tốc truy cập thuộc tính)
    static constexpr bool ENABLE_INLINE_CACHE = true;

    // Bật tính năng Guarded Arithmetic (Cộng trừ nhanh trên số nguyên)
    static constexpr bool ENABLE_INT_FAST_PATH = true;

    // --- Debugging ---

    // In ra mã Assembly (Hex) sau khi compile
    static constexpr bool JIT_DEBUG_LOG = true;

    // In ra thông tin khi Deoptimization xảy ra (fallback về Interpreter)
    static constexpr bool LOG_DEOPT = true;

} // namespace meow::jit
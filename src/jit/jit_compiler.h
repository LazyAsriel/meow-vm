/**
 * @file jit_compiler.h
 * @brief Public API - The main entry point for JIT Compilation
 */

#pragma once

#include "meow/value.h"
#include <cstddef>
#include <cstdint>

// Forward declarations
namespace meow { struct VMState; }

namespace meow::jit {

    // Signature của hàm sau khi đã được JIT
    using JitFunc = void (*)(meow::VMState*);

    class JitCompiler {
    public:
        // Singleton: Chỉ cần 1 trình biên dịch trong suốt vòng đời VM
        static JitCompiler& instance();

        // Chuẩn bị bộ nhớ (mmap, quyền execute...)
        void initialize();

        // Dọn dẹp bộ nhớ khi tắt VM
        void shutdown();

        /**
         * @brief Compile bytecode thành mã máy
         * * @param bytecode Pointer đến mảng bytecode gốc
         * @param length Độ dài
         * @return JitFunc Con trỏ hàm mã máy (hoặc nullptr nếu lỗi/từ chối compile)
         */
        JitFunc compile(const uint8_t* bytecode, size_t length);

    private:
        JitCompiler() = default;
        ~JitCompiler() = default;

        JitCompiler(const JitCompiler&) = delete;
        JitCompiler& operator=(const JitCompiler&) = delete;
    };

} // namespace meow::jit
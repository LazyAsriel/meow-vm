#pragma once

#if defined(__x86_64__) || defined(_M_X64)
    #include "x64/compiler.h"
    namespace meow::jit {
        using Compiler = x64::Compiler;
    }
#elif defined(__aarch64__)
    #error "ARM64 JIT not implemented yet"
#else
    #error "Unsupported architecture for JIT"
#endif
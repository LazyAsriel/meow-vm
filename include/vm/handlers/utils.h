#pragma once

#include "vm/vm_state.h"
#include "bytecode/op_codes.h"
#include "core/value.h"
#include "runtime/operator_dispatcher.h"
#include "runtime/execution_context.h"
#include "runtime/call_frame.h"
#include "core/objects/function.h"
#include "memory/memory_manager.h"
#include "runtime/upvalue.h"
#include "debug/print.h"
#include <cstring>

// Định nghĩa chữ ký chuẩn cho Handler (Argument Threading)
#define HOT_HANDLER [[gnu::always_inline, gnu::hot]] static const uint8_t*

namespace meow {
namespace handlers {

// Helper đọc nhanh (Inline decoder)
// [[gnu::always_inline]]
// inline uint16_t read_u16(const uint8_t*& ip) noexcept {
//     // Dùng reinterpret_cast trực tiếp (x64 hỗ trợ unaligned load tốt)
//     uint16_t val = *reinterpret_cast<const uint16_t*>(ip);
//     ip += 2;
//     return val;
// }

inline uint16_t read_u16(const uint8_t*& ip) noexcept {
    uint16_t val;
    // Copy 2 bytes vào biến val (được align chuẩn)
    std::memcpy(&val, ip, sizeof(uint16_t)); 
    ip += 2;
    return val;
}

} // namespace handlers
} // namespace meow
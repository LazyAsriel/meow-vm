#pragma once

// Include tất cả thư viện cần thiết cho các OpCode
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
#include "common/cast.h"

namespace meow {
namespace handlers {

    // Helper đọc u16 (Luôn inline để tối ưu tốc độ)
    [[gnu::always_inline]]
    inline uint16_t read_u16(const uint8_t*& ip) noexcept {
        uint16_t val = static_cast<uint16_t>(ip[0]) | (static_cast<uint16_t>(ip[1]) << 8);
        ip += 2;
        return val;
    }

} // namespace handlers
} // namespace meow
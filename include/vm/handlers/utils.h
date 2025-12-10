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

#define HOT_HANDLER [[gnu::always_inline, gnu::hot]] static const uint8_t*

namespace meow {
namespace handlers {

[[gnu::always_inline]]
inline uint16_t read_u16(const uint8_t*& ip) noexcept {
    uint16_t val;
    std::memcpy(&val, ip, 2); 
    ip += 2;
    return val;
}

} // namespace handlers
} // namespace meow
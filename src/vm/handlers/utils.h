#pragma once

#include "vm/vm_state.h"
#include <meow/bytecode/op_codes.h>
#include <meow/value.h>
#include <meow/cast.h>
#include "runtime/operator_dispatcher.h"
#include "runtime/execution_context.h"
#include "runtime/call_frame.h"
#include <meow/core/function.h>
#include <meow/memory/memory_manager.h>
#include "runtime/upvalue.h"
#include <cstring>

#define HOT_HANDLER [[gnu::always_inline, gnu::hot, gnu::aligned(32)]] static const uint8_t*

namespace meow {
namespace handlers {

[[gnu::always_inline]]
inline uint16_t read_u16(const uint8_t*& ip) noexcept {
    uint16_t val = *reinterpret_cast<const uint16_t*>(ip);
    ip += 2;
    return val;
}

} // namespace handlers
} // namespace meow
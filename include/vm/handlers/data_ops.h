#pragma once
#include "vm/handlers/utils.h"

namespace meow {
namespace handlers {

    // --- LOAD_CONST ---
    [[gnu::noinline]]
    static const uint8_t* impl_LOAD_CONST(const uint8_t* ip, VMState* state) {
        uint16_t dst = read_u16(ip);
        uint16_t idx = read_u16(ip);
        state->reg(dst) = state->constant(idx);
        return ip;
    }

} // namespace handlers
} // namespace meow
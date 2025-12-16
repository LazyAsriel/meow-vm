#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {

[[gnu::always_inline]] static const uint8_t* impl_THROW(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t reg = read_u16(ip);
    (void)constants;
    Value& val = regs[reg];
    state->error(to_string(val));
    return impl_PANIC(ip, regs, constants, state);
}

[[gnu::always_inline]] static const uint8_t* impl_SETUP_TRY(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t offset = read_u16(ip);
    uint16_t err_reg = read_u16(ip);
    (void)regs; (void)constants;
    
    size_t frame_depth = state->ctx.frame_ptr_ - state->ctx.call_stack_;
    size_t stack_depth = state->ctx.stack_top_ - state->ctx.stack_;
    
    size_t catch_ip_abs = offset; 
    
    state->ctx.exception_handlers_.emplace_back(catch_ip_abs, frame_depth, stack_depth, err_reg);
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_POP_TRY(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    (void)regs; (void)constants;
    if (!state->ctx.exception_handlers_.empty()) {
        state->ctx.exception_handlers_.pop_back();
    }
    return ip;
}

} // namespace meow::handlers
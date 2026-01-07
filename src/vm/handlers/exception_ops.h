#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {

// Mã lỗi giả định cho Exception
constexpr int ERR_RUNTIME = 99;

[[gnu::always_inline]] 
static const uint8_t* impl_THROW(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 1 * u16 = 2 bytes -> Load u16 (hoặc u32 tùy implement decode, nhưng u16 đủ nhanh)
    auto [reg] = decode::args<u16>(ip);
    
    Value& val = regs[reg];
    
    // Sử dụng ERROR<2> để trỏ đúng về lệnh THROW
    // Format string: "{}" để in nội dung exception
    return ERROR<2>(ip, regs, constants, state, ERR_RUNTIME, "Uncaught Exception: {}", to_string(val));
}

[[gnu::always_inline]] 
static const uint8_t* impl_SETUP_TRY(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 2 * u16 = 4 bytes -> Load u32
    auto [offset, err_reg] = decode::args<u16, u16>(ip);
    
    size_t frame_depth = state->ctx.frame_ptr_ - state->ctx.call_stack_;
    size_t stack_depth = state->ctx.stack_top_ - state->ctx.stack_;
    
    size_t catch_ip_abs = offset; 
    
    state->ctx.exception_handlers_.emplace_back(catch_ip_abs, frame_depth, stack_depth, err_reg);
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_POP_TRY(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    if (!state->ctx.exception_handlers_.empty()) {
        state->ctx.exception_handlers_.pop_back();
    }
    return ip;
}

} // namespace meow::handlers
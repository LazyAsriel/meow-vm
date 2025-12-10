#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {

[[gnu::always_inline]] static const uint8_t* impl_THROW(const uint8_t* ip, VMState* state) {
    uint16_t reg = read_u16(ip);
    Value& val = state->reg(reg);
    state->error(to_string(val));
    return impl_PANIC(ip, state);
}

[[gnu::always_inline]] static const uint8_t* impl_SETUP_TRY(const uint8_t* ip, VMState* state) {
    uint16_t offset = read_u16(ip);
    uint16_t err_reg = read_u16(ip);
    
    const uint8_t* code_start = state->ctx.current_frame_->function_->get_proto()->get_chunk().get_code();
    size_t catch_offset = (ip - code_start) + offset - 4; // Trừ đi số byte đã đọc? 
    // Logic trong JUMP dùng read_address (u16) là offset tính từ đầu chunk?
    // Kiểm tra lại logic masm: target là offset tuyệt đối trong chunk
    // Trong file exception.inl cũ: size_t catch_ip = target; (với target là u16 offset)
    
    size_t catch_ip_abs = offset;
    
    size_t frame_depth = state->ctx.call_stack_.size() - 1;
    size_t stack_depth = state->ctx.registers_.size();
    
    state->ctx.exception_handlers_.emplace_back(catch_ip_abs, frame_depth, stack_depth, err_reg);
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_POP_TRY(const uint8_t* ip, VMState* state) {
    if (!state->ctx.exception_handlers_.empty()) {
        state->ctx.exception_handlers_.pop_back();
    }
    return ip;
}

} // namespace meow::handlers
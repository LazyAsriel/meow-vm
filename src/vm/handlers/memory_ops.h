#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {

[[gnu::always_inline]] static const uint8_t* impl_GET_GLOBAL(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t global_idx = read_u16(ip);
        
    regs[dst] = state->current_module->get_global_by_index(global_idx);
    
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_SET_GLOBAL(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t global_idx = read_u16(ip);
    uint16_t src = read_u16(ip);
        
    state->current_module->set_global_by_index(global_idx, regs[src]);
    return ip;
}
[[gnu::always_inline]] static const uint8_t* impl_GET_UPVALUE(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t uv_idx = read_u16(ip);
    (void)constants;
    
    upvalue_t uv = state->ctx.frame_ptr_->function_->get_upvalue(uv_idx);
    if (uv->is_closed()) {
        regs[dst] = uv->get_value();
    } else {
        regs[dst] = state->ctx.stack_[uv->get_index()];
    }
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_SET_UPVALUE(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t uv_idx = read_u16(ip);
    uint16_t src = read_u16(ip);
    (void)constants;

    upvalue_t uv = state->ctx.frame_ptr_->function_->get_upvalue(uv_idx);
    if (uv->is_closed()) {
        uv->close(regs[src]);
    } else {
        state->ctx.stack_[uv->get_index()] = regs[src];
    }
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_CLOSURE(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t proto_idx = read_u16(ip);
    
    // --- [FIX BEGIN] ---
    Value val = constants[proto_idx];
    if (!val.is_proto()) [[unlikely]] {
        state->ctx.current_frame_->ip_ = ip - 5;
        std::println("[DEBUG] CLOSURE {}, {}", dst, proto_idx);
        state->error("CLOSURE: Constant index " + std::to_string(proto_idx) + " is not a Proto (Found: " + to_string(val) + ")");
        return impl_PANIC(ip, regs, constants, state);
    }
    proto_t proto = val.as_proto();
    function_t closure = state->heap.new_function(proto);
    
    size_t current_base_idx = regs - state->ctx.stack_;

    for (size_t i = 0; i < proto->get_num_upvalues(); ++i) {
        const auto& desc = proto->get_desc(i);
        if (desc.is_local_) {
            closure->set_upvalue(i, capture_upvalue(&state->ctx, &state->heap, current_base_idx + desc.index_));
        } else {
            closure->set_upvalue(i, state->ctx.frame_ptr_->function_->get_upvalue(desc.index_));
        }
    }
    regs[dst] = Value(closure);
    return ip;
}
[[gnu::always_inline]] static const uint8_t* impl_CLOSE_UPVALUES(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t last_reg = read_u16(ip);
    (void)constants;
    
    size_t current_base_idx = regs - state->ctx.stack_;
    close_upvalues(&state->ctx, current_base_idx + last_reg);
    return ip;
}

} // namespace meow::handlers
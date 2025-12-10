#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {

[[gnu::always_inline]] static const uint8_t* impl_GET_GLOBAL(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    string_t name = state->constant(name_idx).as_string();
    
    // [FIX] Dùng frame_ptr_ thay vì current_frame_
    module_t mod = state->ctx.frame_ptr_->module_;
    if (mod->has_global(name)) {
        state->reg(dst) = mod->get_global(name);
    } else {
        state->reg(dst) = Value(null_t{});
    }
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_SET_GLOBAL(const uint8_t* ip, VMState* state) {
    uint16_t name_idx = read_u16(ip);
    uint16_t src = read_u16(ip);
    string_t name = state->constant(name_idx).as_string();
    
    // [FIX] Dùng frame_ptr_
    state->ctx.frame_ptr_->module_->set_global(name, state->reg(src));
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_GET_UPVALUE(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t uv_idx = read_u16(ip);
    
    // [FIX] Dùng frame_ptr_
    upvalue_t uv = state->ctx.frame_ptr_->function_->get_upvalue(uv_idx);
    if (uv->is_closed()) {
        state->reg(dst) = uv->get_value();
    } else {
        // [FIX] Truy cập stack_ thay vì registers_
        state->reg(dst) = state->ctx.stack_[uv->get_index()];
    }
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_SET_UPVALUE(const uint8_t* ip, VMState* state) {
    uint16_t uv_idx = read_u16(ip);
    uint16_t src = read_u16(ip);
    
    upvalue_t uv = state->ctx.frame_ptr_->function_->get_upvalue(uv_idx);
    if (uv->is_closed()) {
        uv->close(state->reg(src));
    } else {
        // [FIX] Truy cập stack_
        state->ctx.stack_[uv->get_index()] = state->reg(src);
    }
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_CLOSURE(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t proto_idx = read_u16(ip);
    
    proto_t proto = state->constant(proto_idx).as_proto();
    function_t closure = state->heap.new_function(proto);
    
    // [FIX] Tính index cơ sở của frame hiện tại
    size_t current_base_idx = state->ctx.current_regs_ - state->ctx.stack_;

    for (size_t i = 0; i < proto->get_num_upvalues(); ++i) {
        const auto& desc = proto->get_desc(i);
        if (desc.is_local_) {
            // [FIX] Dùng index tính toán được
            closure->set_upvalue(i, capture_upvalue(&state->ctx, &state->heap, current_base_idx + desc.index_));
        } else {
            closure->set_upvalue(i, state->ctx.frame_ptr_->function_->get_upvalue(desc.index_));
        }
    }
    state->reg(dst) = Value(closure);
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_CLOSE_UPVALUES(const uint8_t* ip, VMState* state) {
    uint16_t last_reg = read_u16(ip);
    // [FIX] Tính index tuyệt đối
    size_t current_base_idx = state->ctx.current_regs_ - state->ctx.stack_;
    close_upvalues(&state->ctx, current_base_idx + last_reg);
    return ip;
}

} // namespace meow::handlers
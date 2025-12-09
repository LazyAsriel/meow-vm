#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {

[[gnu::always_inline]] static const uint8_t* impl_GET_GLOBAL(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    string_t name = state->constant(name_idx).as_string();
    
    module_t mod = state->ctx.current_frame_->module_;
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
    
    state->ctx.current_frame_->module_->set_global(name, state->reg(src));
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_GET_UPVALUE(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t uv_idx = read_u16(ip);
    
    upvalue_t uv = state->ctx.current_frame_->function_->get_upvalue(uv_idx);
    if (uv->is_closed()) {
        state->reg(dst) = uv->get_value();
    } else {
        // Lấy giá trị từ stack (vẫn còn sống)
        state->reg(dst) = state->ctx.registers_[uv->get_index()];
    }
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_SET_UPVALUE(const uint8_t* ip, VMState* state) {
    uint16_t uv_idx = read_u16(ip);
    uint16_t src = read_u16(ip);
    
    upvalue_t uv = state->ctx.current_frame_->function_->get_upvalue(uv_idx);
    if (uv->is_closed()) {
        uv->close(state->reg(src));
    } else {
        state->ctx.registers_[uv->get_index()] = state->reg(src);
    }
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_CLOSURE(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t proto_idx = read_u16(ip);
    
    proto_t proto = state->constant(proto_idx).as_proto();
    function_t closure = state->heap.new_function(proto);
    
    for (size_t i = 0; i < proto->get_num_upvalues(); ++i) {
        const auto& desc = proto->get_desc(i);
        if (desc.is_local_) {
            // Capture biến local từ frame hiện tại
            closure->set_upvalue(i, capture_upvalue(&state->ctx, &state->heap, state->ctx.current_base_ + desc.index_));
        } else {
            // Capture lại từ upvalue của hàm cha
            closure->set_upvalue(i, state->ctx.current_frame_->function_->get_upvalue(desc.index_));
        }
    }
    state->reg(dst) = Value(closure);
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_CLOSE_UPVALUES(const uint8_t* ip, VMState* state) {
    uint16_t last_reg = read_u16(ip);
    close_upvalues(&state->ctx, state->ctx.current_base_ + last_reg);
    return ip;
}

} // namespace meow::handlers
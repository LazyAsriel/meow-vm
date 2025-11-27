#pragma once
// Chứa các handler cho Global, Upvalue, Closure

inline void Machine::op_get_global(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    uint16_t name_idx = READ_U16();
    string_t name = CONSTANT(name_idx).as_string();
    module_t module = context_->current_frame_->module_;
    if (module->has_global(name)) {
        REGISTER(dst) = module->get_global(name);
    } else {
        REGISTER(dst) = Value(null_t{});
    }
}

inline void Machine::op_set_global(const uint8_t*& ip) {
    uint16_t name_idx = READ_U16();
    uint16_t src = READ_U16();
    string_t name = CONSTANT(name_idx).as_string();
    module_t module = context_->current_frame_->module_;
    module->set_global(name, REGISTER(src));
}

inline void Machine::op_get_upvalue(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    uint16_t uv_idx = READ_U16();
    upvalue_t uv = context_->current_frame_->function_->get_upvalue(uv_idx);
    if (uv->is_closed()) {
        REGISTER(dst) = uv->get_value();
    } else {
        REGISTER(dst) = context_->registers_[uv->get_index()];
    }
}

inline void Machine::op_set_upvalue(const uint8_t*& ip) {
    uint16_t uv_idx = READ_U16();
    uint16_t src = READ_U16();
    upvalue_t uv = context_->current_frame_->function_->get_upvalue(uv_idx);
    if (uv->is_closed()) {
        uv->close(REGISTER(src));
    } else {
        context_->registers_[uv->get_index()] = REGISTER(src);
    }
}

inline void Machine::op_closure(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    uint16_t proto_idx = READ_U16();
    proto_t proto = CONSTANT(proto_idx).as_proto();
    function_t closure = heap_->new_function(proto);
    for (size_t i = 0; i < proto->get_num_upvalues(); ++i) {
        const auto& desc = proto->get_desc(i);
        if (desc.is_local_) {
            closure->set_upvalue(i, capture_upvalue(context_.get(), heap_.get(), context_->current_base_ + desc.index_));
        } else {
            closure->set_upvalue(i, context_->current_frame_->function_->get_upvalue(desc.index_));
        }
    }
    REGISTER(dst) = Value(closure);
}

inline void Machine::op_close_upvalues(const uint8_t*& ip) {
    uint16_t last_reg = READ_U16();
    close_upvalues(context_.get(), context_->current_base_ + last_reg);
}
#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {

[[gnu::always_inline]] static const uint8_t* impl_NEW_CLASS(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    
    string_t name = state->constant(name_idx).as_string();
    state->reg(dst) = Value(state->heap.new_class(name));
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_NEW_INSTANCE(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t class_reg = read_u16(ip);
    
    Value& class_val = state->reg(class_reg);
    if (!class_val.is_class()) {
        state->error("NEW_INSTANCE: Toán hạng không phải là Class.");
        return impl_PANIC(ip, state);
    }
    state->reg(dst) = Value(state->heap.new_instance(class_val.as_class()));
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_GET_PROP(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    
    Value& obj = state->reg(obj_reg);
    string_t name = state->constant(name_idx).as_string();
    
    if (obj.is_instance()) {
        instance_t inst = obj.as_instance();
        if (inst->has_field(name)) {
            state->reg(dst) = inst->get_field(name);
            return ip;
        }
        // Tìm method trong class và superclass
        class_t k = inst->get_class();
        while (k) {
            if (k->has_method(name)) {
                state->reg(dst) = Value(state->heap.new_bound_method(inst, k->get_method(name).as_function()));
                return ip;
            }
            k = k->get_super();
        }
    }
    else if (obj.is_module()) {
        module_t mod = obj.as_module();
        if (mod->has_export(name)) {
            state->reg(dst) = mod->get_export(name);
            return ip;
        }
    }
    
    // Property not found -> Return null (hoặc có thể ném lỗi tùy design)
    state->reg(dst) = Value(null_t{});
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_SET_PROP(const uint8_t* ip, VMState* state) {
    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    uint16_t val_reg = read_u16(ip);
    
    Value& obj = state->reg(obj_reg);
    string_t name = state->constant(name_idx).as_string();
    Value& val = state->reg(val_reg);
    
    if (obj.is_instance()) {
        obj.as_instance()->set_field(name, val);
    } else {
        state->error("SET_PROP: Chỉ có thể gán thuộc tính cho Instance.");
        return impl_PANIC(ip, state);
    }
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_SET_METHOD(const uint8_t* ip, VMState* state) {
    uint16_t class_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    uint16_t method_reg = read_u16(ip);
    
    Value& class_val = state->reg(class_reg);
    string_t name = state->constant(name_idx).as_string();
    Value& method_val = state->reg(method_reg);
    
    if (!class_val.is_class()) {
        state->error("SET_METHOD: Đích không phải là Class.");
        return impl_PANIC(ip, state);
    }
    if (!method_val.is_function()) {
        state->error("SET_METHOD: Giá trị không phải là Function.");
        return impl_PANIC(ip, state);
    }
    class_val.as_class()->set_method(name, method_val);
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_INHERIT(const uint8_t* ip, VMState* state) {
    uint16_t sub_reg = read_u16(ip);
    uint16_t super_reg = read_u16(ip);
    
    Value& sub_val = state->reg(sub_reg);
    Value& super_val = state->reg(super_reg);
    
    if (!sub_val.is_class() || !super_val.is_class()) {
        state->error("INHERIT: Cả 2 toán hạng phải là Class.");
        return impl_PANIC(ip, state);
    }
    sub_val.as_class()->set_super(super_val.as_class());
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_GET_SUPER(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    string_t name = state->constant(name_idx).as_string();
    
    // Giả định: 'super' luôn được gọi trong method, và receiver (this) nằm ở R0
    // (Đây là quy ước của compiler)
    Value& receiver_val = state->reg(0); 
    if (!receiver_val.is_instance()) {
        state->error("GET_SUPER: 'super' chỉ dùng được trong method của instance.");
        return impl_PANIC(ip, state);
    }
    
    instance_t receiver = receiver_val.as_instance();
    class_t klass = receiver->get_class();
    class_t super = klass->get_super();
    
    if (!super) {
        state->error("GET_SUPER: Class không có superclass.");
        return impl_PANIC(ip, state);
    }
    
    // Tìm method trên chuỗi thừa kế của super
    class_t k = super;
    while (k) {
        if (k->has_method(name)) {
            Value method_val = k->get_method(name);
            if (!method_val.is_function()) break; // Lỗi
            state->reg(dst) = Value(state->heap.new_bound_method(receiver, method_val.as_function()));
            return ip;
        }
        k = k->get_super();
    }
    
    state->error("GET_SUPER: Không tìm thấy method '" + std::string(name->c_str()) + "' ở superclass.");
    return impl_PANIC(ip, state);
}

} // namespace meow::handlers
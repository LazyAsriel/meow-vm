#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {

[[gnu::always_inline]] static const uint8_t* impl_NEW_CLASS(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    
    string_t name = constants[name_idx].as_string();
    regs[dst] = Value(state->heap.new_class(name));
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_NEW_INSTANCE(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t class_reg = read_u16(ip);
    (void)constants;
    
    Value& class_val = regs[class_reg];
    if (!class_val.is_class()) {
        state->error("NEW_INSTANCE: Toán hạng không phải là Class.");
        return impl_PANIC(ip, regs, constants, state);
    }
    // [FIX] Khởi tạo Instance với Shape rỗng
    regs[dst] = Value(state->heap.new_instance(class_val.as_class(), state->heap.get_empty_shape()));
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_GET_PROP(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    
    Value& obj = regs[obj_reg];
    string_t name = constants[name_idx].as_string();
    
    if (obj.is_instance()) {
        instance_t inst = obj.as_instance();
        
        // [FAST PATH] Tìm offset từ Shape
        int offset = inst->get_shape()->get_offset(name);
        if (offset != -1) {
            regs[dst] = inst->get_field_at(offset); // Array access O(1)
            return ip;
        }

        // [SLOW PATH] Tìm method trong Class
        class_t k = inst->get_class();
        while (k) {
            if (k->has_method(name)) {
                regs[dst] = Value(state->heap.new_bound_method(inst, k->get_method(name).as_function()));
                return ip;
            }
            k = k->get_super();
        }
    }
    else if (obj.is_module()) {
        module_t mod = obj.as_module();
        if (mod->has_export(name)) {
            regs[dst] = mod->get_export(name);
            return ip;
        }
    }
    
    regs[dst] = Value(null_t{});
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_SET_PROP(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    uint16_t val_reg = read_u16(ip);
    
    Value& obj = regs[obj_reg];
    string_t name = constants[name_idx].as_string();
    Value& val = regs[val_reg];
    
    if (obj.is_instance()) {
        instance_t inst = obj.as_instance();
        
        // 1. Kiểm tra xem property đã có chưa
        int offset = inst->get_shape()->get_offset(name);

        if (offset != -1) {
            // [UPDATE] Property đã tồn tại -> Ghi đè vào mảng (Siêu nhanh)
            inst->set_field_at(offset, val);
        } else {
            // [TRANSITION] Property mới -> Cần đổi Shape
            Shape* current_shape = inst->get_shape();
            Shape* next_shape = current_shape->get_transition(name);

            if (next_shape == nullptr) {
                // Chưa có đường đi, tạo đường mới
                next_shape = current_shape->add_transition(name, &state->heap);
            }

            // Chuyển sang Shape mới và mở rộng mảng lưu trữ
            inst->set_shape(next_shape);
            inst->get_fields_raw().push_back(val);
        }
    } else {
        state->error("SET_PROP: Chỉ có thể gán thuộc tính cho Instance.");
        return impl_PANIC(ip, regs, constants, state);
    }
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_SET_METHOD(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t class_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    uint16_t method_reg = read_u16(ip);
    
    Value& class_val = regs[class_reg];
    string_t name = constants[name_idx].as_string();
    Value& method_val = regs[method_reg];
    
    if (!class_val.is_class()) {
        state->error("SET_METHOD: Đích không phải là Class.");
        return impl_PANIC(ip, regs, constants, state);
    }
    if (!method_val.is_function()) {
        state->error("SET_METHOD: Giá trị không phải là Function.");
        return impl_PANIC(ip, regs, constants, state);
    }
    class_val.as_class()->set_method(name, method_val);
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_INHERIT(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t sub_reg = read_u16(ip);
    uint16_t super_reg = read_u16(ip);
    (void)constants;
    
    Value& sub_val = regs[sub_reg];
    Value& super_val = regs[super_reg];
    
    if (!sub_val.is_class() || !super_val.is_class()) {
        state->error("INHERIT: Cả 2 toán hạng phải là Class.");
        return impl_PANIC(ip, regs, constants, state);
    }
    sub_val.as_class()->set_super(super_val.as_class());
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_GET_SUPER(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    string_t name = constants[name_idx].as_string();
    
    Value& receiver_val = regs[0]; 
    if (!receiver_val.is_instance()) {
        state->error("GET_SUPER: 'super' chỉ dùng được trong method của instance.");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    instance_t receiver = receiver_val.as_instance();
    class_t klass = receiver->get_class();
    class_t super = klass->get_super();
    
    if (!super) {
        state->error("GET_SUPER: Class không có superclass.");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    class_t k = super;
    while (k) {
        if (k->has_method(name)) {
            Value method_val = k->get_method(name);
            if (!method_val.is_function()) break; 
            regs[dst] = Value(state->heap.new_bound_method(receiver, method_val.as_function()));
            return ip;
        }
        k = k->get_super();
    }
    
    state->error("GET_SUPER: Không tìm thấy method '" + std::string(name->c_str()) + "' ở superclass.");
    return impl_PANIC(ip, regs, constants, state);
}

} // namespace meow::handlers
#pragma once
// Chứa các handler cho Class, Instance, Prop, Method, Inherit, Super

inline void Machine::op_new_class(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    uint16_t name_idx = READ_U16();
    string_t name = CONSTANT(name_idx).as_string();
    REGISTER(dst) = Value(heap_->new_class(name));
}

inline void Machine::op_new_instance(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    uint16_t class_reg = READ_U16();
    Value& class_val = REGISTER(class_reg);
    if (!class_val.is_class()) throw_vm_error("NEW_INSTANCE: operand is not a class.");
    REGISTER(dst) = Value(heap_->new_instance(class_val.as_class()));
}

inline void Machine::op_get_prop(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    uint16_t obj_reg = READ_U16();
    uint16_t name_idx = READ_U16();
    Value& obj = REGISTER(obj_reg);
    string_t name = CONSTANT(name_idx).as_string();
    if (obj.is_instance()) {
        instance_t inst = obj.as_instance();
        if (inst->has_field(name)) {
            REGISTER(dst) = inst->get_field(name);
            return;
        }
        class_t k = inst->get_class();
        while (k) {
            if (k->has_method(name)) {
                REGISTER(dst) = Value(heap_->new_bound_method(inst, k->get_method(name).as_function()));
                return;
            }
            k = k->get_super();
        }
    }
    if (obj.is_module()) {
        module_t mod = obj.as_module();
        if (mod->has_export(name)) {
            REGISTER(dst) = mod->get_export(name);
            return;
        }
    }
    REGISTER(dst) = Value(null_t{});
}

inline void Machine::op_set_prop(const uint8_t*& ip) {
    uint16_t obj_reg = READ_U16();
    uint16_t name_idx = READ_U16();
    uint16_t val_reg = READ_U16();
    Value& obj = REGISTER(obj_reg);
    string_t name = CONSTANT(name_idx).as_string();
    Value& val = REGISTER(val_reg);
    if (obj.is_instance()) {
        obj.as_instance()->set_field(name, val);
    } else {
        throw_vm_error("SET_PROP: can only set properties on instances.");
    }
}

inline void Machine::op_set_method(const uint8_t*& ip) {
    uint16_t call_reg = READ_U16();
    uint16_t name_idx = READ_U16();
    uint16_t method_reg = READ_U16();
    Value& class_val = REGISTER(call_reg);
    string_t name = CONSTANT(name_idx).as_string();
    Value& methodVal = REGISTER(method_reg);
    if (!class_val.is_class()) throw_vm_error("SET_METHOD: target is not a class.");
    if (!methodVal.is_function()) throw_vm_error("SET_METHOD: value is not a function.");
    class_val.as_class()->set_method(name, methodVal);
}

inline void Machine::op_inherit(const uint8_t*& ip) {
    uint16_t sub_reg = READ_U16();
    uint16_t super_reg = READ_U16();
    Value& sub_val = REGISTER(sub_reg);
    Value& super_val = REGISTER(super_reg);
    if (!sub_val.is_class() || !super_val.is_class()) {
        throw_vm_error("INHERIT: Toán hạng phải là class.");
    }
    class_t sub = sub_val.as_class();
    class_t super = super_val.as_class();
    sub->set_super(super);
}

inline void Machine::op_get_super(const uint8_t*& ip) {
    uint16_t dst = READ_U16(), name_idx = READ_U16();
    string_t name = CONSTANT(name_idx).as_string();
    Value& receiver_val = REGISTER(0);
    if (!receiver_val.is_instance()) {
        throw_vm_error("GET_SUPER: 'super' phải được dùng bên trong một method.");
    }
    instance_t receiver = receiver_val.as_instance();
    class_t klass = receiver->get_class();
    class_t super = klass->get_super();
    if (super == nullptr) {
        throw_vm_error("GET_SUPER: Class không có superclass.");
    }
    class_t k = super;
    while (k) {
        if (k->has_method(name)) {
            Value method_val = k->get_method(name);
            if (!method_val.is_function()) {
                throw_vm_error("GET_SUPER: Thành viên của superclass không phải là function.");
            }
            REGISTER(dst) = Value(heap_->new_bound_method(receiver, method_val.as_function()));
            return;
        }
        k = k->get_super();
    }
    throw_vm_error("GET_SUPER: Superclass không có method tên là '" + std::string(name->c_str()) + "'.");
}
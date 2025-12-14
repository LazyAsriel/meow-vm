#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include <meow/core/shape.h>
#include <meow/core/module.h>
#include <module/module_manager.h>

namespace meow::handlers {

static constexpr int IC_CAPACITY = 4;

struct InlineCacheEntry {
    const Shape* shape;
    uint32_t offset;
};

struct InlineCache {
    InlineCacheEntry entries[IC_CAPACITY];
};

[[gnu::always_inline]]
inline static InlineCache* get_inline_cache(const uint8_t*& ip) {
    auto* ic = reinterpret_cast<InlineCache*>(const_cast<uint8_t*>(ip));
    ip += sizeof(InlineCache); // Skip cache data
    return ic;
}

inline static void update_inline_cache(InlineCache* ic, const Shape* shape, uint32_t offset) {
    for (int i = 0; i < IC_CAPACITY; ++i) {
        if (ic->entries[i].shape == shape) {
            if (i > 0) {
                InlineCacheEntry temp = ic->entries[i];
                std::memmove(&ic->entries[1], &ic->entries[0], i * sizeof(InlineCacheEntry));
                ic->entries[0] = temp;
                ic->entries[0].offset = offset;
            }
            return;
        }
    }

    std::memmove(&ic->entries[1], &ic->entries[0], (IC_CAPACITY - 1) * sizeof(InlineCacheEntry));
    ic->entries[0].shape = shape;
    ic->entries[0].offset = offset;
}

static inline Value find_primitive_method(VMState* state, const Value& obj, string_t name) {
    const char* mod_name = nullptr;
    
    if (obj.is_array()) mod_name = "array";
    else if (obj.is_string()) mod_name = "string";
    else if (obj.is_hash_table()) mod_name = "object";
    
    if (mod_name) {
        module_t mod = state->modules.load_module(state->heap.new_string(mod_name), nullptr);
        if (mod && mod->has_export(name)) {
            return mod->get_export(name);
        }
    }
    return Value(null_t{});
}

[[gnu::always_inline]] 
static const uint8_t* impl_NEW_CLASS(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    string_t name = constants[name_idx].as_string();
    regs[dst] = Value(state->heap.new_class(name));
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_NEW_INSTANCE(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t class_reg = read_u16(ip);
    
    Value& class_val = regs[class_reg];
    if (!class_val.is_class()) [[unlikely]] {
        state->error("NEW_INSTANCE: Toán hạng không phải là Class.");
        return impl_PANIC(ip, regs, constants, state);
    }
    regs[dst] = Value(state->heap.new_instance(class_val.as_class(), state->heap.get_empty_shape()));
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_GET_PROP(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    
    InlineCache* ic = get_inline_cache(ip);
    Value& obj = regs[obj_reg];
    
    if (obj.is_instance()) [[likely]] {
        instance_t inst = obj.as_instance();
        Shape* current_shape = inst->get_shape();

        for (int i = 0; i < IC_CAPACITY; ++i) {
            if (ic->entries[i].shape == current_shape) {
                regs[dst] = inst->get_field_at(ic->entries[i].offset);
                return ip;
            }
        }

        string_t name = constants[name_idx].as_string();

        int offset = current_shape->get_offset(name);
        if (offset != -1) {
            update_inline_cache(ic, current_shape, static_cast<uint32_t>(offset));
            regs[dst] = inst->get_field_at(offset);
            return ip;
        }

        class_t k = inst->get_class();
        while (k) {
            if (k->has_method(name)) {
                // Tạo BoundMethod (gói instance + function)
                regs[dst] = Value(state->heap.new_bound_method(inst, k->get_method(name).as_function()));
                return ip;
            }
            k = k->get_super();
        }
    }
    else if (obj.is_module()) {
        string_t name = constants[name_idx].as_string();
        module_t mod = obj.as_module();
        if (mod->has_export(name)) {
            regs[dst] = mod->get_export(name);
            return ip;
        }
    }
    else {
        string_t name = constants[name_idx].as_string();
        Value method = find_primitive_method(state, obj, name);
        if (!method.is_null()) {
            regs[dst] = method;
            return ip;
        }
    }
    
    regs[dst] = Value(null_t{});
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_SET_PROP(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    uint16_t val_reg = read_u16(ip);
    
    InlineCache* ic = get_inline_cache(ip);
    Value& obj = regs[obj_reg];
    Value& val = regs[val_reg];
    
    if (obj.is_instance()) [[likely]] {
        instance_t inst = obj.as_instance();
        Shape* current_shape = inst->get_shape();

        for (int i = 0; i < IC_CAPACITY; ++i) {
            if (ic->entries[i].shape == current_shape) {
                inst->set_field_at(ic->entries[i].offset, val);
           
                state->heap.write_barrier(inst, val);
                return ip;
            }
        }

        string_t name = constants[name_idx].as_string();
        int offset = current_shape->get_offset(name);

        if (offset != -1) {
            update_inline_cache(ic, current_shape, static_cast<uint32_t>(offset));
            inst->set_field_at(offset, val);
            state->heap.write_barrier(inst, val);
        } 
        else {
            Shape* next_shape = current_shape->get_transition(name);
            if (next_shape == nullptr) {
                next_shape = current_shape->add_transition(name, &state->heap);
            }

            inst->set_shape(next_shape);
            inst->get_fields_raw().push_back(val);
            
            state->heap.write_barrier(inst, val);
        }
    } else {
        state->error("SET_PROP: Chỉ có thể gán thuộc tính cho Instance.");
        return impl_PANIC(ip, regs, constants, state);
    }
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_SET_METHOD(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t class_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    uint16_t method_reg = read_u16(ip);
    
    Value& class_val = regs[class_reg];
    string_t name = constants[name_idx].as_string();
    Value& method_val = regs[method_reg];
    
    if (!class_val.is_class()) [[unlikely]] {
        state->error("SET_METHOD: Đích không phải là Class.");
        return impl_PANIC(ip, regs, constants, state);
    }
    // Chấp nhận cả Function và Native Function làm method
    if (!method_val.is_function() && !method_val.is_native()) [[unlikely]] {
        state->error("SET_METHOD: Giá trị phải là Function hoặc Native.");
        return impl_PANIC(ip, regs, constants, state);
    }
    class_val.as_class()->set_method(name, method_val);
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_INHERIT(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t sub_reg = read_u16(ip);
    uint16_t super_reg = read_u16(ip);
    (void)constants;
    
    Value& sub_val = regs[sub_reg];
    Value& super_val = regs[super_reg];
    
    if (!sub_val.is_class() || !super_val.is_class()) [[unlikely]] {
        state->error("INHERIT: Cả 2 toán hạng phải là Class.");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    sub_val.as_class()->set_super(super_val.as_class());
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_GET_SUPER(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    string_t name = constants[name_idx].as_string();
    
    Value& receiver_val = regs[0]; 
    
    if (!receiver_val.is_instance()) [[unlikely]] {
        state->error("GET_SUPER: 'super' chỉ hợp lệ trong method của instance.");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    instance_t receiver = receiver_val.as_instance();
    class_t klass = receiver->get_class();
    class_t super = klass->get_super();
    
    if (!super) {
        state->error("GET_SUPER: Class hiện tại không có superclass.");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    // Lookup method trên chuỗi thừa kế của cha
    class_t k = super;
    while (k) {
        if (k->has_method(name)) {
            Value method_val = k->get_method(name);
            // Bind instance hiện tại với method của cha
            regs[dst] = Value(state->heap.new_bound_method(receiver, method_val.as_function()));
            return ip;
        }
        k = k->get_super();
    }
    
    state->error("GET_SUPER: Không tìm thấy method '" + std::string(name->c_str()) + "' ở superclass.");
    return impl_PANIC(ip, regs, constants, state);
}

} // namespace meow::handlers
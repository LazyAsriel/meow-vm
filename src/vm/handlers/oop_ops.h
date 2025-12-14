#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include <meow/core/shape.h>
#include <meow/core/module.h>
#include <module/module_manager.h>
#include <iostream> 
#include <format>

namespace meow::handlers {

static constexpr int IC_CAPACITY = 4;

struct InlineCacheEntry {
    const Shape* shape;
    uint32_t offset;
} __attribute__((packed));

struct InlineCache {
    InlineCacheEntry entries[IC_CAPACITY];
} __attribute__((packed));

[[gnu::always_inline]]
inline static InlineCache* get_inline_cache(const uint8_t*& ip) {
    auto* ic = reinterpret_cast<InlineCache*>(const_cast<uint8_t*>(ip));
    ip += sizeof(InlineCache); 
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

// --- HANDLERS ---

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
    const uint8_t* start_ip = ip - 1;

    uint16_t dst = read_u16(ip);
    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    
    InlineCache* ic = get_inline_cache(ip); 
    Value& obj = regs[obj_reg];
    string_t name = constants[name_idx].as_string();

    if (obj.is_null()) [[unlikely]] {
        state->ctx.current_frame_->ip_ = start_ip;
        state->error(std::format("Runtime Error: Cannot read property '{}' of null.", name->c_str()));
        return impl_PANIC(ip, regs, constants, state);
    }
    
    // 1. Instance (Class Object)
    if (obj.is_instance()) [[likely]] {
        instance_t inst = obj.as_instance();
        Shape* current_shape = inst->get_shape();

        for (int i = 0; i < IC_CAPACITY; ++i) {
            if (ic->entries[i].shape == current_shape) {
                regs[dst] = inst->get_field_at(ic->entries[i].offset);
                return ip;
            }
        }

        int offset = current_shape->get_offset(name);
        if (offset != -1) {
            update_inline_cache(ic, current_shape, static_cast<uint32_t>(offset));
            regs[dst] = inst->get_field_at(offset);
            return ip;
        }

        class_t k = inst->get_class();
        while (k) {
            if (k->has_method(name)) {
                regs[dst] = Value(state->heap.new_bound_method(inst, k->get_method(name).as_function()));
                return ip;
            }
            k = k->get_super();
        }
    }
    // 2. [FIX] Hash Table (Dictionary / Object Literal)
    else if (obj.is_hash_table()) {
        hash_table_t hash = obj.as_hash_table();
        
        // Ưu tiên 1: Tìm key trong map (obj.prop)
        if (hash->has(name)) {
            regs[dst] = hash->get(name);
            return ip;
        }
        
        // Ưu tiên 2: Tìm method primitive (obj.keys(), obj.has()...)
        Value method = find_primitive_method(state, obj, name);
        if (!method.is_null()) {
            regs[dst] = method;
            return ip;
        }
    }
    // 3. Module
    else if (obj.is_module()) {
        module_t mod = obj.as_module();
        if (mod->has_export(name)) {
            regs[dst] = mod->get_export(name);
            return ip;
        }
    }
    // 4. Class (Static Method)
    else if (obj.is_class()) {
        class_t k = obj.as_class();
        if (k->has_method(name)) {
            regs[dst] = k->get_method(name); 
            return ip;
        }
    }
    // 5. Primitive khác (Array, String)
    else {
        Value method = find_primitive_method(state, obj, name);
        if (!method.is_null()) {
            auto bound = state->heap.new_bound_method(obj, method); 
            regs[dst] = Value(bound);
            return ip;
        }
    }
    
    // Not found
    state->ctx.current_frame_->ip_ = start_ip;
    state->error(std::format("Runtime Error: Property '{}' not found on type '{}'.", 
        name->c_str(), to_string(obj)));
    return impl_PANIC(ip, regs, constants, state);
}

[[gnu::always_inline]] 
static const uint8_t* impl_SET_PROP(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    const uint8_t* start_ip = ip - 1;

    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    uint16_t val_reg = read_u16(ip);
    
    InlineCache* ic = get_inline_cache(ip);
    Value& obj = regs[obj_reg];
    Value& val = regs[val_reg];
    string_t name = constants[name_idx].as_string();
    
    // 1. Instance
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
    }
    // 2. [FIX] Hash Table (Dictionary / Object Literal)
    else if (obj.is_hash_table()) {
        obj.as_hash_table()->set(name, val);
        state->heap.write_barrier(obj.as_object(), val);
    }
    else {
        state->ctx.current_frame_->ip_ = start_ip;
        state->error(std::format("SET_PROP: Cannot set property '{}' on type '{}'.", name->c_str(), to_string(obj)));
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
        state->error("SET_METHOD: Target must be a Class.");
        return impl_PANIC(ip, regs, constants, state);
    }
    if (!method_val.is_function() && !method_val.is_native()) [[unlikely]] {
        state->error("SET_METHOD: Value must be a Function or Native.");
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
        state->error("INHERIT: Both operands must be Classes.");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    sub_val.as_class()->set_super(super_val.as_class());
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_GET_SUPER(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    const uint8_t* start_ip = ip - 1;

    uint16_t dst = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    string_t name = constants[name_idx].as_string();
    
    Value& receiver_val = regs[0]; 
    
    if (!receiver_val.is_instance()) [[unlikely]] {
        state->ctx.current_frame_->ip_ = start_ip;
        state->error("GET_SUPER: 'super' is only valid inside an instance method.");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    instance_t receiver = receiver_val.as_instance();
    class_t klass = receiver->get_class();
    class_t super = klass->get_super();
    
    if (!super) {
        state->ctx.current_frame_->ip_ = start_ip;
        state->error("GET_SUPER: Class has no superclass.");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    class_t k = super;
    while (k) {
        if (k->has_method(name)) {
            regs[dst] = Value(state->heap.new_bound_method(receiver, k->get_method(name).as_function()));
            return ip;
        }
        k = k->get_super();
    }
    
    state->ctx.current_frame_->ip_ = start_ip;
    state->error(std::format("GET_SUPER: Method '{}' not found in superclass.", name->c_str()));
    return impl_PANIC(ip, regs, constants, state);
}

} // namespace meow::handlers
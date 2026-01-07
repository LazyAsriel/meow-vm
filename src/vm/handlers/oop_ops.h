#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include <meow/core/shape.h>
#include <meow/core/module.h>
#include <module/module_manager.h>
#include <iostream> 
#include <format>
#include <array>
#include <algorithm>

namespace meow::handlers {

// Mã lỗi giả định
constexpr int ERR_PROP = 40;
constexpr int ERR_METHOD = 41;
constexpr int ERR_INHERIT = 42;

static constexpr int IC_CAPACITY = 4;

struct PrimitiveShapes {
    static inline const Shape* ARRAY  = reinterpret_cast<Shape*>(static_cast<uintptr_t>(0x1));
    static inline const Shape* STRING = reinterpret_cast<Shape*>(static_cast<uintptr_t>(0x2));
    static inline const Shape* OBJECT = reinterpret_cast<Shape*>(static_cast<uintptr_t>(0x3));
};

struct InlineCacheEntry {
    const Shape* shape;
    const Shape* transition;
    uint32_t offset;
} __attribute__((packed));

struct InlineCache {
    InlineCacheEntry entries[IC_CAPACITY];
} __attribute__((packed));

// Helper cập nhật Inline Cache (giữ nguyên logic Move-to-front)
inline static void update_inline_cache(InlineCache* ic, const Shape* shape, const Shape* transition, uint32_t offset) {
    for (int i = 0; i < IC_CAPACITY; ++i) {
        if (ic->entries[i].shape == shape) {
            if (i > 0) {
                InlineCacheEntry temp = ic->entries[i];
                // Move to front
                std::memmove(&ic->entries[1], &ic->entries[0], i * sizeof(InlineCacheEntry));
                ic->entries[0] = temp;
                ic->entries[0].transition = transition;
                ic->entries[0].offset = offset;
            } else {
                ic->entries[0].transition = transition;
                ic->entries[0].offset = offset;
            }
            return;
        }
    }
    // Shift right & Insert new at front
    std::memmove(&ic->entries[1], &ic->entries[0], (IC_CAPACITY - 1) * sizeof(InlineCacheEntry));
    ic->entries[0].shape = shape;
    ic->entries[0].transition = transition;
    ic->entries[0].offset = offset;
}

enum class CoreModType { ARRAY, STRING, OBJECT };

[[gnu::always_inline]]
static inline module_t get_core_module(VMState* state, CoreModType type) {
    static module_t mod_array = nullptr;
    static module_t mod_string = nullptr;
    static module_t mod_object = nullptr;

    switch (type) {
        case CoreModType::ARRAY:
            if (!mod_array) [[unlikely]] {
                auto res = state->modules.load_module(state->heap.new_string("array"), nullptr);
                if (res.ok()) mod_array = res.value();
            }
            return mod_array;
        case CoreModType::STRING:
            if (!mod_string) [[unlikely]] {
                auto res = state->modules.load_module(state->heap.new_string("string"), nullptr);
                if (res.ok()) mod_string = res.value();
            }
            return mod_string;
        case CoreModType::OBJECT:
            if (!mod_object) [[unlikely]] {
                auto res = state->modules.load_module(state->heap.new_string("object"), nullptr);
                if (res.ok()) mod_object = res.value();
            }
            return mod_object;
    }
    return nullptr;
}

static inline Value find_primitive_method_slow(VMState* state, const Value& obj, string_t name, int32_t* out_index = nullptr) {
    module_t mod = nullptr;

    if (obj.is_array()) mod = get_core_module(state, CoreModType::ARRAY);
    else if (obj.is_string()) mod = get_core_module(state, CoreModType::STRING);
    else if (obj.is_hash_table()) mod = get_core_module(state, CoreModType::OBJECT);

    if (mod) {
        int32_t idx = mod->resolve_export_index(name);
        if (idx != -1) {
            if (out_index) *out_index = idx;
            return mod->get_export_by_index(static_cast<uint32_t>(idx));
        }
    }
    return Value(null_t{});
}

// --- HANDLERS ---

[[gnu::always_inline]] 
static const uint8_t* impl_INVOKE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst, obj_reg, name_idx, arg_start, argc] = decode::args<u16, u16, u16, u16, u16>(ip);
    
    // 2. Inline Cache (Tự động tính size dựa trên architecture)
    InlineCache* ic = const_cast<InlineCache*>(decode::as_struct<InlineCache>(ip));
    
    // IP lúc này đã trỏ sang lệnh tiếp theo (Next IP)
    const uint8_t* next_ip = ip; 
    
    // Tính offset để báo lỗi (10 byte args + Size IC)
    constexpr size_t ErrOffset = 10 + sizeof(InlineCache);

    Value& receiver = regs[obj_reg];
    string_t name = constants[name_idx].as_string();

    // 3. Instance Method Call (Fast Path)
    if (receiver.is_instance()) [[likely]] {
        instance_t inst = receiver.as_instance();
        class_t k = inst->get_class();
                
        while (k) {
            if (k->has_method(name)) {
                Value method = k->get_method(name);
                
                if (method.is_function()) {
                    const uint8_t* jump_target = push_call_frame(
                        state, method.as_function(), argc, &regs[arg_start], &receiver,
                        (dst == 0xFFFF) ? nullptr : &regs[dst], next_ip, ip - ErrOffset - 1 // Error IP (approx)
                    );
                    if (!jump_target) return impl_PANIC(ip, regs, constants, state);
                    return jump_target;
                }
                else if (method.is_native()) {
                    constexpr size_t MAX_NATIVE_ARGS = 64;
                    Value arg_buffer[MAX_NATIVE_ARGS];
                    arg_buffer[0] = receiver;
                    size_t copy_count = std::min(static_cast<size_t>(argc), MAX_NATIVE_ARGS - 1);
                    if (copy_count > 0) std::copy_n(regs + arg_start, copy_count, arg_buffer + 1);

                    Value result = method.as_native()(&state->machine, copy_count + 1, arg_buffer);
                    if (state->machine.has_error()) return impl_PANIC(ip, regs, constants, state);
                    if (dst != 0xFFFF) regs[dst] = result;
                    return next_ip;
                }
                break;
            }
            k = k->get_super();
        }
    }

    // 4. Primitive Method Call (Optimized Stack Allocation)
    Value method_val = find_primitive_method_slow(state, receiver, name); 
    
    if (!method_val.is_null()) [[likely]] {         
         if (method_val.is_native()) {
             constexpr size_t MAX_NATIVE_ARGS = 64;
             Value arg_buffer[MAX_NATIVE_ARGS];

             arg_buffer[0] = receiver; 
             size_t copy_count = std::min(static_cast<size_t>(argc), MAX_NATIVE_ARGS - 1);
             if (copy_count > 0) std::copy_n(regs + arg_start, copy_count, arg_buffer + 1);

             Value result = method_val.as_native()(&state->machine, copy_count + 1, arg_buffer);

             if (state->machine.has_error()) return impl_PANIC(ip, regs, constants, state);
             if (dst != 0xFFFF) regs[dst] = result;
             return next_ip;
         } else {
             return ERROR<ErrOffset>(ip, regs, constants, state, ERR_METHOD, "Primitive method must be native");
         }
    }

    return ERROR<ErrOffset>(ip, regs, constants, state, ERR_METHOD, 
                            "Method '{}' not found on object '{}'.", name->c_str(), to_string(receiver));
}

[[gnu::always_inline]] 
static const uint8_t* impl_NEW_CLASS(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 2 * u16 = 4 bytes -> Load u32
    auto [dst, name_idx] = decode::args<u16, u16>(ip);
    regs[dst] = Value(state->heap.new_class(constants[name_idx].as_string()));
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_NEW_INSTANCE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 2 * u16 = 4 bytes -> Load u32
    auto [dst, class_reg] = decode::args<u16, u16>(ip);
    Value& class_val = regs[class_reg];
    
    if (!class_val.is_class()) [[unlikely]] {
        return ERROR<4>(ip, regs, constants, state, ERR_INHERIT, "NEW_INSTANCE: Operand is not a Class");
    }
    regs[dst] = Value(state->heap.new_instance(class_val.as_class(), state->heap.get_empty_shape()));
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_GET_PROP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 1. Decode Args: 3 * u16 = 6 bytes -> Load u64
    auto [dst, obj_reg, name_idx] = decode::args<u16, u16, u16>(ip);
    
    // 2. Decode IC
    InlineCache* ic = const_cast<InlineCache*>(decode::as_struct<InlineCache>(ip));

    constexpr size_t ErrOffset = 6 + sizeof(InlineCache);

    Value& obj = regs[obj_reg];
    string_t name = constants[name_idx].as_string();

    // 1. Magic Prop: length
    static string_t str_length = nullptr;
    if (!str_length) [[unlikely]] str_length = state->heap.new_string("length");

    if (name == str_length) {
        if (obj.is_array()) {
            regs[dst] = Value(static_cast<int64_t>(obj.as_array()->size()));
            return ip;
        }
        if (obj.is_string()) {
            regs[dst] = Value(static_cast<int64_t>(obj.as_string()->size()));
            return ip;
        }
    }

    // 2. Instance Access (IC Optimized)
    if (obj.is_instance()) [[likely]] {
        instance_t inst = obj.as_instance();
        Shape* current_shape = inst->get_shape();

        // IC Hit (Slot 0)
        if (ic->entries[0].shape == current_shape) {
            regs[dst] = inst->get_field_at(ic->entries[0].offset);
            return ip;
        }
        // Move-to-front (Slot 1)
        if (ic->entries[1].shape == current_shape) {
            InlineCacheEntry temp = ic->entries[1];
            ic->entries[1] = ic->entries[0];
            ic->entries[0] = temp;
            regs[dst] = inst->get_field_at(temp.offset);
            return ip;
        }
        
        // IC Miss
        int offset = current_shape->get_offset(name);
        if (offset != -1) {
            update_inline_cache(ic, current_shape, nullptr, static_cast<uint32_t>(offset));
            regs[dst] = inst->get_field_at(offset);
            return ip;
        }
        
        // Lookup Class Methods
        class_t k = inst->get_class();
        while (k) {
            if (k->has_method(name)) {
                regs[dst] = Value(state->heap.new_bound_method(inst, k->get_method(name).as_function()));
                return ip;
            }
            k = k->get_super();
        }
    }
    // 3. Array/String Primitive Access (IC Optimized)
    else if (obj.is_array() || obj.is_string()) {
        const Shape* sentinel = obj.is_array() ? PrimitiveShapes::ARRAY : PrimitiveShapes::STRING;
        
        if (ic->entries[0].shape == sentinel) {
            module_t mod = get_core_module(state, obj.is_array() ? CoreModType::ARRAY : CoreModType::STRING);
            Value method = mod->get_export_by_index(ic->entries[0].offset);
            regs[dst] = Value(state->heap.new_bound_method(obj, method));
            return ip;
        }

        int32_t idx = -1;
        Value method = find_primitive_method_slow(state, obj, name, &idx);
        
        if (!method.is_null()) {
            if (idx != -1) {
                update_inline_cache(ic, sentinel, nullptr, static_cast<uint32_t>(idx));
            }
            regs[dst] = Value(state->heap.new_bound_method(obj, method));
            return ip;
        }
    }
    // 4. Hash Table & Module
    else if (obj.is_hash_table()) {
        hash_table_t hash = obj.as_hash_table();
        if (hash->get(name, &regs[dst])) return ip;
        
        int32_t idx = -1;
        Value method = find_primitive_method_slow(state, obj, name, &idx);
        if (!method.is_null()) {
            regs[dst] = Value(state->heap.new_bound_method(obj, method));
            return ip;
        }
        regs[dst] = Value(null_t{});
        return ip;
    }
    else if (obj.is_module()) {
        module_t mod = obj.as_module();
        if (mod->has_export(name)) {
            regs[dst] = mod->get_export(name);
            return ip;
        }
    }
    else if (obj.is_class()) {
        class_t k = obj.as_class();
        if (k->has_method(name)) {
            regs[dst] = k->get_method(name); 
            return ip;
        }
    }
    else if (obj.is_null()) [[unlikely]] {
        return ERROR<ErrOffset>(ip, regs, constants, state, ERR_PROP, "Cannot read property '{}' of null.", name->c_str());
    }

    return ERROR<ErrOffset>(ip, regs, constants, state, ERR_PROP, 
                            "Property '{}' not found on type '{}'.", name->c_str(), to_string(obj));
}

[[gnu::always_inline]] 
static const uint8_t* impl_SET_PROP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 1. Decode: 3 * u16 = 6 bytes -> Load u64
    auto [obj_reg, name_idx, val_reg] = decode::args<u16, u16, u16>(ip);
    
    // 2. Decode IC
    InlineCache* ic = const_cast<InlineCache*>(decode::as_struct<InlineCache>(ip));
    
    constexpr size_t ErrOffset = 6 + sizeof(InlineCache);

    Value& obj = regs[obj_reg];
    Value& val = regs[val_reg];
    
    if (obj.is_instance()) [[likely]] {
        instance_t inst = obj.as_instance();
        Shape* current_shape = inst->get_shape();

        // IC Check
        for (int i = 0; i < IC_CAPACITY; ++i) {
            if (ic->entries[i].shape == current_shape) {
                if (ic->entries[i].transition) { // Transition
                    inst->set_shape(const_cast<Shape*>(ic->entries[i].transition));
                    inst->add_field(val); 
                    state->heap.write_barrier(inst, val);
                    return ip;
                }
                // Update
                inst->set_field_at(ic->entries[i].offset, val);
                state->heap.write_barrier(inst, val);
                return ip;
            }
        }

        string_t name = constants[name_idx].as_string();
        int offset = current_shape->get_offset(name);

        if (offset != -1) { // Update existing
            update_inline_cache(ic, current_shape, nullptr, static_cast<uint32_t>(offset));
            inst->set_field_at(offset, val);
            state->heap.write_barrier(inst, val);
        } 
        else { // Transition new
            Shape* next_shape = current_shape->get_transition(name);
            if (!next_shape) next_shape = current_shape->add_transition(name, &state->heap);
            
            uint32_t new_offset = static_cast<uint32_t>(inst->get_field_count());
            update_inline_cache(ic, current_shape, next_shape, new_offset);

            inst->set_shape(next_shape);
            inst->add_field(val);
            state->heap.write_barrier(inst, Value(reinterpret_cast<object_t>(next_shape))); 
            state->heap.write_barrier(inst, val);
        }
        return ip;
    }
    else if (obj.is_hash_table()) {
        string_t name = constants[name_idx].as_string();
        obj.as_hash_table()->set(name, val);
        state->heap.write_barrier(obj.as_object(), val);
    }
    else {
        return ERROR<ErrOffset>(ip, regs, constants, state, ERR_PROP, 
                                "SET_PROP: Cannot set property '{}' on type '{}'.", 
                                constants[name_idx].as_string()->c_str(), to_string(obj));
    }
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_SET_METHOD(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 3 * u16 = 6 bytes -> Load u64
    auto [class_reg, name_idx, method_reg] = decode::args<u16, u16, u16>(ip);
    
    Value& class_val = regs[class_reg];
    Value& method_val = regs[method_reg];
    
    if (!class_val.is_class()) [[unlikely]] {
        return ERROR<6>(ip, regs, constants, state, ERR_INHERIT, "SET_METHOD: Operand is not a Class");
    }
    
    class_val.as_class()->set_method(constants[name_idx].as_string(), method_val);
    state->heap.write_barrier(class_val.as_class(), method_val);
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_INHERIT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 2 * u16 = 4 bytes -> Load u32
    auto [sub_reg, super_reg] = decode::args<u16, u16>(ip);
    
    Value& sub_val = regs[sub_reg];
    Value& super_val = regs[super_reg];
    
    if (!sub_val.is_class() || !super_val.is_class()) [[unlikely]] {
        return ERROR<4>(ip, regs, constants, state, ERR_INHERIT, "INHERIT: Both operands must be Classes");
    }
    
    sub_val.as_class()->set_super(super_val.as_class());
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_GET_SUPER(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 2 * u16 = 4 bytes -> Load u32
    auto [dst, name_idx] = decode::args<u16, u16>(ip);
    string_t name = constants[name_idx].as_string();
    
    Value& receiver_val = regs[0]; // Convention: this/receiver is always reg[0] in method call? 
    // FIXME: Nếu GET_SUPER được gọi ngoài method thì sao? 
    // Tuy nhiên bytecode GET_SUPER thường chỉ sinh ra bên trong method của class.
    
    if (!receiver_val.is_instance()) [[unlikely]] {
        return ERROR<4>(ip, regs, constants, state, ERR_INHERIT, "GET_SUPER: 'this' is not an instance");
    }
    
    instance_t receiver = receiver_val.as_instance();
    class_t super = receiver->get_class()->get_super();
    
    if (!super) {
        return ERROR<4>(ip, regs, constants, state, ERR_INHERIT, "GET_SUPER: Class has no superclass");
    }
    
    class_t k = super;
    while (k) {
        if (k->has_method(name)) {
            regs[dst] = Value(state->heap.new_bound_method(receiver, k->get_method(name).as_function()));
            return ip;
        }
        k = k->get_super();
    }
    
    return ERROR<4>(ip, regs, constants, state, ERR_METHOD, "GET_SUPER: Method '{}' not found in superclass", name->c_str());
}

} // namespace meow::handlers
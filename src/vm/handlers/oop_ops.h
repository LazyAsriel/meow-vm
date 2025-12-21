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
static const uint8_t* impl_INVOKE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    static bool is_ran = false;
    if (!is_ran) {
        std::println("Đang dùng INVOKE OpCode (chỉ hiện log một lần)");
        is_ran = true;
    }
    // 1. Tính toán địa chỉ return (Nhảy qua 80 bytes fat instruction)
    // ip đang ở byte đầu tiên của tham số (sau opcode)
    // Next Opcode = (ip - 1) + 80 = ip + 79
    const uint8_t* next_ip = ip + 79;
    const uint8_t* start_ip = ip - 1; // Để báo lỗi chính xác

    // 2. Decode Arguments (10 bytes)
    uint16_t dst = read_u16(ip);
    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    uint16_t arg_start = read_u16(ip);
    uint16_t argc = read_u16(ip);
    
    // 3. Inline Cache (48 bytes)
    InlineCache* ic = get_inline_cache(ip); 

    Value& receiver = regs[obj_reg];
    string_t name = constants[name_idx].as_string();

    // 4. Instance Method Call (Fast Path)
    if (receiver.is_instance()) [[likely]] {
        instance_t inst = receiver.as_instance();
        Shape* current_shape = inst->get_shape();

        // --- OPTIMIZATION: Inline Cache Check ---
        // Nếu shape của object trùng với shape trong cache, ta lấy luôn method offset/index
        // (Lưu ý: Bạn cần mở rộng cấu trúc IC để lưu method ptr hoặc dùng cơ chế lookup nhanh)
        
        // Hiện tại ta vẫn lookup thủ công (nhưng vẫn nhanh hơn tạo BoundMethod):
        class_t k = inst->get_class();
        while (k) {
            if (k->has_method(name)) {
                Value method = k->get_method(name);
                
                // A. Gọi Meow Function (Optimized)
                if (method.is_function()) {
                    const uint8_t* jump_target = push_call_frame(
                        state,
                        method.as_function(),
                        argc,
                        &regs[arg_start], // Arguments source
                        &receiver,        // Receiver ('this')
                        (dst == 0xFFFF) ? nullptr : &regs[dst],
                        next_ip,          // Return address (sau padding)
                        start_ip          // Error address
                    );
                    
                    if (jump_target == nullptr) return impl_PANIC(start_ip, regs, constants, state);
                    return jump_target;
                }
                
                // B. Gọi Native Function
                else if (method.is_native()) {
                    // Native cần mảng liên tục [this, arg1, arg2...]
                    // Vì 'receiver' và 'args' không nằm liền nhau trên stack (regs),
                    // ta phải tạo buffer tạm.
                    
                    // Small optimization: Dùng stack C++ (alloca) hoặc vector nhỏ
                    std::vector<Value> native_args;
                    native_args.reserve(argc + 1);
                    native_args.push_back(receiver); // this
                    for(int i=0; i<argc; ++i) native_args.push_back(regs[arg_start + i]);

                    Value result = method.as_native()(&state->machine, native_args.size(), native_args.data());
                    
                    if (state->machine.has_error()) return impl_PANIC(start_ip, regs, constants, state);
                    
                    if (dst != 0xFFFF) regs[dst] = result;
                    return next_ip;
                }
                break;
            }
            k = k->get_super();
        }
    }

    // 5. Fallback (Slow Path)
    // Xử lý trường hợp:
    // - Receiver không phải Instance (vd: String, Array, Module...)
    // - Method không tìm thấy trong Class (có thể là field chứa closure: obj.callback())
    
    // Logic: Tái sử dụng logic của GET_PROP để lấy value, sau đó CALL value đó.
    
    // a. Mô phỏng GET_PROP (lấy value vào regs[dst] tạm thời hoặc temp var)
    // Lưu ý: impl_GET_PROP trong oop_ops.h đã có logic tìm field/method/bound_method.
    // Nhưng ta không gọi impl_GET_PROP được vì nó thao tác IP và Stack khác.
    
    // Solution đơn giản: Gọi hàm helper find_property (bạn cần tách logic từ impl_GET_PROP ra)
    // Hoặc copy logic find primitive method.
    
    // Ví dụ fallback đơn giản cho Primitive (String/Array method):
    Value method_val = find_primitive_method(state, receiver, name); 
    if (!method_val.is_null()) {
         // Primitive method thường là Native, xử lý như case Native ở trên
         // Cần tạo BoundMethod? Không, native call trực tiếp được nếu ta pass receiver.
         // Nhưng find_primitive_method trả về NativeFunction thuần.
         
         std::vector<Value> native_args;
         native_args.reserve(argc + 1);
         native_args.push_back(receiver); 
         for(int i=0; i<argc; ++i) native_args.push_back(regs[arg_start + i]);
         
         Value result;
         if (method_val.is_native()) {
             result = method_val.as_native()(&state->machine, native_args.size(), native_args.data());
         } else {
             // Trường hợp hiếm: Primitive trả về BoundMethod hoặc Closure
             // ... xử lý tương tự ...
             state->error("INVOKE: Primitive method type not supported yet.");
             return impl_PANIC(start_ip, regs, constants, state);
         }

         if (state->machine.has_error()) return impl_PANIC(start_ip, regs, constants, state);
         if (dst != 0xFFFF) regs[dst] = result;
         return next_ip;
    }

    // Nếu vẫn không tìm thấy -> Lỗi
    state->error(std::format("INVOKE: Method '{}' not found on object '{}'.", name->c_str(), to_string(receiver)));
    return impl_PANIC(start_ip, regs, constants, state);
}

[[gnu::always_inline]] 
static const uint8_t* impl_NEW_CLASS(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // static bool is_ran = false;
    // if (!is_ran) {
    //     std::println("Đang dùng NEW_CLASS OpCode (chỉ hiện log một lần)");
    //     is_ran = true;
    // }
    uint16_t dst = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    string_t name = constants[name_idx].as_string();
    regs[dst] = Value(state->heap.new_class(name));
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_NEW_INSTANCE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
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
static const uint8_t* impl_GET_PROP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
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
    else if (obj.is_hash_table()) {
        hash_table_t hash = obj.as_hash_table();
        
        if (hash->has(name)) {
            regs[dst] = hash->get(name);
            return ip;
        }
        
        Value method = find_primitive_method(state, obj, name);
        if (!method.is_null()) {
            auto bound = state->heap.new_bound_method(obj, method); 
            regs[dst] = Value(bound);
            return ip;
        }
        
        regs[dst] = Value(null_t{}); 
        return ip;
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
    else if (obj.is_array() && std::strcmp(name->c_str(), "length") == 0) {
        regs[dst] = Value(static_cast<int64_t>(obj.as_array()->size()));
        return ip;
    }
    else if (obj.is_string() && std::strcmp(name->c_str(), "length") == 0) {
        regs[dst] = Value(static_cast<int64_t>(obj.as_string()->size()));
        return ip;
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
static const uint8_t* impl_SET_PROP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const uint8_t* start_ip = ip - 1;

    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    uint16_t val_reg = read_u16(ip);
    
    InlineCache* ic = get_inline_cache(ip);
    Value& obj = regs[obj_reg];
    Value& val = regs[val_reg];
    string_t name = constants[name_idx].as_string();
    
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
            
            state->heap.write_barrier(inst, Value(reinterpret_cast<object_t>(next_shape))); 

            inst->add_field(val);
            state->heap.write_barrier(inst, val);
        }
    }
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
static const uint8_t* impl_SET_METHOD(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
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
    state->heap.write_barrier(class_val.as_class(), method_val);
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_INHERIT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
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
static const uint8_t* impl_GET_SUPER(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
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
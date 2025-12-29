#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include <meow/cast.h>

namespace meow::handlers {


[[gnu::always_inline]] static const uint8_t* impl_LOAD_CONST(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t idx = read_u16(ip);
    regs[dst] = constants[idx];
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_LOAD_NULL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    regs[dst] = null_t{};
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_LOAD_TRUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    regs[dst] = true;
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_LOAD_FALSE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    regs[dst] = false;
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_LOAD_INT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    regs[dst] = *reinterpret_cast<const int64_t*>(ip);
    ip += 8;
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_LOAD_FLOAT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    regs[dst] = *reinterpret_cast<const double*>(ip);
    ip += 8;
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_MOVE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src = read_u16(ip);
    regs[dst] = regs[src];
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_NEW_ARRAY(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t start_idx = read_u16(ip);
    uint16_t count = read_u16(ip);

    auto array = state->heap.new_array();
    regs[dst] = object_t(array);
    array->reserve(count);
    for (size_t i = 0; i < count; ++i) {
        array->push(regs[start_idx + i]);
    }
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_NEW_HASH(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t start_idx = read_u16(ip);
    uint16_t count = read_u16(ip);
    
    auto hash = state->heap.new_hash(count); 

    regs[dst] = Value(hash); 

    for (size_t i = 0; i < count; ++i) {
        Value& key = regs[start_idx + i * 2];
        Value& val = regs[start_idx + i * 2 + 1];
        
        if (key.is_string()) [[likely]] {
            hash->set(key.as_string(), val);
        } else {
            std::string s = to_string(key);
            string_t k = state->heap.new_string(s);
            hash->set(k, val);
        }
    }
    
    return ip;
}
[[gnu::always_inline]] static const uint8_t* impl_GET_INDEX(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src_reg = read_u16(ip);
    uint16_t key_reg = read_u16(ip);
    
    Value& src = regs[src_reg];
    Value& key = regs[key_reg];

    if (src.is_array()) {
        if (!key.is_int()) {
            state->error("Array index phải là số nguyên.", ip);
            return impl_PANIC(ip, regs, constants, state);
        }
        array_t arr = src.as_array();
        int64_t idx = key.as_int();
        // if (idx < 0 || static_cast<size_t>(idx) >= arr->size()) {
        //     state->error("Array index out of bounds.", ip);
        //     return impl_PANIC(ip, regs, constants, state);
        // }
        // regs[dst] = arr->get(idx);

        if (idx < 0 || static_cast<size_t>(idx) >= arr->size()) {
            regs[dst] = null_t{};
        } else {
            regs[dst] = arr->get(idx);
        }
    } 
    else if (src.is_hash_table()) {
        hash_table_t hash = src.as_hash_table();
        string_t k = nullptr;
        
        if (!key.is_string()) {
            std::string s = to_string(key);
            k = state->heap.new_string(s);
        } else {
            k = key.as_string();
        }

        if (hash->has(k)) {
            regs[dst] = hash->get(k);
        } else {
            regs[dst] = Value(null_t{});
        }
    }
    else if (src.is_string()) {
        if (!key.is_int()) {
            state->error("String index phải là số nguyên.", ip);
            return impl_PANIC(ip, regs, constants, state);
        }
        string_t str = src.as_string();
        int64_t idx = key.as_int();
        if (idx < 0 || static_cast<size_t>(idx) >= str->size()) {
            state->error("String index out of bounds.", ip);
            return impl_PANIC(ip, regs, constants, state);
        }
        char c = str->get(idx);
        regs[dst] = Value(state->heap.new_string(&c, 1));
    }

    else if (src.is_instance()) {
        if (!key.is_string()) {
            state->error("Instance index key phải là chuỗi (tên thuộc tính/phương thức).", ip);
            return impl_PANIC(ip, regs, constants, state);
        }
        
        string_t name = key.as_string();
        instance_t inst = src.as_instance();
        
        int offset = inst->get_shape()->get_offset(name);
        if (offset != -1) {
            regs[dst] = inst->get_field_at(offset);
        } 
        else {
            class_t k = inst->get_class();
            Value method = null_t{};
            
            while (k) {
                if (k->has_method(name)) {
                    method = k->get_method(name);
                    break;
                }
                k = k->get_super();
            }
            
            if (!method.is_null()) {
                if (method.is_function() || method.is_native()) {
                    auto bound = state->heap.new_bound_method(src, method);
                    regs[dst] = Value(bound);
                } else {
                    regs[dst] = method;
                }
            } else {
                regs[dst] = Value(null_t{});
            }
        }
    }

    else {
        state->error("Không thể dùng toán tử index [] trên kiểu dữ liệu này.", ip);
        return impl_PANIC(ip, regs, constants, state);
    }
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_SET_INDEX(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t src_reg = read_u16(ip);
    uint16_t key_reg = read_u16(ip);
    uint16_t val_reg = read_u16(ip);

    Value& src = regs[src_reg];
    Value& key = regs[key_reg];
    Value& val = regs[val_reg];

    if (src.is_array()) {
        if (!key.is_int()) {
            state->error("Array index phải là số nguyên.", ip);
            return impl_PANIC(ip, regs, constants, state);
        }
        array_t arr = src.as_array();
        int64_t idx = key.as_int();
        if (idx < 0) {
            state->error("Array index không được âm.", ip);
            return impl_PANIC(ip, regs, constants, state);
        }
        if (static_cast<size_t>(idx) >= arr->size()) {
            arr->resize(idx + 1);
        }
        
        arr->set(idx, val);
        state->heap.write_barrier(src.as_object(), val);
    }
    else if (src.is_hash_table()) {
        hash_table_t hash = src.as_hash_table();
        string_t k = nullptr;

        if (!key.is_string()) {
            std::string s = to_string(key);
            k = state->heap.new_string(s);
        } else {
            k = key.as_string();
        }

        hash->set(k, val);
        
        state->heap.write_barrier(src.as_object(), val);
    } 
        else if (src.is_instance()) {
        if (!key.is_string()) {
            state->error("Instance set index key phải là chuỗi.", ip);
            return impl_PANIC(ip, regs, constants, state);
        }
        
        string_t name = key.as_string();
        instance_t inst = src.as_instance();
        
        int offset = inst->get_shape()->get_offset(name);
        if (offset != -1) {
            inst->set_field_at(offset, val);
            state->heap.write_barrier(inst, val);
        } else {
            Shape* current_shape = inst->get_shape();
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
    else {
        state->error("Không thể gán index [] trên kiểu dữ liệu này.", ip);
        return impl_PANIC(ip, regs, constants, state);
    }
    return ip;
}
[[gnu::always_inline]] static const uint8_t* impl_GET_KEYS(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src_reg = read_u16(ip);
    Value& src = regs[src_reg];
    
    auto keys_array = state->heap.new_array();
    
    if (src.is_hash_table()) {
        hash_table_t hash = src.as_hash_table();
        keys_array->reserve(hash->size());
        for (auto it = hash->begin(); it != hash->end(); ++it) {
            keys_array->push(Value(it->first));
        }
    } else if (src.is_array()) {
        size_t sz = src.as_array()->size();
        keys_array->reserve(sz);
        for (size_t i = 0; i < sz; ++i) keys_array->push(Value((int64_t)i));
    } else if (src.is_string()) {
        size_t sz = src.as_string()->size();
        keys_array->reserve(sz);
        for (size_t i = 0; i < sz; ++i) keys_array->push(Value((int64_t)i));
    }
    
    regs[dst] = Value(keys_array);
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_GET_VALUES(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src_reg = read_u16(ip);
    Value& src = regs[src_reg];
    
    auto vals_array = state->heap.new_array();

    if (src.is_hash_table()) {
        hash_table_t hash = src.as_hash_table();
        vals_array->reserve(hash->size());
        for (auto it = hash->begin(); it != hash->end(); ++it) {
            vals_array->push(it->second);
        }
    } else if (src.is_array()) {
        array_t arr = src.as_array();
        vals_array->reserve(arr->size());
        for (size_t i = 0; i < arr->size(); ++i) vals_array->push(arr->get(i));
    } else if (src.is_string()) {
        string_t str = src.as_string();
        vals_array->reserve(str->size());
        for (size_t i = 0; i < str->size(); ++i) {
            char c = str->get(i);
            vals_array->push(Value(state->heap.new_string(&c, 1)));
        }
    }

    regs[dst] = Value(vals_array);
    return ip;
}

} // namespace meow::handlers
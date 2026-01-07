#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include <meow/cast.h>

namespace meow::handlers {

// Mã lỗi giả định (Bạn nên move vào enum chung)
constexpr int ERR_TYPE = 10;
constexpr int ERR_BOUNDS = 11;
constexpr int ERR_READ_ONLY = 12;

// =========================================================
// STANDARD 16-BIT REGISTER OPERATIONS
// =========================================================

[[gnu::always_inline]] 
static const uint8_t* impl_LOAD_CONST(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 2 * u16 = 4 bytes -> Tự động load u32
    auto [dst, idx] = decode::args<u16, u16>(ip);
    regs[dst] = constants[idx];
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_LOAD_NULL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst] = decode::args<u16>(ip);
    regs[dst] = null_t{};
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_LOAD_TRUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst] = decode::args<u16>(ip);
    regs[dst] = true;
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_LOAD_FALSE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst] = decode::args<u16>(ip);
    regs[dst] = false;
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_LOAD_INT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // Tổng > 8 bytes -> Decoder tự động fallback đọc tuần tự (an toàn alignment)
    auto [dst, val] = decode::args<u16, i64>(ip);
    regs[dst] = val;
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_LOAD_FLOAT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst, val] = decode::args<u16, f64>(ip);
    regs[dst] = val;
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_MOVE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 2 * u16 = 4 bytes -> Load u32
    auto [dst, src] = decode::args<u16, u16>(ip);
    regs[dst] = regs[src];
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_NEW_ARRAY(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 3 * u16 = 6 bytes -> Load u64 (đọc lố padding)
    auto [dst, start_idx, count] = decode::args<u16, u16, u16>(ip);

    auto array = state->heap.new_array();
    regs[dst] = object_t(array);
    array->reserve(count);
    
    // Unroll loop nhẹ nếu cần, hoặc để compiler lo
    for (size_t i = 0; i < count; ++i) {
        array->push(regs[start_idx + i]);
    }
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_NEW_HASH(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 3 * u16 = 6 bytes -> Load u64
    auto [dst, start_idx, count] = decode::args<u16, u16, u16>(ip);
    
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

[[gnu::always_inline]] 
static const uint8_t* impl_GET_INDEX(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 3 * u16 = 6 bytes -> Load u64
    auto [dst, src_reg, key_reg] = decode::args<u16, u16, u16>(ip);
    
    Value& src = regs[src_reg];
    Value& key = regs[key_reg];

    if (src.is_array()) {
        if (!key.is_int()) {
            return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "Array index must be integer");
        }
        array_t arr = src.as_array();
        int64_t idx = key.as_int();

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
            return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "String index must be integer");
        }
        string_t str = src.as_string();
        int64_t idx = key.as_int();
        if (idx < 0 || static_cast<size_t>(idx) >= str->size()) {
            return ERROR<6>(ip, regs, constants, state, ERR_BOUNDS, "String index out of bounds");
        }
        char c = str->get(idx);
        regs[dst] = Value(state->heap.new_string(&c, 1));
    }
    else if (src.is_instance()) {
        if (!key.is_string()) {
            return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "Instance index key must be string");
        }
        
        string_t name = key.as_string();
        instance_t inst = src.as_instance();
        
        int offset = inst->get_shape()->get_offset(name);
        if (offset != -1) {
            regs[dst] = inst->get_field_at(offset);
        } 
        else {
            // Fallback to method lookup
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
        return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "Cannot index on type {}", to_string(src));
    }
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_SET_INDEX(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 3 * u16 = 6 bytes -> Load u64
    auto [src_reg, key_reg, val_reg] = decode::args<u16, u16, u16>(ip);

    Value& src = regs[src_reg];
    Value& key = regs[key_reg];
    Value& val = regs[val_reg];

    if (src.is_array()) {
        if (!key.is_int()) {
            return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "Array index must be integer");
        }
        array_t arr = src.as_array();
        int64_t idx = key.as_int();
        if (idx < 0) {
            return ERROR<6>(ip, regs, constants, state, ERR_BOUNDS, "Array index cannot be negative");
        }
        // Auto resize functionality
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
            return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "Instance key must be string");
        }
        string_t name = key.as_string();
        instance_t inst = src.as_instance();
        
        int offset = inst->get_shape()->get_offset(name);
        if (offset != -1) {
            inst->set_field_at(offset, val);
            state->heap.write_barrier(inst, val);
        } else {
            // Shape Transition (Poly/Morphism support)
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
        return ERROR<6>(ip, regs, constants, state, ERR_TYPE, "Cannot set index on this type");
    }
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_GET_KEYS(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 2 * u16 = 4 bytes -> Load u32
    auto [dst, src_reg] = decode::args<u16, u16>(ip);
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

[[gnu::always_inline]] 
static const uint8_t* impl_GET_VALUES(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst, src_reg] = decode::args<u16, u16>(ip);
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

// =========================================================
// OPTIMIZED 8-BIT REGISTER OPERATIONS (_B VARIANTS)
// =========================================================

[[gnu::always_inline]] 
static const uint8_t* impl_MOVE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 2 * u8 = 2 bytes -> Load u16
    auto [dst, src] = decode::args<u8, u8>(ip);
    regs[dst] = regs[src];
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_LOAD_CONST_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // u8 + u16 = 3 bytes -> Load u32 (tận dụng padding)
    auto [dst, idx] = decode::args<u8, u16>(ip);
    regs[dst] = constants[idx];
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_LOAD_NULL_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst] = decode::args<u8>(ip);
    regs[dst] = null_t{};
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_LOAD_TRUE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst] = decode::args<u8>(ip);
    regs[dst] = true;
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_LOAD_FALSE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst] = decode::args<u8>(ip);
    regs[dst] = false;
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_LOAD_INT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // Fallback tuần tự: u8 (1b) + i64 (8b)
    auto [dst, val] = decode::args<u8, i64>(ip);
    regs[dst] = val;
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_LOAD_FLOAT_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [dst, val] = decode::args<u8, f64>(ip);
    regs[dst] = val;
    return ip;
}

} // namespace meow::handlers
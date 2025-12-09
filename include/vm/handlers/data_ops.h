#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include <cstring> // memcpy

namespace meow::handlers {

// --- Load / Move ---

[[always_inline]] static const uint8_t* impl_LOAD_CONST(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t idx = read_u16(ip);
    state->reg(dst) = state->constant(idx);
    return ip;
}

[[always_inline]] static const uint8_t* impl_LOAD_NULL(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    state->reg(dst) = Value(null_t{});
    return ip;
}

[[always_inline]] static const uint8_t* impl_LOAD_TRUE(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    state->reg(dst) = Value(true);
    return ip;
}

[[always_inline]] static const uint8_t* impl_LOAD_FALSE(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    state->reg(dst) = Value(false);
    return ip;
}

[[always_inline]] static const uint8_t* impl_LOAD_INT(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    int64_t val;
    std::memcpy(&val, ip, 8); ip += 8;
    state->reg(dst) = Value(val);
    return ip;
}

[[always_inline]] static const uint8_t* impl_LOAD_FLOAT(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    double val;
    std::memcpy(&val, ip, 8); ip += 8;
    state->reg(dst) = Value(val);
    return ip;
}

[[always_inline]] static const uint8_t* impl_MOVE(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src = read_u16(ip);
    state->reg(dst) = state->reg(src);
    return ip;
}

// --- Data Structures ---

[[always_inline]] static const uint8_t* impl_NEW_ARRAY(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t start_idx = read_u16(ip);
    uint16_t count = read_u16(ip);
    
    auto array = state->heap.new_array();
    array->reserve(count);
    for (size_t i = 0; i < count; ++i) {
        array->push(state->reg(start_idx + i));
    }
    state->reg(dst) = Value(object_t(array));
    return ip;
}

[[always_inline]] static const uint8_t* impl_NEW_HASH(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t start_idx = read_u16(ip);
    uint16_t count = read_u16(ip);
    
    auto hash = state->heap.new_hash();
    for (size_t i = 0; i < count; ++i) {
        Value& key = state->reg(start_idx + i * 2);
        Value& val = state->reg(start_idx + i * 2 + 1);
        
        if (!key.is_string()) {
            state->error("NEW_HASH: Key phải là string.");
            return impl_PANIC(ip, state);
        }
        hash->set(key.as_string(), val);
    }
    state->reg(dst) = Value(hash);
    return ip;
}

[[always_inline]] static const uint8_t* impl_GET_INDEX(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src_reg = read_u16(ip);
    uint16_t key_reg = read_u16(ip);
    
    Value& src = state->reg(src_reg);
    Value& key = state->reg(key_reg);

    if (src.is_array()) {
        if (!key.is_int()) {
            state->error("Array index phải là số nguyên.");
            return impl_PANIC(ip, state);
        }
        array_t arr = src.as_array();
        int64_t idx = key.as_int();
        if (idx < 0 || static_cast<size_t>(idx) >= arr->size()) {
            state->error("Array index out of bounds.");
            return impl_PANIC(ip, state);
        }
        state->reg(dst) = arr->get(idx);
    } 
    else if (src.is_hash_table()) {
        if (!key.is_string()) {
            state->error("Hash key phải là string.");
            return impl_PANIC(ip, state);
        }
        hash_table_t hash = src.as_hash_table();
        string_t k = key.as_string();
        if (hash->has(k)) {
            state->reg(dst) = hash->get(k);
        } else {
            state->reg(dst) = Value(null_t{});
        }
    }
    else if (src.is_string()) {
        if (!key.is_int()) {
            state->error("String index phải là số nguyên.");
            return impl_PANIC(ip, state);
        }
        string_t str = src.as_string();
        int64_t idx = key.as_int();
        if (idx < 0 || static_cast<size_t>(idx) >= str->size()) {
            state->error("String index out of bounds.");
            return impl_PANIC(ip, state);
        }
        // Tạo string mới 1 ký tự
        char c = str->get(idx);
        state->reg(dst) = Value(state->heap.new_string(&c, 1));
    }
    else {
        state->error("Không thể dùng toán tử index [] trên kiểu dữ liệu này.");
        return impl_PANIC(ip, state);
    }
    return ip;
}

[[always_inline]] static const uint8_t* impl_SET_INDEX(const uint8_t* ip, VMState* state) {
    uint16_t src_reg = read_u16(ip);
    uint16_t key_reg = read_u16(ip);
    uint16_t val_reg = read_u16(ip);

    Value& src = state->reg(src_reg);
    Value& key = state->reg(key_reg);
    Value& val = state->reg(val_reg);

    if (src.is_array()) {
        if (!key.is_int()) {
            state->error("Array index phải là số nguyên.");
            return impl_PANIC(ip, state);
        }
        array_t arr = src.as_array();
        int64_t idx = key.as_int();
        if (idx < 0) {
            state->error("Array index không được âm.");
            return impl_PANIC(ip, state);
        }
        // Tự động resize nếu index >= size (giống JavaScript/Ruby)
        if (static_cast<size_t>(idx) >= arr->size()) {
            arr->resize(idx + 1);
        }
        arr->set(idx, val);
    }
    else if (src.is_hash_table()) {
        if (!key.is_string()) {
            state->error("Hash key phải là string.");
            return impl_PANIC(ip, state);
        }
        src.as_hash_table()->set(key.as_string(), val);
    }
    else {
        state->error("Không thể gán index [] trên kiểu dữ liệu này.");
        return impl_PANIC(ip, state);
    }
    return ip;
}

[[always_inline]] static const uint8_t* impl_GET_KEYS(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src_reg = read_u16(ip);
    Value& src = state->reg(src_reg);
    
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
    
    state->reg(dst) = Value(keys_array);
    return ip;
}

[[always_inline]] static const uint8_t* impl_GET_VALUES(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src_reg = read_u16(ip);
    Value& src = state->reg(src_reg);
    
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

    state->reg(dst) = Value(vals_array);
    return ip;
}

} // namespace meow::handlers
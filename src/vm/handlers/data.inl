#pragma once
// Chứa các handler cho Array, Hash, Index

inline void Machine::op_new_array(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    uint16_t start_idx = READ_U16();
    uint16_t count = READ_U16();
    auto array = heap_->new_array();
    array->reserve(count);
    for (size_t i = 0; i < count; ++i) {
        array->push(REGISTER(start_idx + i));
    }
    REGISTER(dst) = Value(object_t(array));
    printl("new_array r{}, r{}, {}", dst, start_idx, count);
    printl("is_array(): {}", REGISTER(dst).is_array());
}

inline void Machine::op_new_hash(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    uint16_t start_idx = READ_U16();
    uint16_t count = READ_U16();
    auto hash_table = heap_->new_hash();
    for (size_t i = 0; i < count; ++i) {
        Value& key = REGISTER(start_idx + i * 2);
        Value& val = REGISTER(start_idx + i * 2 + 1);
        if (!key.is_string()) {
            throw_vm_error("NEW_HASH: Key is not a string.");
        }
        hash_table->set(key.as_string(), val);
    }
    REGISTER(dst) = Value(hash_table);
}

inline void Machine::op_get_index(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    uint16_t src_reg = READ_U16();
    uint16_t key_reg = READ_U16();
    Value& src = REGISTER(src_reg);
    Value& key = REGISTER(key_reg);
    if (src.is_array()) {
        if (!key.is_int()) throw_vm_error("Array index must be an integer.");
        int64_t idx = key.as_int();
        array_t arr = src.as_array();
        if (idx < 0 || (uint64_t)idx >= arr->size()) {
            throw_vm_error("Array index out of bounds.");
        }
        REGISTER(dst) = arr->get(idx);
    } else if (src.is_hash_table()) {
        if (!key.is_string()) throw_vm_error("Hash table key must be a string.");
        hash_table_t hash = src.as_hash_table();
        if (hash->has(key.as_string())) {
            REGISTER(dst) = hash->get(key.as_string());
        } else {
            REGISTER(dst) = Value(null_t{});
        }
    } else if (src.is_string()) {
        if (!key.is_int()) throw_vm_error("String index must be an integer.");
        int64_t idx = key.as_int();
        string_t str = src.as_string();
        if (idx < 0 || (uint64_t)idx >= str->size()) {
            throw_vm_error("String index out of bounds.");
        }
        REGISTER(dst) = Value(heap_->new_string(std::string(1, str->get(idx))));
    } else {
        throw_vm_error("Cannot apply index operator to this type.");
    }
}

inline void Machine::op_set_index(const uint8_t*& ip) {
    uint16_t src_reg = READ_U16();
    uint16_t key_reg = READ_U16();
    uint16_t val_reg = READ_U16();
    Value& src = REGISTER(src_reg);
    Value& key = REGISTER(key_reg);
    Value& val = REGISTER(val_reg);
    if (src.is_array()) {
        if (!key.is_int()) throw_vm_error("Array index must be an integer.");
        int64_t idx = key.as_int();
        array_t arr = src.as_array();
        if (idx < 0) throw_vm_error("Array index cannot be negative.");
        if ((uint64_t)idx >= arr->size()) {
            arr->resize(idx + 1);
        }
        arr->set(idx, val);
    } else if (src.is_hash_table()) {
        if (!key.is_string()) throw_vm_error("Hash table key must be a string.");
        hash_table_t hash = src.as_hash_table();
        hash->set(key.as_string(), val);
    } else {
        throw_vm_error("Cannot apply index set operator to this type.");
    }
}

inline void Machine::op_get_keys(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    uint16_t src_reg = READ_U16();
    Value& src = REGISTER(src_reg);
    auto keys_array = heap_->new_array();
    if (src.is_hash_table()) {
        hash_table_t hash = src.as_hash_table();
        keys_array->reserve(hash->size());
        for (auto it = hash->begin(); it != hash->end(); ++it) {
            keys_array->push(Value(it->first));
        }
    }
    else if (src.is_array()) {
        array_t arr = src.as_array();
        keys_array->reserve(arr->size());
        for (size_t i = 0; i < arr->size(); ++i) {
            keys_array->push(Value(static_cast<int64_t>(i)));
        }
    } else if (src.is_string()) {
        string_t str = src.as_string();
        keys_array->reserve(str->size());
        for (size_t i = 0; i < str->size(); ++i) {
            keys_array->push(Value(static_cast<int64_t>(i)));
        }
    }
    REGISTER(dst) = Value(keys_array);
}

inline void Machine::op_get_values(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    uint16_t src_reg = READ_U16();
    Value& src = REGISTER(src_reg);
    auto vals_array = heap_->new_array();
    if (src.is_hash_table()) {
        hash_table_t hash = src.as_hash_table();
        vals_array->reserve(hash->size());
        for (auto it = hash->begin(); it != hash->end(); ++it) {
            vals_array->push(it->second);
        }
    } else if (src.is_array()) {
        array_t arr = src.as_array();
        vals_array->reserve(arr->size());
        for (size_t i = 0; i < arr->size(); ++i) {
            vals_array->push(arr->get(i));
        }
    } else if (src.is_string()) {
        string_t str = src.as_string();
        vals_array->reserve(str->size());
        for (size_t i = 0; i < str->size(); ++i) {
            vals_array->push(Value(heap_->new_string(std::string(1, str->get(i)))));
        }
    }
    REGISTER(dst) = Value(vals_array);
}
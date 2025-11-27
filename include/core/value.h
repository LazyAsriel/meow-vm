#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "meow_variant.h"
#include "core/meow_object.h"
#include "core/object_traits.h"

namespace meow {

using base_t = meow::variant<null_t, bool_t, int_t, float_t, native_t, object_t>;

class Value {
private:
    base_t data_;

    template <typename Self>
    inline auto get_object_ptr(this Self&& self) noexcept {
        if (auto obj_ptr = self.data_.template get_if<object_t>()) {
            return *obj_ptr;
        }
        using ret_t = std::remove_reference_t<decltype(*std::declval<object_t*>())>;
        return static_cast<ret_t>(nullptr);
    }

public:
    // --- Constructors ---
    inline Value() noexcept : data_(null_t{}) {}
    inline Value(null_t v) noexcept : data_(std::move(v)) {}
    inline Value(bool_t v) noexcept : data_(v) {}
    inline Value(int_t v) noexcept : data_(v) {}
    inline Value(float_t v) noexcept : data_(v) {}
    inline Value(object_t v) noexcept : data_(v) {}

    // --- Rule of five ---
    inline Value(const Value& other) noexcept : data_(other.data_) {}
    inline Value(Value&& other) noexcept : data_(std::move(other.data_)) {}

    inline Value& operator=(const Value& other) noexcept {
        if (this == &other) return *this;
        data_ = other.data_;
        return *this;
    }
    inline Value& operator=(Value&& other) noexcept {
        if (this == &other) return *this;
        data_ = std::move(other.data_);
        return *this;
    }
    inline ~Value() noexcept = default;

    // --- Assignment operators ---
    inline Value& operator=(null_t v) noexcept { data_ = std::move(v); return *this; }
    inline Value& operator=(bool_t v) noexcept { data_ = v; return *this; }
    inline Value& operator=(int_t v) noexcept { data_ = v; return *this; }
    inline Value& operator=(float_t v) noexcept { data_ = v; return *this; }
    inline Value& operator=(object_t v) noexcept { data_ = v; return *this; }

    inline constexpr size_t index() const noexcept {
        return data_.index();
    }

    // === Type Checkers ===

    // --- Primary type ---
    inline bool is_null() const noexcept { return data_.holds<null_t>(); }
    inline bool is_bool() const noexcept { return data_.holds<bool_t>(); }
    inline bool is_int() const noexcept { return data_.holds<int_t>(); }
    inline bool is_float() const noexcept { return data_.holds<float_t>(); }
    inline bool is_native() const noexcept { return data_.holds<native_t>(); }

    // --- Object type (generic) ---
    inline bool is_object() const noexcept {
        return get_object_ptr() != nullptr;
    }

    // --- Specific object type ---
    inline bool is_array() const noexcept {
        auto obj = get_object_ptr();
        return (obj && obj->get_type() == ObjectType::ARRAY);
    }
    inline bool is_string() const noexcept {
        auto obj = get_object_ptr();
        return (obj && obj->get_type() == ObjectType::STRING);
    }
    inline bool is_hash_table() const noexcept {
        auto obj = get_object_ptr();
        return (obj && obj->get_type() == ObjectType::HASH_TABLE);
    }
    inline bool is_upvalue() const noexcept {
        auto obj = get_object_ptr();
        return (obj && obj->get_type() == ObjectType::UPVALUE);
    }
    inline bool is_proto() const noexcept {
        auto obj = get_object_ptr();
        return (obj && obj->get_type() == ObjectType::PROTO);
    }
    inline bool is_function() const noexcept {
        auto obj = get_object_ptr();
        return (obj && obj->get_type() == ObjectType::FUNCTION);
    }
    inline bool is_class() const noexcept {
        auto obj = get_object_ptr();
        return (obj && obj->get_type() == ObjectType::CLASS);
    }
    inline bool is_instance() const noexcept {
        auto obj = get_object_ptr();
        return (obj && obj->get_type() == ObjectType::INSTANCE);
    }
    inline bool is_bound_method() const noexcept {
        auto obj = get_object_ptr();
        return (obj && obj->get_type() == ObjectType::BOUND_METHOD);
    }
    inline bool is_module() const noexcept {
        auto obj = get_object_ptr();
        return (obj && obj->get_type() == ObjectType::MODULE);
    }

    // === Accessors (Unsafe / By Value) ===
    inline bool as_bool() const noexcept { return data_.get<bool_t>(); }
    inline int64_t as_int() const noexcept { return data_.get<int_t>(); }
    inline double as_float() const noexcept { return data_.get<float_t>(); }
    inline native_t as_native() const noexcept { return data_.get<native_t>(); }
    
    inline MeowObject* as_object() const noexcept {
        return data_.get<object_t>();
    }
    inline array_t as_array() const noexcept {
        return reinterpret_cast<array_t>(as_object());
    }
    inline string_t as_string() const noexcept {
        return reinterpret_cast<string_t>(as_object());
    }
    inline hash_table_t as_hash_table() const noexcept {
        return reinterpret_cast<hash_table_t>(as_object());
    }
    inline upvalue_t as_upvalue() const noexcept {
        return reinterpret_cast<upvalue_t>(as_object());
    }
    inline proto_t as_proto() const noexcept {
        return reinterpret_cast<proto_t>(as_object());
    }
    inline function_t as_function() const noexcept {
        return reinterpret_cast<function_t>(as_object());
    }
    inline class_t as_class() const noexcept {
        return reinterpret_cast<class_t>(as_object());
    }
    inline instance_t as_instance() const noexcept {
        return reinterpret_cast<instance_t>(as_object());
    }
    inline bound_method_t as_bound_method() const noexcept {
        return reinterpret_cast<bound_method_t>(as_object());
    }
    inline module_t as_module() const noexcept {
        return reinterpret_cast<module_t>(as_object());
    }

    // === Safe Getters (Deducing 'this' - C++23) ===
    
    // --- Primitive Types ---
    template <typename Self>
    inline auto as_if_bool(this Self&& self) noexcept {
        return self.data_.template get_if<bool_t>();
    }

    template <typename Self>
    inline auto as_if_int(this Self&& self) noexcept {
        return self.data_.template get_if<int_t>();
    }

    template <typename Self>
    inline auto as_if_float(this Self&& self) noexcept {
        return self.data_.template get_if<float_t>();
    }

    template <typename Self>
    inline auto as_if_native(this Self&& self) noexcept {
        return self.data_.template get_if<native_t>();
    }

    // --- Object Types ---
    
    // Array
    template <typename Self>
    inline auto as_if_array(this Self&& self) noexcept {
        if (auto obj = self.get_object_ptr()) {
            if (obj->get_type() == ObjectType::ARRAY) {
                return reinterpret_cast<array_t>(obj);
            }
        }
        return static_cast<array_t>(nullptr);
    }

    // String
    template <typename Self>
    inline auto as_if_string(this Self&& self) noexcept {
        if (auto obj = self.get_object_ptr()) {
            if (obj->get_type() == ObjectType::STRING) {
                return reinterpret_cast<string_t>(obj);
            }
        }
        return static_cast<string_t>(nullptr);
    }

    // Hash table
    template <typename Self>
    inline auto as_if_hash_table(this Self&& self) noexcept {
        if (auto obj = self.get_object_ptr()) {
            if (obj->get_type() == ObjectType::HASH_TABLE) {
                return reinterpret_cast<hash_table_t>(obj);
            }
        }
        return static_cast<hash_table_t>(nullptr);
    }

    // Upvalue
    template <typename Self>
    inline auto as_if_upvalue(this Self&& self) noexcept {
        if (auto obj = self.get_object_ptr()) {
            if (obj->get_type() == ObjectType::UPVALUE) {
                return reinterpret_cast<upvalue_t>(obj);
            }
        }
        return static_cast<upvalue_t>(nullptr);
    }

    // Proto
    template <typename Self>
    inline auto as_if_proto(this Self&& self) noexcept {
        if (auto obj = self.get_object_ptr()) {
            if (obj->get_type() == ObjectType::PROTO) {
                return reinterpret_cast<proto_t>(obj);
            }
        }
        return static_cast<proto_t>(nullptr);
    }

    // Function
    template <typename Self>
    inline auto as_if_function(this Self&& self) noexcept {
        if (auto obj = self.get_object_ptr()) {
            if (obj->get_type() == ObjectType::FUNCTION) {
                return reinterpret_cast<function_t>(obj);
            }
        }
        return static_cast<function_t>(nullptr);
    }

    // Class
    template <typename Self>
    inline auto as_if_class(this Self&& self) noexcept {
        if (auto obj = self.get_object_ptr()) {
            if (obj->get_type() == ObjectType::CLASS) {
                return reinterpret_cast<class_t>(obj);
            }
        }
        return static_cast<class_t>(nullptr);
    }

    // Instance
    template <typename Self>
    inline auto as_if_instance(this Self&& self) noexcept {
        if (auto obj = self.get_object_ptr()) {
            if (obj->get_type() == ObjectType::INSTANCE) {
                return reinterpret_cast<instance_t>(obj);
            }
        }
        return static_cast<instance_t>(nullptr);
    }

    // Bound method
    template <typename Self>
    inline auto as_if_bound_method(this Self&& self) noexcept {
        if (auto obj = self.get_object_ptr()) {
            if (obj->get_type() == ObjectType::BOUND_METHOD) {
                return reinterpret_cast<bound_method_t>(obj);
            }
        }
        return static_cast<bound_method_t>(nullptr);
    }

    // Module
    template <typename Self>
    inline auto as_if_module(this Self&& self) noexcept {
        if (auto obj = self.get_object_ptr()) {
            if (obj->get_type() == ObjectType::MODULE) {
                return reinterpret_cast<module_t>(obj);
            }
        }
        return static_cast<module_t>(nullptr);
    }

    // === Visitor ===
    // (Cũng có thể dùng deducing this để gộp, nhưng giữ nguyên cũng tốt)
    template <typename Visitor>
    decltype(auto) visit(Visitor&& vis) {
        return data_.visit(vis);
    }
    
    template <typename Visitor>
    decltype(auto) visit(Visitor&& vis) const {
        return data_.visit(vis);
    }

    template <typename... Fs>
    decltype(auto) visit(Fs&&... fs) {
        return data_.visit(std::forward<Fs>(fs)...);
    }

    template <typename... Fs>
    decltype(auto) visit(Fs&&... fs) const {
        return data_.visit(std::forward<Fs>(fs)...);
    }
};

}
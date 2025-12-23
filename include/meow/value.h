#pragma once

#include <utility>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <meow/common.h>
#include "meow_variant.h"
#include <meow/core/meow_object.h>

namespace meow {

class Value {
private:
    base_t data_;

    // --- Private Helpers ---

    template <typename Self>
    inline auto get_object_ptr(this Self&& self) noexcept -> object_t {
        if (auto* val_ptr = self.data_.template get_if<object_t>()) {
            return *val_ptr;
        }
        return nullptr;
    }

    inline bool check_obj_type(ObjectType type) const noexcept {
        auto obj = get_object_ptr();
        return (obj && obj->get_type() == type);
    }

    template <typename TargetType, ObjectType Type, typename Self>
    inline auto get_obj_if(this Self&& self) noexcept {
        if (auto obj = self.get_object_ptr()) {
            if (obj->get_type() == Type) {
                return reinterpret_cast<TargetType>(obj);
            }
        }
        return static_cast<TargetType>(nullptr);
    }

public:
    using layout_traits = base_t::layout_traits;

    // --- Constructors & Assignments ---
    
    inline Value() noexcept : data_(null_t{}) {}
    
    inline Value(const Value&) noexcept = default;
    inline Value(Value&&) noexcept = default;
    inline Value& operator=(const Value&) noexcept = default;
    inline Value& operator=(Value&&) noexcept = default;
    inline ~Value() noexcept = default;

    template <typename T>
    requires (std::is_convertible_v<T, MeowObject*> && !std::is_same_v<std::decay_t<T>, Value>)
    inline Value(T&& v) noexcept : data_(static_cast<MeowObject*>(v)) {}

    template <typename T>
    requires (std::is_convertible_v<T, MeowObject*> && !std::is_same_v<std::decay_t<T>, Value>)
    inline Value& operator=(T&& v) noexcept {
        data_ = static_cast<MeowObject*>(v);
        return *this;
    }

    template <typename T>
    requires (!std::is_convertible_v<T, MeowObject*> && !std::is_same_v<std::decay_t<T>, Value>)
    inline Value(T&& v) noexcept : data_(std::forward<T>(v)) {}

    template <typename T>
    requires (!std::is_convertible_v<T, MeowObject*> && !std::is_same_v<std::decay_t<T>, Value>)
    inline Value& operator=(T&& v) noexcept {
        data_ = std::forward<T>(v);
        return *this;
    }

    // --- Operators ---

    inline bool operator==(const Value& other) const noexcept { return data_ == other.data_; }
    inline bool operator!=(const Value& other) const noexcept { return data_ != other.data_; }

    // --- Core Access ---

    inline constexpr size_t index() const noexcept { return data_.index(); }
    inline uint64_t raw_tag() const noexcept { return data_.raw_tag(); }
    inline void set_raw(uint64_t bits) noexcept { data_.set_raw(bits); }
    inline uint64_t raw() const noexcept { return data_.raw(); }

    template <typename T>
    inline bool holds_both(const Value& other) const noexcept {
        return data_.template holds_both<T>(other.data_);
    }

    static inline Value from_raw(uint64_t bits) noexcept {
        Value v; v.set_raw(bits); return v;
    }

    // === Type Checkers ===

    inline bool is_null() const noexcept { return data_.holds<null_t>(); }
    inline bool is_bool() const noexcept { return data_.holds<bool_t>(); }
    inline bool is_int() const noexcept { return data_.holds<int_t>(); }
    inline bool is_float() const noexcept { return data_.holds<float_t>(); }
    inline bool is_native() const noexcept { return data_.holds<native_t>(); }
    inline bool is_pointer() const noexcept { return data_.holds<pointer_t>(); }
    inline bool is_object() const noexcept { return get_object_ptr() != nullptr; }

    inline bool is_array() const noexcept        { return check_obj_type(ObjectType::ARRAY); }
    inline bool is_string() const noexcept       { return check_obj_type(ObjectType::STRING); }
    inline bool is_hash_table() const noexcept   { return check_obj_type(ObjectType::HASH_TABLE); }
    inline bool is_upvalue() const noexcept      { return check_obj_type(ObjectType::UPVALUE); }
    inline bool is_proto() const noexcept        { return check_obj_type(ObjectType::PROTO); }
    inline bool is_function() const noexcept     { return check_obj_type(ObjectType::FUNCTION); }
    inline bool is_class() const noexcept        { return check_obj_type(ObjectType::CLASS); }
    inline bool is_instance() const noexcept     { return check_obj_type(ObjectType::INSTANCE); }
    inline bool is_bound_method() const noexcept { return check_obj_type(ObjectType::BOUND_METHOD); }
    inline bool is_module() const noexcept       { return check_obj_type(ObjectType::MODULE); }

    // === Unsafe Accessors ===

    inline bool as_bool() const noexcept       { return data_.get<bool_t>(); }
    inline int64_t as_int() const noexcept     { return data_.get<int_t>(); }
    inline double as_float() const noexcept    { return data_.get<float_t>(); }
    inline native_t as_native() const noexcept { return data_.get<native_t>(); }
    inline pointer_t as_pointer() const noexcept { return data_.get<pointer_t>(); }
    
    inline MeowObject* as_object() const noexcept { return data_.get<object_t>(); }

    template <typename T>
    inline T as_obj_unsafe() const noexcept { return reinterpret_cast<T>(as_object()); }

    inline array_t as_array() const noexcept               { return as_obj_unsafe<array_t>(); }
    inline string_t as_string() const noexcept             { return as_obj_unsafe<string_t>(); }
    inline hash_table_t as_hash_table() const noexcept     { return as_obj_unsafe<hash_table_t>(); }
    inline upvalue_t as_upvalue() const noexcept           { return as_obj_unsafe<upvalue_t>(); }
    inline proto_t as_proto() const noexcept               { return as_obj_unsafe<proto_t>(); }
    inline function_t as_function() const noexcept         { return as_obj_unsafe<function_t>(); }
    inline class_t as_class() const noexcept               { return as_obj_unsafe<class_t>(); }
    inline instance_t as_instance() const noexcept         { return as_obj_unsafe<instance_t>(); }
    inline bound_method_t as_bound_method() const noexcept { return as_obj_unsafe<bound_method_t>(); }
    inline module_t as_module() const noexcept             { return as_obj_unsafe<module_t>(); }

    // === Safe Getters (Deducing 'this') ===

    template <typename Self>
    auto as_if_bool(this Self&& self) noexcept   { return self.data_.template get_if<bool_t>(); }
    template <typename Self>
    auto as_if_int(this Self&& self) noexcept    { return self.data_.template get_if<int_t>(); }
    template <typename Self>
    auto as_if_float(this Self&& self) noexcept  { return self.data_.template get_if<float_t>(); }
    template <typename Self>
    auto as_if_native(this Self&& self) noexcept { return self.data_.template get_if<native_t>(); }
    template <typename Self>
    auto as_if_pointer(this Self&& self) noexcept  { return self.data_.template get_if<pointer_t>(); }

    // Objects
    template <typename Self> auto as_if_array(this Self&& self) noexcept        { return self.template get_obj_if<array_t, ObjectType::ARRAY>(); }
    template <typename Self> auto as_if_string(this Self&& self) noexcept       { return self.template get_obj_if<string_t, ObjectType::STRING>(); }
    template <typename Self> auto as_if_hash_table(this Self&& self) noexcept   { return self.template get_obj_if<hash_table_t, ObjectType::HASH_TABLE>(); }
    template <typename Self> auto as_if_upvalue(this Self&& self) noexcept      { return self.template get_obj_if<upvalue_t, ObjectType::UPVALUE>(); }
    template <typename Self> auto as_if_proto(this Self&& self) noexcept        { return self.template get_obj_if<proto_t, ObjectType::PROTO>(); }
    template <typename Self> auto as_if_function(this Self&& self) noexcept     { return self.template get_obj_if<function_t, ObjectType::FUNCTION>(); }
    template <typename Self> auto as_if_class(this Self&& self) noexcept        { return self.template get_obj_if<class_t, ObjectType::CLASS>(); }
    template <typename Self> auto as_if_instance(this Self&& self) noexcept     { return self.template get_obj_if<instance_t, ObjectType::INSTANCE>(); }
    template <typename Self> auto as_if_bound_method(this Self&& self) noexcept { return self.template get_obj_if<bound_method_t, ObjectType::BOUND_METHOD>(); }
    template <typename Self> auto as_if_module(this Self&& self) noexcept       { return self.template get_obj_if<module_t, ObjectType::MODULE>(); }

    // === Visitor ===
    template <typename... Fs>
    decltype(auto) visit(Fs&&... fs) const { return data_.visit(std::forward<Fs>(fs)...); }
    
    template <typename... Fs>
    decltype(auto) visit(Fs&&... fs) { return data_.visit(std::forward<Fs>(fs)...); }
};

}
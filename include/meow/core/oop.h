/**
 * @file oop.h
 * @author LazyPaws
 * @brief Core definition of Class, Instance, BoundMethod in TrangMeo
 * @copyright Copyright (c) 2025 LazyPaws
 * @license All rights reserved. Unauthorized copying of this file, in any form
 * or medium, is strictly prohibited
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <variant>
#include <memory>
#include <meow/definitions.h>
#include <meow/core/meow_object.h>
#include <meow/definitions.h>
#include <meow/value.h>
#include <meow/memory/gc_visitor.h>
#include <meow/core/shape.h>
#include <meow_flat_map.h> // [NEW]

namespace meow {
class ObjClass : public ObjBase<ObjectType::CLASS> {
   private:
    using string_t = string_t;
    using class_t = class_t;
    using method_map = meow::flat_map<string_t, value_t>;
    using visitor_t = GCVisitor;

    string_t name_;
    class_t superclass_;
    method_map methods_;

   public:
    explicit ObjClass(string_t name = nullptr) noexcept : name_(name) {
    }

    // --- Metadata ---
    inline string_t get_name() const noexcept {
        return name_;
    }
    inline class_t get_super() const noexcept {
        return superclass_;
    }
    inline void set_super(class_t super) noexcept {
        superclass_ = super;
    }

    // --- Methods ---
    inline bool has_method(string_t name) const noexcept {
        return methods_.contains(name);
    }
    
    inline return_t get_method(string_t name) noexcept {
        auto idx = methods_.index_of(name);
        if (idx != method_map::npos) {
            return methods_.unsafe_get(idx);
        }
        return Value(null_t{});
    }
    
    inline void set_method(string_t name, return_t value) noexcept {
        methods_[name] = value;
    }

    inline const method_map& get_methods_raw() const { return methods_; }

    void trace(visitor_t& visitor) const noexcept override;
};

class ObjInstance : public ObjBase<ObjectType::INSTANCE> {
   private:
    using string_t = string_t;
    using class_t = class_t;
    using visitor_t = GCVisitor;

    class_t klass_;
    Shape* shape_;              
    std::vector<Value> fields_; 

   public:
    explicit ObjInstance(class_t k, Shape* empty_shape) noexcept 
        : klass_(k), shape_(empty_shape) {
    }

    // --- Metadata ---
    inline class_t get_class() const noexcept { return klass_; }
    inline void set_class(class_t klass) noexcept { klass_ = klass; }

    inline Shape* get_shape() const noexcept { return shape_; }
    inline void set_shape(Shape* s) noexcept { shape_ = s; }

    // --- Fast Field Access (By Index) ---
    inline Value get_field_at(int offset) const noexcept {
        return fields_[offset];
    }
    
    inline void set_field_at(int offset, Value value) noexcept {
        fields_[offset] = value;
    }
    
    // --- Raw Field Access (Cho Transition) ---
    inline std::vector<Value>& get_fields_raw() { return fields_; }

    // --- Slow API ---
    inline bool has_field(string_t name) const {
        return shape_->get_offset(name) != -1;
    }
    
    inline Value get_field(string_t name) const {
        int offset = shape_->get_offset(name);
        if (offset != -1) return fields_[offset];
        return Value(null_t{});
    }

    void trace(visitor_t& visitor) const noexcept override {
        visitor.visit_object(klass_);
        visitor.visit_object(shape_);
        for (const auto& val : fields_) {
            visitor.visit_value(val);
        }
    }
};

class ObjBoundMethod : public ObjBase<ObjectType::BOUND_METHOD> {
   private:
    using instance_t = instance_t;
    using function_t = function_t;
    using visitor_t = GCVisitor;

    instance_t instance_;
    function_t function_;

   public:
    explicit ObjBoundMethod(instance_t instance = nullptr, function_t function = nullptr) noexcept : instance_(instance), function_(function) {
    }

    inline instance_t get_instance() const noexcept {
        return instance_;
    }
    inline function_t get_function() const noexcept {
        return function_;
    }

    void trace(visitor_t& visitor) const noexcept override;
};
}
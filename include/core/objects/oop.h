/**
 * @file oop.h
 * @author LazyPaws
 * @brief Core definition of Class, Instance, BoundMethod in TrangMeo
 * @copyright Copyright (c) 2025 LazyPaws
 * @license All rights reserved. Unauthorized copying of this file, in any form
 * or medium, is strictly prohibited
 */

#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "core/meow_object.h"
#include "common/definitions.h"
#include "core/value.h"
#include "memory/gc_visitor.h"

namespace meow {
class ObjClass : public ObjBase<ObjectType::CLASS> {
   private:
    using string_t = string_t;
    using class_t = class_t;
    using method_map = std::unordered_map<string_t, value_t>;
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
        return methods_.find(name) != methods_.end();
    }
    inline return_t get_method(string_t name) noexcept {
        return methods_[name];
    }
    inline void set_method(string_t name, return_t value) noexcept {
        methods_[name] = value;
    }

    void trace(visitor_t& visitor) const noexcept override;
};

class ObjInstance : public ObjBase<ObjectType::INSTANCE> {
   private:
    using string_t = string_t;
    using class_t = class_t;
    using field_map = std::unordered_map<string_t, value_t>;
    using visitor_t = GCVisitor;

    class_t klass_;
    field_map fields_;

   public:
    explicit ObjInstance(class_t k = nullptr) noexcept : klass_(k) {
    }

    // --- Metadata ---
    inline class_t get_class() const noexcept {
        return klass_;
    }
    inline void set_class(class_t klass) noexcept {
        klass_ = klass;
    }

    // --- Fields ---
    inline return_t get_field(string_t name) noexcept {
        return fields_[name];
    }
    inline void set_field(string_t name, param_t value) noexcept {
        fields_[name] = value;
    }
    inline bool has_field(string_t name) const {
        return fields_.find(name) != fields_.end();
    }

    void trace(visitor_t& visitor) const noexcept override;
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
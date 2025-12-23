#pragma once

#include <meow/common.h>
#include <meow/core/meow_object.h>
#include <meow/common.h>
#include <meow/value.h>
#include <meow/memory/gc_visitor.h>
#include <meow/core/shape.h>
#include <meow_flat_map.h>
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace meow {
class ObjClass : public ObjBase<ObjectType::CLASS> {
private:
    using method_map = meow::flat_map<string_t, value_t>;

    string_t name_;
    class_t superclass_;
    method_map methods_;

public:
    explicit ObjClass(string_t name = nullptr) noexcept : name_(name) {}

    inline string_t get_name() const noexcept {
        return name_;
    }
    inline class_t get_super() const noexcept {
        return superclass_;
    }
    inline void set_super(class_t super) noexcept {
        superclass_ = super;
    }

    inline bool has_method(string_t name) const noexcept {
        return methods_.contains(name);
    }
    
    inline return_t get_method(string_t name) noexcept {
        if (auto* val_ptr = methods_.find(name)) {
            return *val_ptr;
        }
        return Value(null_t{});
    }
    
    inline void set_method(string_t name, param_t value) noexcept {
        methods_[name] = value;
    }

    void trace(GCVisitor& visitor) const noexcept override;
};

class ObjInstance : public ObjBase<ObjectType::INSTANCE> {
private:
    class_t klass_;
    Shape* shape_;              
    std::vector<Value> fields_; 
public:
    explicit ObjInstance(class_t k, Shape* empty_shape) noexcept 
        : klass_(k), shape_(empty_shape) {
    }

    inline class_t get_class() const noexcept { return klass_; }
    inline void set_class(class_t klass) noexcept { klass_ = klass; }

    inline Shape* get_shape() const noexcept { return shape_; }
    inline void set_shape(Shape* s) noexcept { shape_ = s; }

    inline Value get_field_at(int offset) const noexcept {
        return fields_[offset];
    }
    
    inline void set_field_at(int offset, Value value) noexcept {
        fields_[offset] = value;
    }
    
    inline void add_field(param_t value) noexcept {
        fields_.push_back(value);
    }

    inline bool has_field(string_t name) const noexcept {
        return shape_->get_offset(name) != -1;
    }
    
    inline Value get_field(string_t name) const noexcept {
        int offset = shape_->get_offset(name);
        if (offset != -1) return fields_[offset];
        return Value(null_t{});
    }

    inline void trace(GCVisitor& visitor) const noexcept override {
        visitor.visit_object(klass_);
        visitor.visit_object(shape_);
        for (const auto& val : fields_) {
            visitor.visit_value(val);
        }
    }
};

class ObjBoundMethod : public ObjBase<ObjectType::BOUND_METHOD> {
private:
    Value receiver_; 
    Value method_;   
public:
    explicit ObjBoundMethod(Value receiver, Value method) noexcept 
        : receiver_(receiver), method_(method) {}

    inline Value get_receiver() const noexcept { return receiver_; }
    inline Value get_method() const noexcept { return method_; }

    inline void trace(GCVisitor& visitor) const noexcept override {
        visitor.visit_value(receiver_);
        visitor.visit_value(method_);
    }
};
}
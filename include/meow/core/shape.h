#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <variant>
#include <memory>
#include <meow/definitions.h>
#include <meow/core/meow_object.h>
#include <meow/memory/gc_visitor.h>
#include <meow/core/string.h>
#include <meow_flat_map.h>

namespace meow {

class MemoryManager; 

class Shape : public ObjBase<ObjectType::SHAPE> {
public:
    using TransitionMap = meow::flat_map<string_t, Shape*>;
    using PropertyMap = meow::flat_map<string_t, uint32_t>;

private:
    PropertyMap property_offsets_;
    TransitionMap transitions_;     
    uint32_t num_fields_ = 0;       

public:
    explicit Shape() = default;

    int get_offset(string_t name) const;

    Shape* get_transition(string_t name) const;

    Shape* add_transition(string_t name, MemoryManager* heap);

    inline uint32_t count() const { return num_fields_; }
    
    void copy_from(const Shape* other) {
        property_offsets_ = other->property_offsets_;
        num_fields_ = other->num_fields_;
    }
    
    void add_property(string_t name) {
        property_offsets_[name] = num_fields_++;
    }

    size_t obj_size() const noexcept override { return sizeof(Shape); }

    void trace(GCVisitor& visitor) const noexcept override;
};

}
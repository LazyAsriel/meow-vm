#pragma once
#include "common/pch.h"
#include "common/definitions.h"
#include "core/meow_object.h"

namespace meow {

class MemoryManager; 

class Shape : public ObjBase<ObjectType::SHAPE> {
public:
    using TransitionMap = std::unordered_map<string_t, Shape*>;
    using PropertyMap   = std::unordered_map<string_t, uint32_t>;

private:
    PropertyMap property_offsets_;  // "x" -> 0, "y" -> 1
    TransitionMap transitions_;     // Cây chuyển đổi trạng thái
    Shape* parent_ = nullptr;       
    uint32_t num_fields_ = 0;       

public:
    explicit Shape(uint32_t num_fields = 0) : num_fields_(num_fields) {}

    // --- Fast Lookup ---
    inline int get_offset(string_t name) const {
        auto it = property_offsets_.find(name);
        if (it != property_offsets_.end()) return static_cast<int>(it->second);
        return -1;
    }

    // --- Transitions ---
    Shape* get_transition(string_t name) const {
        auto it = transitions_.find(name);
        return (it != transitions_.end()) ? it->second : nullptr;
    }

    Shape* add_transition(string_t name, MemoryManager* heap);

    inline uint32_t count() const { return num_fields_; }
    
    void copy_offsets_from(const Shape* other) {
        property_offsets_ = other->property_offsets_;
    }
    
    void set_offset(string_t name, uint32_t offset) {
        property_offsets_[name] = offset;
    }

    void trace(GCVisitor& visitor) const noexcept override;
};

}
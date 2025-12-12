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

namespace meow {

class MemoryManager; 

class Shape : public ObjBase<ObjectType::SHAPE> {
public:
    using TransitionMap = std::unordered_map<string_t, Shape*>;
    using PropertyMap = std::unordered_map<string_t, uint32_t, ObjStringHasher>;

private:
    PropertyMap property_offsets_;  // Map: "x" -> 0, "y" -> 1
    TransitionMap transitions_;     // Map: Chuyển đổi trạng thái khi thêm prop mới
    uint32_t num_fields_ = 0;       

public:
    explicit Shape() = default;

    // --- Fast Lookup ---
    // Trả về index của property trong mảng fields của Instance
    inline int get_offset(string_t name) const {
        auto it = property_offsets_.find(name);
        if (it != property_offsets_.end()) return static_cast<int>(it->second);
        return -1;
    }

    // --- Transitions ---
    // Tìm đường đi tiếp theo nếu thêm property 'name'
    Shape* get_transition(string_t name) const {
        auto it = transitions_.find(name);
        return (it != transitions_.end()) ? it->second : nullptr;
    }

    // Tạo đường đi mới (Fork)
    Shape* add_transition(string_t name, MemoryManager* heap);

    inline uint32_t count() const { return num_fields_; }
    
    // Copy dữ liệu từ Shape cha (dùng khi tạo Shape mới)
    void copy_from(const Shape* other) {
        property_offsets_ = other->property_offsets_;
        num_fields_ = other->num_fields_;
    }
    
    void add_property(string_t name) {
        property_offsets_[name] = num_fields_++;
    }

    void trace(GCVisitor& visitor) const noexcept override;
};

}
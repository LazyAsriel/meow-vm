#include <meow/core/shape.h>
#include <meow/memory/memory_manager.h>

namespace meow {

int Shape::get_offset(string_t name) const {
    auto idx = property_offsets_.index_of(name);
    if (idx != PropertyMap::npos) {
        return static_cast<int>(property_offsets_.unsafe_get(idx));
    }
    return -1;
}

Shape* Shape::get_transition(string_t name) const {
    auto idx = transitions_.index_of(name);
    if (idx != TransitionMap::npos) {
        return transitions_.unsafe_get(idx);
    }
    return nullptr;
}

Shape* Shape::add_transition(string_t name, MemoryManager* heap) {
    Shape* new_shape = heap->new_shape();
    
    new_shape->copy_from(this);
    
    new_shape->add_property(name);

    transitions_[name] = new_shape;
    
    return new_shape;
}

void Shape::trace(GCVisitor& visitor) const noexcept {
    const auto& prop_keys = property_offsets_.keys();
    for (auto key : prop_keys) {
        visitor.visit_object(key);
    }

    const auto& trans_keys = transitions_.keys();
    const auto& trans_vals = transitions_.values();
    
    for (size_t i = 0; i < trans_keys.size(); ++i) {
        visitor.visit_object(trans_keys[i]);
        visitor.visit_object(trans_vals[i]);
    }
}

}
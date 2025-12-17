#include <meow/core/shape.h>
#include <meow/memory/memory_manager.h>

namespace meow {

int Shape::get_offset(string_t name) const {
    if (const uint32_t* ptr = property_offsets_.find(name)) {
        return static_cast<int>(*ptr);
    }
    return -1;
}

Shape* Shape::get_transition(string_t name) const {
    if (Shape* const* ptr = transitions_.find(name)) {
        return *ptr;
    }
    return nullptr;
}

Shape* Shape::add_transition(string_t name, MemoryManager* heap) {
    heap->disable_gc();
    Shape* new_shape = heap->new_shape();
    
    new_shape->copy_from(this);
    new_shape->add_property(name);

    transitions_.try_emplace(name, new_shape);
    
    heap->write_barrier(this, Value(reinterpret_cast<object_t>(new_shape))); 
    heap->enable_gc();
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
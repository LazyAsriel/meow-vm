#include <meow/core/objects.h>
#include <meow/memory/gc_visitor.h>
#include <meow/memory/memory_manager.h>

namespace meow {

void ObjArray::trace(GCVisitor& visitor) const noexcept {
    for (const auto& element : elements_) {
        visitor.visit_value(element);
    }
}

void ObjClass::trace(GCVisitor& visitor) const noexcept {
    visitor.visit_object(name_);
    visitor.visit_object(superclass_);
    
    const auto& keys = methods_.keys();
    const auto& vals = methods_.values();
    const size_t size = keys.size();
    for (size_t i = 0; i < size; ++i) {
        visitor.visit_object(keys[i]);
        visitor.visit_value(vals[i]);
    }
}

void ObjUpvalue::trace(GCVisitor& visitor) const noexcept {
    visitor.visit_value(closed_);
}

void ObjFunctionProto::trace(GCVisitor& visitor) const noexcept {
    visitor.visit_object(name_);
    visitor.visit_object(module_);
    for (size_t i = 0; i < chunk_.get_pool_size(); ++i) {
        visitor.visit_value(chunk_.get_constant(i));
    }
}

void ObjClosure::trace(GCVisitor& visitor) const noexcept {
    visitor.visit_object(proto_);
    for (const auto& upvalue : upvalues_) {
        visitor.visit_object(upvalue);
    }
}

void ObjModule::trace(GCVisitor& visitor) const noexcept {
    visitor.visit_object(file_name_);
    visitor.visit_object(file_path_);
    visitor.visit_object(main_proto_);

    for (const auto& val : globals_store_) {
        visitor.visit_value(val);
    }
    
    const auto& g_keys = global_names_.keys();
    for (auto key : g_keys) {
        visitor.visit_object(key);
    }

    const auto& e_keys = exports_.keys();
    const auto& e_vals = exports_.values();
    
    const size_t e_size = e_keys.size();
    for (size_t i = 0; i < e_size; ++i) {
        visitor.visit_object(e_keys[i]);
        visitor.visit_value(e_vals[i]);
    }
}

}
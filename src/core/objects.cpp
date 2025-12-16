#include <meow/core/objects.h>
#include <meow/memory/gc_visitor.h>
#include <meow/memory/memory_manager.h>

namespace meow {

void ObjArray::trace(GCVisitor& visitor) const noexcept {
    for (const auto& element : elements_) {
        visitor.visit_value(element);
    }
}

void ObjHashTable::trace(GCVisitor& visitor) const noexcept {
    for (const auto& [key, value] : fields_) {
        visitor.visit_object(key);
        visitor.visit_value(value);
    }
}

void ObjClass::trace(GCVisitor& visitor) const noexcept {
    visitor.visit_object(name_);
    visitor.visit_object(superclass_);
    
    const auto& method_keys = get_methods_raw().keys();
    const auto& method_vals = get_methods_raw().values();
    
    for (size_t i = 0; i < method_keys.size(); ++i) {
        visitor.visit_object(method_keys[i]);
        visitor.visit_value(method_vals[i]);
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
    
    const auto& g_keys = get_global_names_raw().keys();
    for (auto key : g_keys) {
        visitor.visit_object(key);
    }

    const auto& e_keys = get_exports_raw().keys();
    const auto& e_vals = get_exports_raw().values();
    
    for (size_t i = 0; i < e_keys.size(); ++i) {
        visitor.visit_object(e_keys[i]);
        visitor.visit_value(e_vals[i]);
    }
}

}
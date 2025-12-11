#include "core/objects.h"
#include "memory/gc_visitor.h"

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
    for (const auto& [name, method] : methods_) {
        visitor.visit_object(name);
        visitor.visit_value(method);
    }
}

void ObjBoundMethod::trace(GCVisitor& visitor) const noexcept {
    visitor.visit_object(instance_);
    visitor.visit_object(function_);
}

void ObjUpvalue::trace(GCVisitor& visitor) const noexcept {
    visitor.visit_value(closed_);
}

void ObjFunctionProto::trace(GCVisitor& visitor) const noexcept {
    visitor.visit_object(name_);
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

// [FIXED] Cập nhật trace cho cấu trúc Module mới
void ObjModule::trace(GCVisitor& visitor) const noexcept {
    visitor.visit_object(file_name_);
    visitor.visit_object(file_path_);
    visitor.visit_object(main_proto_);

    // Trace mảng giá trị Globals
    for (const auto& val : globals_store_) {
        visitor.visit_value(val);
    }
    
    // Trace các tên biến Global (String keys)
    for (const auto& [key, idx] : global_names_) {
        visitor.visit_object(key);
    }

    // Trace Exports
    for (const auto& [key, value] : exports_) {
        visitor.visit_object(key);
        visitor.visit_value(value);
    }
}

}
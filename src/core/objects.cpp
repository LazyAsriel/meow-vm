#include "core/objects.h"
#include "memory/gc_visitor.h"
#include "memory/memory_manager.h"

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


// void MeowObject::trace(GCVisitor& visitor) const noexcept {
//     switch (type) {
//         case ObjectType::STRING:
//             break;
        
//         case ObjectType::UPVALUE:      
//             static_cast<const ObjUpvalue*>(this)->trace(visitor); 
//             break;
//         case ObjectType::ARRAY:        
//             static_cast<const ObjArray*>(this)->trace(visitor); 
//             break;
//         case ObjectType::HASH_TABLE:   
//             static_cast<const ObjHashTable*>(this)->trace(visitor); 
//             break;
//         case ObjectType::FUNCTION:     
//             static_cast<const ObjClosure*>(this)->trace(visitor); 
//             break;
//         case ObjectType::PROTO:        
//             static_cast<const ObjFunctionProto*>(this)->trace(visitor); 
//             break;
//         case ObjectType::CLASS:        
//             static_cast<const ObjClass*>(this)->trace(visitor); 
//             break;
//         case ObjectType::INSTANCE:     
//             static_cast<const ObjInstance*>(this)->trace(visitor); 
//             break;
//         case ObjectType::BOUND_METHOD: 
//             static_cast<const ObjBoundMethod*>(this)->trace(visitor); 
//             break;
//         case ObjectType::MODULE:       
//             static_cast<const ObjModule*>(this)->trace(visitor); 
//             break;
//         case ObjectType::SHAPE:        
//             static_cast<const Shape*>(this)->trace(visitor); 
//             break;
            
//         default: 
//             // Nếu có type lạ thì toang, nhưng runtime chuẩn sẽ không bao giờ vào đây
//             break; 
//     }
// }

// void MeowObject::destroy(MemoryManager* heap) noexcept {
//     switch (type) {
//         case ObjectType::STRING:
//             static_cast<ObjString*>(this)->~ObjString();
//             heap->deallocate_raw(this, sizeof(ObjString));
//             break;

//         case ObjectType::UPVALUE:
//             static_cast<ObjUpvalue*>(this)->~ObjUpvalue();
//             heap->deallocate_raw(this, sizeof(ObjUpvalue));
//             break;

//         case ObjectType::ARRAY:
//             static_cast<ObjArray*>(this)->~ObjArray();
//             heap->deallocate_raw(this, sizeof(ObjArray));
//             break;

//         case ObjectType::HASH_TABLE:
//             static_cast<ObjHashTable*>(this)->~ObjHashTable();
//             heap->deallocate_raw(this, sizeof(ObjHashTable));
//             break;

//         case ObjectType::FUNCTION:
//             static_cast<ObjClosure*>(this)->~ObjClosure();
//             heap->deallocate_raw(this, sizeof(ObjClosure));
//             break;

//         case ObjectType::PROTO:
//             static_cast<ObjFunctionProto*>(this)->~ObjFunctionProto();
//             heap->deallocate_raw(this, sizeof(ObjFunctionProto));
//             break;

//         case ObjectType::CLASS:
//             static_cast<ObjClass*>(this)->~ObjClass();
//             heap->deallocate_raw(this, sizeof(ObjClass));
//             break;

//         case ObjectType::INSTANCE:
//             static_cast<ObjInstance*>(this)->~ObjInstance();
//             heap->deallocate_raw(this, sizeof(ObjInstance));
//             break;

//         case ObjectType::BOUND_METHOD:
//             static_cast<ObjBoundMethod*>(this)->~ObjBoundMethod();
//             heap->deallocate_raw(this, sizeof(ObjBoundMethod));
//             break;

//         case ObjectType::MODULE:
//             static_cast<ObjModule*>(this)->~ObjModule();
//             heap->deallocate_raw(this, sizeof(ObjModule));
//             break;

//         case ObjectType::SHAPE:
//             static_cast<Shape*>(this)->~Shape();
//             heap->deallocate_raw(this, sizeof(Shape));
//             break;

//         default:
//             break;
//     }
// }
}
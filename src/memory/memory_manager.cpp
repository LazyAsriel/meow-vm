#include <meow/memory/memory_manager.h>
#include <meow/core/objects.h>

namespace meow {

MemoryManager::MemoryManager(std::unique_ptr<GarbageCollector> gc) noexcept 
    : arena_(64 * 1024), 
      heap_(arena_),
      gc_(std::move(gc)), 
      gc_threshold_(1024 * 1024), 
      object_allocated_(0) 
{ 
    if (gc_) {
        gc_->set_heap(&heap_);
    }
}
MemoryManager::~MemoryManager() noexcept {}

string_t MemoryManager::new_string(std::string_view str_view) {
    if (auto it = string_pool_.find(str_view); it != string_pool_.end()) {
        return *it;
    }
    
    size_t length = str_view.size();
    size_t hash = std::hash<std::string_view>{}(str_view);
    
    string_t new_obj = heap_.create_varsize<ObjString>(length, str_view.data(), length, hash);
    
    gc_->register_permanent(new_obj);
    
    object_allocated_++;
    string_pool_.insert(new_obj);
    return new_obj;
}

string_t MemoryManager::new_string(const char* chars, size_t length) {
    return new_string(std::string(chars, length));
}

array_t MemoryManager::new_array(const std::vector<Value>& elements) {
    // meow::allocator<Value> alloc(arena_);
    
    // return new_object<ObjArray>(elements, alloc);
    return new_object<ObjArray>(elements);
}

hash_table_t MemoryManager::new_hash(uint32_t capacity) {
    auto alloc = heap_.get_allocator<Entry>();
    return heap_.create<ObjHashTable>(alloc, capacity);
}

upvalue_t MemoryManager::new_upvalue(size_t index) {
    return new_object<ObjUpvalue>(index);
}

proto_t MemoryManager::new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk) {
    return new_object<ObjFunctionProto>(registers, upvalues, name, std::move(chunk));
}

proto_t MemoryManager::new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk, std::vector<UpvalueDesc>&& descs) {
    return new_object<ObjFunctionProto>(registers, upvalues, name, std::move(chunk), std::move(descs));
}

function_t MemoryManager::new_function(proto_t proto) {
    return new_object<ObjClosure>(proto);
}

module_t MemoryManager::new_module(string_t file_name, string_t file_path, proto_t main_proto) {
    return new_object<ObjModule>(file_name, file_path, main_proto);
}

class_t MemoryManager::new_class(string_t name) {
    return new_object<ObjClass>(name);
}

instance_t MemoryManager::new_instance(class_t klass, Shape* shape) {
    return new_object<ObjInstance>(klass, shape);
}

bound_method_t MemoryManager::new_bound_method(Value instance, Value function) {
    return new_object<ObjBoundMethod>(instance, function);
}

Shape* MemoryManager::new_shape() {
    return new_object<Shape>();
}

Shape* MemoryManager::get_empty_shape() noexcept {
    if (empty_shape_ == nullptr) {
        empty_shape_ = heap_.create<Shape>(); 
        
        if (gc_) gc_->register_permanent(empty_shape_);
        
        object_allocated_++;
    }
    return empty_shape_;
}

}
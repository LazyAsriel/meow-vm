#include <meow/memory/memory_manager.h>
#include <meow/core/objects.h>

namespace meow {

MemoryManager::MemoryManager(std::unique_ptr<GarbageCollector> gc) noexcept 
    : arena_(64 * 1024), 
      heap_(arena_),
      gc_(std::move(gc)), 
      gc_threshold_(1024), 
      object_allocated_(0) {
          
    if (gc_) {
        gc_->set_heap(&heap_);
    }
}

MemoryManager::~MemoryManager() noexcept {}

string_t MemoryManager::new_string(std::string_view str_view) noexcept {
    auto it = string_pool_.find(str_view);
    if (it != string_pool_.end()) {
        return *it;
    }
    
    std::string s(str_view);
    string_t new_obj = new_object<ObjString>(std::move(s));
    string_pool_.insert(new_obj);
    return new_obj;
}

string_t MemoryManager::new_string(const char* chars, size_t length) noexcept {
    return new_string(std::string(chars, length));
}

array_t MemoryManager::new_array(const std::vector<Value>& elements) noexcept {
    meow::allocator<Value> alloc(arena_);
    
    return new_object<ObjArray>(elements, alloc);
}

hash_table_t MemoryManager::new_hash(const std::unordered_map<string_t, Value, ObjStringHasher>& fields) noexcept {
    return new_object<ObjHashTable>(fields);
}

upvalue_t MemoryManager::new_upvalue(size_t index) noexcept {
    return new_object<ObjUpvalue>(index);
}

proto_t MemoryManager::new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk) noexcept {
    return new_object<ObjFunctionProto>(registers, upvalues, name, std::move(chunk));
}

proto_t MemoryManager::new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk, std::vector<UpvalueDesc>&& descs) noexcept {
    return new_object<ObjFunctionProto>(registers, upvalues, name, std::move(chunk), std::move(descs));
}

function_t MemoryManager::new_function(proto_t proto) noexcept {
    return new_object<ObjClosure>(proto);
}

module_t MemoryManager::new_module(string_t file_name, string_t file_path, proto_t main_proto) noexcept {
    return new_object<ObjModule>(file_name, file_path, main_proto);
}

class_t MemoryManager::new_class(string_t name) noexcept {
    return new_object<ObjClass>(name);
}

instance_t MemoryManager::new_instance(class_t klass, Shape* shape) noexcept {
    return new_object<ObjInstance>(klass, shape);
}

bound_method_t MemoryManager::new_bound_method(instance_t instance, function_t function) noexcept {
    return new_object<ObjBoundMethod>(instance, function);
}

Shape* MemoryManager::new_shape() noexcept {
    return new_object<Shape>();
}

Shape* MemoryManager::get_empty_shape() noexcept {
    if (empty_shape_ == nullptr) {
        empty_shape_ = new_shape();
    }
    return empty_shape_;
}

}
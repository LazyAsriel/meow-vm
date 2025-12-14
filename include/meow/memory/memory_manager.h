#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_set>
#include <memory>
#include <meow/core/objects.h>
#include <meow/definitions.h>
#include <meow/memory/garbage_collector.h>
#include <meow/core/shape.h>
#include <meow/core/string.h>

#include "meow_heap.h"
#include "meow_allocator.h"

namespace meow {
class MemoryManager {
public:
    explicit MemoryManager(std::unique_ptr<GarbageCollector> gc) noexcept;
    ~MemoryManager() noexcept;

    // --- Factory Methods ---
    array_t new_array(const std::vector<Value>& elements = {});
    string_t new_string(std::string_view str_view);
    string_t new_string(const char* chars, size_t length);
    hash_table_t new_hash(const std::unordered_map<string_t, Value, ObjStringHasher>& fields = {});
    upvalue_t new_upvalue(size_t index);
    proto_t new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk);
    proto_t new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk, std::vector<UpvalueDesc>&& descs);
    function_t new_function(proto_t proto);
    module_t new_module(string_t file_name, string_t file_path, proto_t main_proto = nullptr);
    class_t new_class(string_t name = nullptr);
    instance_t new_instance(class_t klass, Shape* shape);
    bound_method_t new_bound_method(Value instance, Value function);
    Shape* new_shape();

    Shape* get_empty_shape() noexcept;

    // --- GC Control ---
    void enable_gc() noexcept { 
        if (gc_pause_count_ > 0) gc_pause_count_--; 
    }
    
    void disable_gc() noexcept { 
        gc_pause_count_++; 
    }
    
    void collect() noexcept { object_allocated_ = gc_->collect(); }

    [[gnu::always_inline]]
    void write_barrier(MeowObject* owner, Value value) noexcept {
        if (gc_pause_count_ == 0) {
            gc_->write_barrier(owner, value);
        }
    }

private:
    struct StringPoolHash {
        using is_transparent = void;
        size_t operator()(const char* txt) const { return std::hash<std::string_view>{}(txt); }
        size_t operator()(std::string_view txt) const { return std::hash<std::string_view>{}(txt); }
        size_t operator()(string_t s) const { return s->hash(); }
    };

    struct StringPoolEq {
        using is_transparent = void;
        bool operator()(string_t a, string_t b) const { return a == b; }
        bool operator()(string_t a, std::string_view b) const { return std::string_view(a->c_str(), a->size()) == b; }
        bool operator()(std::string_view a, string_t b) const { return a == std::string_view(b->c_str(), b->size()); }
    };

    meow::arena arena_;
    meow::heap heap_; 

    std::unique_ptr<GarbageCollector> gc_;
    std::unordered_set<string_t, StringPoolHash, StringPoolEq> string_pool_;
    Shape* empty_shape_ = nullptr;

    size_t gc_threshold_;
    size_t object_allocated_;
    size_t gc_pause_count_ = 0;
    // bool gc_enabled_ = true;

    template <typename T, typename... Args>
    T* new_object(Args&&... args) {
        if (object_allocated_ >= gc_threshold_ && gc_pause_count_ == 0) {
            collect();
            gc_threshold_ = std::max(gc_threshold_ * 2, object_allocated_ * 2);
        }
        
        T* obj = heap_.create<T>(std::forward<Args>(args)...);
        
        gc_->register_object(static_cast<MeowObject*>(obj));
        ++object_allocated_;
        return obj;
    }
};
}
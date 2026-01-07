#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <meow/core/objects.h>
#include <meow/common.h>
#include <meow/memory/garbage_collector.h>
#include <meow/core/shape.h>
#include <meow/core/string.h>
#include <meow/core/function.h>

#include "meow_heap.h"
#include "meow_allocator.h"
#include "meow_hash_map.h"

namespace meow {
class MemoryManager {
private:
    static thread_local MemoryManager* current_;
public:
    explicit MemoryManager(std::unique_ptr<GarbageCollector> gc) noexcept;
    ~MemoryManager() noexcept;

    // --- Factory Methods ---
    array_t new_array(const std::vector<Value>& elements = {});
    string_t new_string(std::string_view str_view);
    string_t new_string(const char* chars, size_t length);
    hash_table_t new_hash(uint32_t capacity = 0);
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

    static void set_current(MemoryManager* instance) noexcept { current_ = instance; }
    static MemoryManager* get_current() noexcept { return current_; }
private:
    struct HashedView {
        std::string_view view;
        size_t hash;
    };

    struct StringPoolHash {
        using is_transparent = void;
        size_t operator()(string_t s) const noexcept { return s->hash(); }
        size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
        size_t operator()(HashedView s) const noexcept { return s.hash; }
    };

    struct StringPoolEq {
        using is_transparent = void;
        bool operator()(string_t a, string_t b) const noexcept { return a == b; }
        bool operator()(string_t a, std::string_view b) const noexcept { return std::string_view(a->c_str(), a->size()) == b; }
        bool operator()(std::string_view a, string_t b) const noexcept { return a == std::string_view(b->c_str(), b->size()); }
        bool operator()(string_t a, HashedView b) const noexcept {
            if (a->size() != b.view.size()) return false;
            if (a->hash() != b.hash) return false; 
            return std::string_view(a->c_str(), a->size()) == b.view;
        }
    };

    meow::arena arena_;
    meow::heap heap_; 

    std::unique_ptr<GarbageCollector> gc_;
    meow::hash_map<string_t, string_t, StringPoolHash, StringPoolEq> string_pool_;
    
    Shape* empty_shape_ = nullptr;

    size_t gc_threshold_;
    size_t object_allocated_;
    size_t gc_pause_count_ = 0;

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
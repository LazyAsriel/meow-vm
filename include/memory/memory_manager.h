#pragma once

#include "common/pch.h"
#include "core/objects.h"
#include "common/definitions.h"
#include "memory/garbage_collector.h"
#include "core/objects/shape.h"
#include "core/objects/string.h"

namespace meow {
class MemoryManager {
public:
    explicit MemoryManager(std::unique_ptr<GarbageCollector> gc) noexcept;
    ~MemoryManager() noexcept;
    array_t new_array(const std::vector<Value>& elements = {}) noexcept;
    string_t new_string(std::string_view str_view) noexcept;
    string_t new_string(const char* chars, size_t length) noexcept;
    hash_table_t new_hash(const std::unordered_map<string_t, Value, ObjStringHasher>& fields = {}) noexcept;
    upvalue_t new_upvalue(size_t index) noexcept;
    proto_t new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk) noexcept;
    proto_t new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk, std::vector<UpvalueDesc>&& descs) noexcept;
    function_t new_function(proto_t proto) noexcept;
    module_t new_module(string_t file_name, string_t file_path, proto_t main_proto = nullptr) noexcept;
    class_t new_class(string_t name = nullptr) noexcept;
    instance_t new_instance(class_t klass, Shape* shape) noexcept;
    
    bound_method_t new_bound_method(instance_t instance, function_t function) noexcept;

    // --- Shape Management ---
    Shape* new_shape() noexcept;

    // void deallocate_raw(void* ptr, size_t size) noexcept {
    //     ::operator delete(ptr, size);
    // }
    
    inline Shape* get_empty_shape() noexcept {
        if (!empty_shape_) [[unlikely]] {
            empty_shape_ = new_shape();
        }
        return empty_shape_;
    }

    inline void enable_gc() noexcept {
        gc_enabled_ = true;
    }
    inline void disable_gc() noexcept {
        gc_enabled_ = false;
    }
    inline void collect() noexcept {
        object_allocated_ = gc_->collect();
    }
private:
    struct StringHash {
        using is_transparent = void;
        size_t operator()(const char* txt) const { return std::hash<std::string_view>{}(txt); }
        size_t operator()(std::string_view txt) const { return std::hash<std::string_view>{}(txt); }
        size_t operator()(const std::string& txt) const { return std::hash<std::string>{}(txt); }
    };

    std::unique_ptr<GarbageCollector> gc_;
    std::unordered_map<std::string, string_t, StringHash, std::equal_to<>> string_pool_;
    
    Shape* empty_shape_ = nullptr; // [FIX] Cache empty shape

    size_t gc_threshold_;
    size_t object_allocated_;
    bool gc_enabled_ = true;

    template <typename T, typename... Args>
    T* new_object(Args&&... args) noexcept {
        if (object_allocated_ >= gc_threshold_ && gc_enabled_) {
            collect();
            gc_threshold_ *= 2;
        }
        T* new_object = new T(std::forward<Args>(args)...);
        gc_->register_object(static_cast<MeowObject*>(new_object));
        ++object_allocated_;
        return new_object;
    }
};
}
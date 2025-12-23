#pragma once
#include <cstdint>
#include <cstring>
#include <string> 
#include <meow/core/meow_object.h>

namespace meow {

class ObjString : public ObjBase<ObjectType::STRING> {
private:
    size_t length_;
    size_t hash_;
    char chars_[1]; 

    friend class MemoryManager;
    friend class heap; 
    
    ObjString(const char* chars, size_t length, size_t hash) 
        : length_(length), hash_(hash) {
        std::memcpy(chars_, chars, length);
        chars_[length] = '\0'; 
    }

public:
    ObjString() = delete; 
    ObjString(const ObjString&) = delete;
    
    // --- Accessors ---
    inline const char* c_str() const noexcept { return chars_; }
    inline size_t size() const noexcept { return length_; }
    inline bool empty() const noexcept { return length_ == 0; }
    inline size_t hash() const noexcept { return hash_; }

    inline char get(size_t index) const noexcept { return chars_[index]; }

    inline void trace(GCVisitor&) const noexcept override {}
};

struct ObjStringHasher {
    inline size_t operator()(string_t s) const noexcept {
        return s->hash();
    }
};
}
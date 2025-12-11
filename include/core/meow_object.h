#pragma once

namespace meow {
struct GCVisitor;

enum class ObjectType : uint8_t {
    ARRAY = 6,
    STRING,
    HASH_TABLE,
    INSTANCE,
    CLASS,
    BOUND_METHOD,
    UPVALUE,
    PROTO,
    FUNCTION,
    MODULE,
    SHAPE
};

struct MeowObject {
    // 1. Vtable Pointer (8 bytes - Hidden by compiler)
    
    // 2. Data Fields
    const ObjectType type; // 1 byte
    
    // [OPTIMIZATION] Nhét cờ GC vào vùng padding
    // Mặc định compiler sẽ padding thêm 7 bytes ở đây để align 8 bytes.
    // Ta lấy 1 byte dùng, không tốn thêm RAM.
    bool is_marked_ = false; 

    explicit MeowObject(ObjectType type_tag) noexcept : type(type_tag) {}
    
    virtual ~MeowObject() = default;
    virtual void trace(GCVisitor& visitor) const noexcept = 0;

    inline ObjectType get_type() const noexcept { return type; }
    
    // GC Helper
    inline bool is_marked() const noexcept { return is_marked_; }
    inline void mark() noexcept { is_marked_ = true; }
    inline void unmark() noexcept { is_marked_ = false; }
};

template <ObjectType type_tag>
struct ObjBase : public MeowObject {
    ObjBase() noexcept : MeowObject(type_tag) {}
};

}
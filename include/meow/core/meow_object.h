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
    const ObjectType type;
    bool marked = false; 

    explicit MeowObject(ObjectType type_tag) noexcept : type(type_tag) {}
    
    virtual ~MeowObject() = default;
    virtual void trace(GCVisitor& visitor) const noexcept = 0;

    inline ObjectType get_type() const noexcept { return type; }
    
    // GC Helper
    inline bool is_marked() const noexcept { return marked; }
    inline void mark() noexcept { marked = true; }
    inline void unmark() noexcept { marked = false; }
};

template <ObjectType type_tag>
struct ObjBase : public MeowObject {
    ObjBase() noexcept : MeowObject(type_tag) {}
};

}
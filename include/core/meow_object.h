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
    MODULE
};

struct MeowObject {
    const ObjectType type;

    explicit MeowObject(ObjectType type_tag) noexcept : type(type_tag) {}
    
    virtual ~MeowObject() = default;
    virtual void trace(GCVisitor& visitor) const noexcept = 0;

    inline ObjectType get_type() const noexcept { return type; }
};

template <ObjectType type_tag>
struct ObjBase : public MeowObject {
    ObjBase() noexcept : MeowObject(type_tag) {}
};

}
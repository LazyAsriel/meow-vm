#pragma once

#include <meow/common.h>
#include <cstdint>

namespace meow {
struct GCVisitor;

enum class GCState : uint8_t {
    UNMARKED = 0, MARKED = 1, OLD = 2
};

enum class ObjectType : uint8_t {
    ARRAY = base_t::index_of<object_t>() + 1,
    STRING, HASH_TABLE, INSTANCE, CLASS,
    BOUND_METHOD, UPVALUE, PROTO, FUNCTION, MODULE, SHAPE
};

struct MeowObject {
    const ObjectType type;
    GCState gc_state = GCState::UNMARKED;

    explicit MeowObject(ObjectType type_tag) noexcept : type(type_tag) {}
    virtual ~MeowObject() = default;
    
    virtual void trace(GCVisitor& visitor) const noexcept = 0;
    
    inline ObjectType get_type() const noexcept { return type; }
    inline bool is_marked() const noexcept { return gc_state != GCState::UNMARKED; }
    inline void mark() noexcept { if (gc_state == GCState::UNMARKED) gc_state = GCState::MARKED; }
    inline void unmark() noexcept { if (gc_state != GCState::OLD) gc_state = GCState::UNMARKED; }
};

template <ObjectType type_tag>
struct ObjBase : public MeowObject {
    ObjBase() noexcept : MeowObject(type_tag) {}
};
}
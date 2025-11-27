#pragma once

#include "core/meow_object.h"
#include "common/definitions.h"

namespace meow {

namespace detail {

template <typename T>
struct object_traits {
    static_assert(std::is_base_of_v<MeowObject, T>, 
                  "as_if<T> only works on types derived from MeowObject");
};

template <> struct object_traits<ObjArray> {
    static constexpr ObjectType type_tag = ObjectType::ARRAY;
};
template <> struct object_traits<ObjString> {
    static constexpr ObjectType type_tag = ObjectType::STRING;
};
template <> struct object_traits<ObjHashTable> {
    static constexpr ObjectType type_tag = ObjectType::HASH_TABLE;
};
template <> struct object_traits<ObjInstance> {
    static constexpr ObjectType type_tag = ObjectType::INSTANCE;
};
template <> struct object_traits<ObjClass> {
    static constexpr ObjectType type_tag = ObjectType::CLASS;
};
template <> struct object_traits<ObjBoundMethod> {
    static constexpr ObjectType type_tag = ObjectType::BOUND_METHOD;
};
template <> struct object_traits<ObjUpvalue> {
    static constexpr ObjectType type_tag = ObjectType::UPVALUE;
};
template <> struct object_traits<ObjFunctionProto> {
    static constexpr ObjectType type_tag = ObjectType::PROTO;
};
template <> struct object_traits<ObjClosure> {
    static constexpr ObjectType type_tag = ObjectType::FUNCTION;
};
template <> struct object_traits<ObjModule> {
    static constexpr ObjectType type_tag = ObjectType::MODULE;
};

}
}
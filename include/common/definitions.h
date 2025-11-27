#pragma once

#include "common/pch.h"

namespace meow {
}


namespace meow {
struct MeowObject;
class Machine;
class Value;
class ObjString;
class ObjArray;
class ObjHashTable;
class ObjClass;
class ObjInstance;
class ObjBoundMethod;
class ObjUpvalue;
class ObjFunctionProto;
class ObjNativeFunction;
class ObjClosure;
class ObjModule;


using value_t = Value;
using param_t = value_t;
using return_t = value_t;
using mutable_t = value_t&;
using arguments_t = std::vector<value_t>&;
    
using null_t = std::monostate;
using bool_t = bool;
using int_t = int64_t;
using float_t = double;
using native_t = value_t (*)(Machine* engine, int argc, value_t* argv);
using object_t = MeowObject*;

using array_t = ObjArray*;
using string_t = ObjString*;
using hash_table_t = ObjHashTable*;
using instance_t = ObjInstance*;
using class_t = ObjClass*;
using bound_method_t = ObjBoundMethod*;
using upvalue_t = ObjUpvalue*;
using proto_t = ObjFunctionProto*;
using function_t = ObjClosure*;
using module_t = ObjModule*;

enum class ValueType : uint8_t {
    Null,
    Bool,
    Int,
    Float,
    NativeFn,
    Object,

    Array,        // 1  — ARRAY
    String,       // 2  — STRING
    HashTable,    // 3  — HASH_TABLE
    Instance,     // 4  — INSTANCE
    Class,        // 5  — CLASS
    BoundMethod,  // 6  — BOUND_METHOD
    Upvalue,      // 7  — UPVALUE
    Proto,        // 8  — PROTO
    Function,     // 9  — FUNCTION
    Module,       // 10 — MODULE

    TotalValueTypes
};
}
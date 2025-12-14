// src/vm/stdlib/string_lib.cpp
#include "pch.h"
#include "vm/stdlib/stdlib.h"
#include <meow/machine.h>
#include <meow/memory/memory_manager.h>
#include <meow/core/module.h>
#include <algorithm> // transform

namespace meow::natives::str {

#define CHECK_SELF() \
    if (argc < 1 || !argv[0].is_string()) { \
        vm->error("String method expects 'this' to be a String."); \
        return Value(null_t{}); \
    } \
    string_t self = argv[0].as_string();

static Value len(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    return Value((int64_t)self->size());
}

static Value upper(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    std::string s(self->c_str(), self->size());
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return Value(vm->get_heap()->new_string(s));
}

static Value lower(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    std::string s(self->c_str(), self->size());
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return Value(vm->get_heap()->new_string(s));
}

static Value trim(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    std::string_view sv(self->c_str(), self->size());
    // Trim left
    while (!sv.empty() && std::isspace(sv.front())) sv.remove_prefix(1);
    // Trim right
    while (!sv.empty() && std::isspace(sv.back())) sv.remove_suffix(1);
    
    return Value(vm->get_heap()->new_string(sv));
}

static Value contains(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_string()) return Value(false);
    
    std::string_view haystack(self->c_str(), self->size());
    string_t needle_obj = argv[1].as_string();
    std::string_view needle(needle_obj->c_str(), needle_obj->size());
    
    return Value(haystack.find(needle) != std::string::npos);
}

static Value join(Machine* vm, int argc, Value* argv) {
    if (argc < 2 || !argv[0].is_string() || !argv[1].is_array()) {
        vm->error("Usage: string.join(separator, array_of_strings)");
        return Value(null_t{});
    }

    string_t sep = argv[0].as_string();
    array_t arr = argv[1].as_array();
    
    std::string res = "";
    for (size_t i = 0; i < arr->size(); ++i) {
        if (i > 0) res += sep->c_str();
        
        Value item = arr->get(i);
        if (item.is_string()) {
            res += item.as_string()->c_str();
        } else {}
    }
    return Value(vm->get_heap()->new_string(res));
}

} // namespace

namespace meow::stdlib {
module_t create_string_module(Machine* vm, MemoryManager* heap) noexcept {
    auto name = heap->new_string("string");
    auto mod = heap->new_module(name, name);
    auto reg = [&](const char* n, native_t fn) { mod->set_export(heap->new_string(n), Value(fn)); };

    using namespace meow::natives::str;
    reg("len", len);
    reg("upper", upper);
    reg("lower", lower);
    reg("trim", trim);
    reg("contains", contains);
    reg("join", join);
    
    return mod;
}
}
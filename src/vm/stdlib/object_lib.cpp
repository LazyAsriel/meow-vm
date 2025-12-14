#include "pch.h"
#include "vm/stdlib/stdlib.h"
#include <meow/machine.h>
#include <meow/memory/memory_manager.h>
#include <meow/core/module.h>
#include <meow/core/hash_table.h>

namespace meow::natives::obj {

#define CHECK_SELF() \
    if (argc < 1 || !argv[0].is_hash_table()) { \
        vm->error("Object method expects 'this' to be a Hash Table."); \
        return Value(null_t{}); \
    } \
    hash_table_t self = argv[0].as_hash_table();

static Value keys(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    auto arr = vm->get_heap()->new_array();
    arr->reserve(self->size());
    for(auto it = self->begin(); it != self->end(); ++it) {
        arr->push(Value(it->first));
    }
    return Value(arr);
}

static Value has(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_string()) return Value(false);
    return Value(self->has(argv[1].as_string()));
}

static Value len(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    return Value((int64_t)self->size());
}

} // namespace

namespace meow::stdlib {
module_t create_object_module(Machine* vm, MemoryManager* heap) noexcept {
    auto name = heap->new_string("object");
    auto mod = heap->new_module(name, name);
    auto reg = [&](const char* n, native_t fn) { mod->set_export(heap->new_string(n), Value(fn)); };

    using namespace meow::natives::obj;
    reg("keys", keys);
    reg("has", has);
    reg("len", len);
    
    return mod;
}
}
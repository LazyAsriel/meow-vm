#include "pch.h"
#include "vm/stdlib/stdlib.h"
#include <meow/machine.h>
#include <meow/value.h>
#include <meow/memory/memory_manager.h>
#include <meow/core/module.h>
#include <meow/core/array.h> //

namespace meow::natives::array {

#define CHECK_SELF() \
    if (argc < 1 || !argv[0].is_array()) { \
        vm->error("Array method expects 'this' to be an Array."); \
        return Value(null_t{}); \
    } \
    array_t self = argv[0].as_array();

static Value push(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    // Đẩy tất cả các tham số còn lại vào mảng
    for (int i = 1; i < argc; ++i) {
        self->push(argv[i]);
    }
    return Value((int64_t)self->size());
}

static Value pop(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (self->empty()) return Value(null_t{}); // Pop mảng rỗng ra null
    
    Value val = self->back();
    self->pop();
    return val;
}

static Value clear(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    self->clear();
    return Value(null_t{});
}

static Value length(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    return Value((int64_t)self->size());
}

static Value resize(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_int()) {
        vm->error("resize expects an integer size.");
        return Value(null_t{});
    }
    size_t new_size = static_cast<size_t>(argv[1].as_int());
    self->resize(new_size); // Mặc định fill null
    return Value(null_t{});
}

} // namespace

namespace meow::stdlib {
module_t create_array_module(Machine* vm, MemoryManager* heap) noexcept {
    auto name = heap->new_string("array");
    auto mod = heap->new_module(name, name);
    auto reg = [&](const char* n, native_t fn) { mod->set_export(heap->new_string(n), Value(fn)); };

    using namespace meow::natives::array;
    reg("push", push);
    reg("pop", pop);
    reg("clear", clear);
    reg("len", length);
    reg("resize", resize);
    
    return mod;
}
}
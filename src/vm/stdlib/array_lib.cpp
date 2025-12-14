#include "pch.h"
#include "vm/stdlib/stdlib.h"
#include <meow/machine.h>
#include <meow/value.h>
#include <meow/memory/memory_manager.h>
#include <meow/core/module.h>
#include <meow/core/array.h> 
#include <meow/cast.h>
#include <format> 

namespace meow::natives::array {

// Định nghĩa giới hạn cứng (64 triệu phần tử) để tránh crash
constexpr size_t MAX_ARRAY_CAPACITY = 64 * 1024 * 1024; 

#define CHECK_SELF() \
    if (argc < 1 || !argv[0].is_array()) { \
        vm->error("Array method expects 'this' to be an Array."); \
        return Value(null_t{}); \
    } \
    array_t self = argv[0].as_array();

static Value push(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (self->size() + (argc - 1) >= MAX_ARRAY_CAPACITY) {
        vm->error("Array size exceeded limit during push.");
        return Value(null_t{});
    }

    for (int i = 1; i < argc; ++i) {
        self->push(argv[i]);
    }
    return Value((int64_t)self->size());
}

static Value pop(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (self->empty()) return Value(null_t{}); 
    
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
    std::println("[DEBUG] {}.size() is called", meow::to_string(argv[0]));
    CHECK_SELF();
    return Value((int64_t)self->size());
}

static Value resize(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_int()) {
        vm->error("resize expects an integer size.");
        return Value(null_t{});
    }

    int64_t input_size = argv[1].as_int();

    if (input_size < 0) {
        vm->error("New size cannot be negative.");
        return Value(null_t{});
    }

    if (static_cast<size_t>(input_size) > MAX_ARRAY_CAPACITY) {
        vm->error(std::format("New size too large ({}). Max allowed: {}", input_size, MAX_ARRAY_CAPACITY));
        return Value(null_t{});
    }

    try {
        self->resize(static_cast<size_t>(input_size)); 
    } catch (const std::exception& e) {
        vm->error("Out of memory during array resize.");
    }

    return Value(null_t{});
}

// [MỚI] Hàm cắt mảng (slice)
static Value slice(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    
    int64_t len = static_cast<int64_t>(self->size());
    int64_t start = 0;
    int64_t end = len;

    // Tham số thứ 2: start index (có thể âm)
    if (argc >= 2 && argv[1].is_int()) {
        start = argv[1].as_int();
        if (start < 0) start += len;
        if (start < 0) start = 0;
        if (start > len) start = len;
    }

    // Tham số thứ 3: end index (có thể âm)
    if (argc >= 3 && argv[2].is_int()) {
        end = argv[2].as_int();
        if (end < 0) end += len;
        if (end < 0) end = 0;
        if (end > len) end = len;
    }

    // Nếu start >= end -> mảng rỗng
    if (start >= end) {
        return Value(vm->get_heap()->new_array());
    }

    auto new_arr = vm->get_heap()->new_array();
    new_arr->reserve(static_cast<size_t>(end - start));
    
    for (int64_t i = start; i < end; ++i) {
        new_arr->push(self->get(static_cast<size_t>(i)));
    }
    
    return Value(new_arr);
}

} // namespace meow::natives::array

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
    reg("size", length); 
    reg("resize", resize);
    reg("slice", slice); // [Đăng ký]
    reg("length", length);
    return mod;
}
}
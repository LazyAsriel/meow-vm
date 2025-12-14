#include "pch.h"
#include "vm/stdlib/stdlib.h"
#include <meow/machine.h>
#include <meow/value.h>
#include <meow/memory/memory_manager.h>
#include <meow/core/module.h>
#include <meow/core/array.h> 
#include <format> // Thêm thư viện này để in lỗi đẹp hơn

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
    // Nên check xem push có làm nổ RAM không, nhưng thường vector tự handle bad_alloc
    // Nếu muốn an toàn tuyệt đối:
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
    CHECK_SELF();
    return Value((int64_t)self->size());
}

// --- HÀM GÂY LỖI CẦN SỬA ---
static Value resize(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_int()) {
        vm->error("resize expects an integer size.");
        return Value(null_t{});
    }

    int64_t input_size = argv[1].as_int();

    // 1. Chặn số âm (sẽ thành số siêu lớn khi cast sang size_t)
    if (input_size < 0) {
        vm->error("New size cannot be negative.");
        return Value(null_t{});
    }

    // 2. Chặn số quá lớn (nguyên nhân gây std::length_error)
    if (static_cast<size_t>(input_size) > MAX_ARRAY_CAPACITY) {
        vm->error(std::format("New size too large ({}). Max allowed: {}", input_size, MAX_ARRAY_CAPACITY));
        return Value(null_t{});
    }

    // 3. Thực hiện resize an toàn
    // try-catch ở đây để bắt bad_alloc nếu hết RAM thật
    try {
        self->resize(static_cast<size_t>(input_size)); 
    } catch (const std::exception& e) {
        vm->error("Out of memory during array resize.");
    }

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
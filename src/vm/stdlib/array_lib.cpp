#include "pch.h"
#include "vm/stdlib/stdlib.h"
#include <meow/machine.h>
#include <meow/value.h>
#include <meow/memory/memory_manager.h>
#include <meow/core/module.h>
#include <meow/core/array.h> 
#include <meow/cast.h>
#include <format> 
#include <algorithm>

namespace meow::natives::array {

constexpr size_t MAX_ARRAY_CAPACITY = 64 * 1024 * 1024; 

#define CHECK_SELF() \
    if (argc < 1 || !argv[0].is_array()) { \
        vm->error("Array method expects 'this' to be an Array."); \
        return Value(null_t{}); \
    } \
    array_t self = argv[0].as_array();

static Value push(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (self->size() + (argc - 1) >= MAX_ARRAY_CAPACITY) [[unlikely]] {
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

static Value reserve(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_int()) return Value(null_t{});
    int64_t cap = argv[1].as_int();
    
    if (cap > 0 && static_cast<size_t>(cap) < MAX_ARRAY_CAPACITY) {
        self->reserve(static_cast<size_t>(cap));
    }
    return Value(null_t{});
}

static Value resize(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2 || !argv[1].is_int()) {
        vm->error("resize expects an integer size.");
        return Value(null_t{});
    }

    int64_t input_size = argv[1].as_int();
    Value fill_val = (argc > 2) ? argv[2] : Value(null_t{});

    if (input_size < 0) {
        vm->error("New size cannot be negative.");
        return Value(null_t{});
    }

    if (static_cast<size_t>(input_size) > MAX_ARRAY_CAPACITY) {
        vm->error(std::format("New size too large ({}). Max allowed: {}", input_size, MAX_ARRAY_CAPACITY));
        return Value(null_t{});
    }
    
    size_t old_size = self->size();
    size_t new_size = static_cast<size_t>(input_size);
    self->resize(new_size);
    
    if (new_size > old_size && !fill_val.is_null()) {
        for(size_t i = old_size; i < new_size; ++i) {
            self->set(i, fill_val);
        }
    }

    return Value(null_t{});
}

static Value slice(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    
    int64_t len = static_cast<int64_t>(self->size());
    int64_t start = 0;
    int64_t end = len;

    if (argc >= 2 && argv[1].is_int()) {
        start = argv[1].as_int();
        if (start < 0) start += len;
        if (start < 0) start = 0;
        if (start > len) start = len;
    }

    if (argc >= 3 && argv[2].is_int()) {
        end = argv[2].as_int();
        if (end < 0) end += len;
        if (end < 0) end = 0;
        if (end > len) end = len;
    }

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

static Value reverse(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    std::reverse(self->begin(), self->end());
    return argv[0];
}

static Value forEach(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2) return Value(null_t{});
    Value callback = argv[1];

    for (size_t i = 0; i < self->size(); ++i) {
        std::vector<Value> args = { self->get(i), Value((int64_t)i) };
        vm->call_callable(callback, args);
        if (vm->has_error()) return Value(null_t{});
    }
    return Value(null_t{});
}

static Value map(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2) return Value(null_t{});
    Value callback = argv[1];

    auto result_arr = vm->get_heap()->new_array();
    result_arr->reserve(self->size());

    for (size_t i = 0; i < self->size(); ++i) {
        std::vector<Value> args = { self->get(i), Value((int64_t)i) };
        Value res = vm->call_callable(callback, args);
        
        if (vm->has_error()) return Value(null_t{});
        result_arr->push(res);
    }
    return Value(result_arr);
}

static Value filter(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2) return Value(null_t{});
    Value callback = argv[1];

    auto result_arr = vm->get_heap()->new_array();

    for (size_t i = 0; i < self->size(); ++i) {
        Value val = self->get(i);
        std::vector<Value> args = { val, Value((int64_t)i) };
        Value condition = vm->call_callable(callback, args);
        if (vm->has_error()) return Value(null_t{});
        
        if (to_bool(condition)) {
            result_arr->push(val);
        }
    }
    return Value(result_arr);
}

static Value reduce(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2) return Value(null_t{});
    Value callback = argv[1];
    Value accumulator = (argc > 2) ? argv[2] : Value(null_t{});
    
    size_t start_index = 0;

    if (argc < 3) {
        if (self->empty()) {
            vm->error("Reduce on empty array with no initial value.");
            return Value(null_t{});
        }
        accumulator = self->get(0);
        start_index = 1;
    }

    for (size_t i = start_index; i < self->size(); ++i) {
        std::vector<Value> args = { accumulator, self->get(i), Value((int64_t)i) };
        accumulator = vm->call_callable(callback, args);
        if (vm->has_error()) return Value(null_t{});
    }
    return accumulator;
}

static Value find(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2) return Value(null_t{});
    Value callback = argv[1];

    for (size_t i = 0; i < self->size(); ++i) {
        Value val = self->get(i);
        std::vector<Value> args = { val, Value((int64_t)i) };
        Value res = vm->call_callable(callback, args);
        if (vm->has_error()) return Value(null_t{});
        
        if (to_bool(res)) return val;
    }
    return Value(null_t{});
}

static Value findIndex(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    if (argc < 2) return Value((int64_t)-1);
    Value callback = argv[1];

    for (size_t i = 0; i < self->size(); ++i) {
        Value val = self->get(i);
        std::vector<Value> args = { val, Value((int64_t)i) };
        Value res = vm->call_callable(callback, args);
        if (vm->has_error()) return Value((int64_t)-1);
        
        if (to_bool(res)) return Value((int64_t)i);
    }
    return Value((int64_t)-1);
}

static Value sort(Machine* vm, int argc, Value* argv) {
    CHECK_SELF();
    
    size_t n = self->size();
    if (n < 2) return argv[0];

    bool has_comparator = (argc > 1);
    Value comparator = has_comparator ? argv[1] : Value(null_t{});

    for (size_t i = 0; i < n - 1; i++) {
        for (size_t j = 0; j < n - i - 1; j++) {
            Value a = self->get(j);
            Value b = self->get(j + 1);
            bool swap = false;

            if (has_comparator) {
                std::vector<Value> args = { a, b };
                Value res = vm->call_callable(comparator, args);
                if (vm->has_error()) return Value(null_t{});
                if (res.is_int() && res.as_int() > 0) swap = true; 
                else if (res.is_float() && res.as_float() > 0) swap = true;
            } else {
                if (a.is_int() && b.is_int()) {
                    if (a.as_int() > b.as_int()) swap = true;
                } else if (a.is_float() || b.is_float()) {
                    if (to_float(a) > to_float(b)) swap = true;
                } else {
                    if (std::string_view(to_string(a)) > std::string_view(to_string(b))) swap = true;
                }
            }

            if (swap) {
                self->set(j, b);
                self->set(j + 1, a);
            }
        }
    }
    return argv[0];
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
    reg("length", length);
    reg("resize", resize);
    reg("reserve", reserve);
    reg("slice", slice); 
    
    reg("map", map);
    reg("filter", filter);
    reg("reduce", reduce);
    reg("forEach", forEach);
    reg("find", find);
    reg("findIndex", findIndex);
    reg("reverse", reverse);
    reg("sort", sort);

    return mod;
}
}
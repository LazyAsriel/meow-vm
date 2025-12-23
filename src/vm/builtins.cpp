#include "pch.h"
#include <meow/machine.h>
#include <meow/core/objects.h>
#include <meow/value.h>
#include <meow/memory/memory_manager.h>
#include "module/module_manager.h"
#include <meow/cast.h>
#include "vm/stdlib/stdlib.h"

namespace meow {

#define CHECK_ARGS(n) \
    if (argc < n) [[unlikely]] vm->error("Native function expects at least " #n " arguments.");

namespace natives {

// print(val1, val2, ...)
static Value print([[maybe_unused]] Machine* vm, int argc, Value* argv) {
    for (int i = 0; i < argc; ++i) {
        if (i > 0) std::cout << " ";
        std::print("{}", to_string(argv[i]));
    }
    std::println("");
    return Value(null_t{});
}

// typeof(value)
static Value type_of(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    Value v = argv[0];
    std::string type_str = "unknown";

    if (v.is_null()) type_str = "null";
    else if (v.is_bool()) type_str = "bool";
    else if (v.is_int()) type_str = "int";
    else if (v.is_float()) type_str = "real";
    else if (v.is_string()) type_str = "string";
    else if (v.is_array()) type_str = "array";
    else if (v.is_hash_table()) type_str = "object";
    else if (v.is_function() || v.is_native() || v.is_bound_method()) type_str = "function";
    else if (v.is_class()) type_str = "class";
    else if (v.is_instance()) type_str = "instance";
    else if (v.is_module()) type_str = "module";

    return Value(vm->get_heap()->new_string(type_str));
}

// len(container)
static Value len(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    Value v = argv[0];
    int64_t length = -1;

    if (v.is_string()) length = v.as_string()->size();
    else if (v.is_array()) length = v.as_array()->size();
    else if (v.is_hash_table()) length = v.as_hash_table()->size();
    
    return Value(length);
}

// assert(condition, message?)
static Value assert_fn(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    if (!to_bool(argv[0])) {
        std::string msg = "Assertion failed.";
        if (argc > 1 && argv[1].is_string()) {
            msg = argv[1].as_string()->c_str();
        }
        vm->error(msg);
    }
    return Value(null_t{});
}

// int(value)
static Value to_int_fn(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    return Value(to_int(argv[0]));
}

// real(value)
static Value to_real_fn(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    return Value(to_float(argv[0]));
}

// bool(value)
static Value to_bool_fn(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    return Value(to_bool(argv[0]));
}

// str(value)
static Value to_str_fn(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    return Value(vm->get_heap()->new_string(to_string(argv[0])));
}

// ord(char_string)
static Value ord(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    if (!argv[0].is_string()) vm->error("ord() expects a string.");
    string_t s = argv[0].as_string();
    if (s->size() != 1) vm->error("ord() expects a single character.");
    return Value(static_cast<int64_t>(static_cast<unsigned char>(s->get(0))));
}

// char(code)
static Value chr(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    if (!argv[0].is_int()) vm->error("char() expects an integer.");
    int64_t code = argv[0].as_int();
    if (code < 0 || code > 255) vm->error("char() code out of range [0-255].");
    char c = static_cast<char>(code);
    return Value(vm->get_heap()->new_string(std::string(1, c)));
}

// range(stop) or range(start, stop) or range(start, stop, step)
static Value range(Machine* vm, int argc, Value* argv) {
    CHECK_ARGS(1);
    int64_t start = 0;
    int64_t stop = 0;
    int64_t step = 1;

    if (argc == 1) {
        stop = to_int(argv[0]);
    } else if (argc == 2) {
        start = to_int(argv[0]);
        stop = to_int(argv[1]);
    } else {
        start = to_int(argv[0]);
        stop = to_int(argv[1]);
        step = to_int(argv[2]);
    }

    if (step == 0) vm->error("range() step cannot be 0.");

    auto arr = vm->get_heap()->new_array();
    
    if (step > 0) {
        for (int64_t i = start; i < stop; i += step) {
            arr->push(Value(i));
        }
    } else {
        for (int64_t i = start; i > stop; i += step) {
            arr->push(Value(i));
        }
    }

    return Value(arr);
}

// clock()
static Value clock_fn([[maybe_unused]] Machine* vm, int argc, Value* argv) {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    double millis = std::chrono::duration<double, std::milli>(duration).count();
    return Value(millis);
}

} // namespace natives

void Machine::load_builtins() {
    auto name_native = heap_->new_string("native");
    auto mod = heap_->new_module(name_native, name_native);

    auto reg = [&](const char* name, native_t fn) {
        mod->set_global(heap_->new_string(name), Value(fn));
    };

    // Đăng ký danh sách hàm
    reg("print", natives::print);
    reg("typeof", natives::type_of);
    reg("len", natives::len);
    reg("assert", natives::assert_fn);
    reg("int", natives::to_int_fn);
    reg("real", natives::to_real_fn);
    reg("bool", natives::to_bool_fn);
    reg("str", natives::to_str_fn);
    reg("ord", natives::ord);
    reg("char", natives::chr);
    reg("range", natives::range);
    reg("clock", natives::clock_fn);

    mod_manager_->add_cache(name_native, mod);
    mod_manager_->add_cache(heap_->new_string("io"), stdlib::create_io_module(this, heap_.get()));
    mod_manager_->add_cache(heap_->new_string("system"), stdlib::create_system_module(this, heap_.get()));
    mod_manager_->add_cache(heap_->new_string("array"), stdlib::create_array_module(this, heap_.get()));
    mod_manager_->add_cache(heap_->new_string("string"), stdlib::create_string_module(this, heap_.get()));
    mod_manager_->add_cache(heap_->new_string("object"), stdlib::create_object_module(this, heap_.get()));
    mod_manager_->add_cache(heap_->new_string("json"), stdlib::create_json_module(this, heap_.get()));
    mod_manager_->add_cache(heap_->new_string("memory"), stdlib::create_memory_module(this, heap_.get()));

}

} // namespace meow
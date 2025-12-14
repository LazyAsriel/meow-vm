#include "pch.h"
#include "vm/stdlib/stdlib.h"
#include <meow/machine.h>
#include <meow/value.h>
#include <meow/memory/memory_manager.h>
#include <meow/cast.h>
#include <meow/core/module.h>

namespace meow::natives::sys {

// system.argv()
static Value get_argv(Machine* vm, int, Value*) {
    // [Cite: 1] Accessing VM arguments
    const auto& cmd_args = vm->get_args().command_line_arguments_; 
    auto arr = vm->get_heap()->new_array();
    // Pre-allocate to avoid reallocations
    arr->reserve(cmd_args.size());
    
    for (const auto& arg : cmd_args) {
        arr->push(Value(vm->get_heap()->new_string(arg)));
    }
    return Value(arr);
}

// system.exit(code)
static Value exit_vm(Machine*, int argc, Value* argv) {
    int code = 0;
    if (argc > 0) code = static_cast<int>(to_int(argv[0]));
    std::exit(code);
    std::unreachable();
}

// system.exec(command)
static Value exec_cmd(Machine* vm, int argc, Value* argv) {
    if (argc < 1) [[unlikely]] return Value(static_cast<int64_t>(-1));
    const char* cmd = argv[0].as_string()->c_str();
    int code = std::system(cmd);
    return Value(static_cast<int64_t>(code));
}

// system.time() -> ms
static Value time_now(Machine*, int, Value*) {
    // Use steady_clock for measuring duration, system_clock for wall time.
    // Usually 'time()' implies wall clock.
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return Value(static_cast<int64_t>(ms));
}

// system.env(name)
static Value get_env(Machine* vm, int argc, Value* argv) {
    if (argc < 1) [[unlikely]] return Value(null_t{});
    const char* val = std::getenv(argv[0].as_string()->c_str());
    if (val) return Value(vm->get_heap()->new_string(val));
    return Value(null_t{});
}

} // namespace meow::natives::sys

namespace meow::stdlib {

module_t create_system_module(Machine* vm, MemoryManager* heap) noexcept {
    auto name = heap->new_string("system");
    auto mod = heap->new_module(name, name);

    auto reg = [&](const char* n, native_t fn) {
        mod->set_export(heap->new_string(n), Value(fn));
    };

    using namespace meow::natives::sys;
    reg("argv", get_argv);
    reg("exit", exit_vm);
    reg("exec", exec_cmd);
    reg("time", time_now);
    reg("env", get_env);

    return mod;
}

} // namespace meow::stdlib
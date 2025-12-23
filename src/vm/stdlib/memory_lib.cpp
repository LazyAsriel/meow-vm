#include "pch.h"
#include "vm/stdlib/stdlib.h"
#include <cstdlib>
#include <meow/machine.h>
#include <meow/value.h>
#include <meow/memory/memory_manager.h>

namespace meow::stdlib {

// malloc(size: int) -> pointer
static Value malloc(Machine* vm, int argc, Value* argv) {
    if (argc < 1 || !argv[0].is_int()) [[unlikely]] {
        return Value(); 
    }

    size_t size = static_cast<size_t>(argv[0].as_int());
    
    if (size == 0) return Value();

    void* buffer = std::malloc(size);
    return Value(buffer);
} 

// free(ptr: pointer) -> null
static Value free(Machine* vm, int argc, Value* argv) {
    if (argc >= 1 && argv[0].is_pointer()) [[likely]] {
        void* ptr = argv[0].as_pointer();
        if (ptr) std::free(ptr);
    }
    return Value();
}
    
module_t create_memory_module(Machine* vm, MemoryManager* heap) noexcept {
    auto name = heap->new_string("memory");
    auto mod = heap->new_module(name, name);
    
    auto reg = [&](const char* n, native_t fn) { 
        mod->set_export(heap->new_string(n), Value(fn)); 
    };

    reg("malloc", malloc);
    reg("free", free);

    return mod;
}

}
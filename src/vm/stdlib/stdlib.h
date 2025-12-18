/**
 * @file stdlib.h
 * @brief Factory definitions for Standard Libraries
 * @note  Zero-overhead abstractions.
 */
#pragma once

#include <meow/common.h>

namespace meow {
    class Machine;
    class MemoryManager;
}

namespace meow::stdlib {
    // Factory functions - Return raw Module Object pointer
    [[nodiscard]] module_t create_io_module(Machine* vm, MemoryManager* heap) noexcept;
    [[nodiscard]] module_t create_system_module(Machine* vm, MemoryManager* heap) noexcept;
    [[nodiscard]] module_t create_array_module(Machine* vm, MemoryManager* heap) noexcept;
    [[nodiscard]] module_t create_string_module(Machine* vm, MemoryManager* heap) noexcept;
    [[nodiscard]] module_t create_object_module(Machine* vm, MemoryManager* heap) noexcept;
    [[nodiscard]] module_t create_json_module(Machine* vm, MemoryManager* heap) noexcept;
}

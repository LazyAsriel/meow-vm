#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "runtime/execution_context.h"
#include "runtime/call_frame.h"
#include "bytecode/chunk.h"
#include "core/objects/function.h"

namespace meow {
struct ExecutionContext;
class MemoryManager;
class ModuleManager;
class Machine;
}

namespace meow {
struct VMState {
    Machine& machine;
    ExecutionContext& ctx;
    MemoryManager& heap;
    ModuleManager& modules;

    // --- CACHED POINTERS ---
    Value* registers;       
    Value* constants;       
    const uint8_t* instruction_base;
    module_t current_module = nullptr;

    std::string_view error_msg;
    bool has_error_ = false;

    void error(std::string_view msg) noexcept {
        error_msg = msg;
        has_error_ = true;
    }
    bool has_error() const noexcept { return has_error_; }
    void clear_error() noexcept { has_error_ = false; }
    std::string_view get_error_message() const noexcept { return error_msg; }

    // --- Fast Accessors ---
    [[gnu::always_inline]]
    inline void update_pointers() noexcept {
        registers = ctx.current_regs_;
        
        auto proto = ctx.frame_ptr_->function_->get_proto();
        
        constants = proto->get_chunk().get_constants_raw();
        instruction_base = proto->get_chunk().get_code();
        
        current_module = proto->get_module();
    }

    [[gnu::always_inline]] 
    inline Value& reg(uint16_t idx) noexcept {
        return registers[idx];
    }
};
}
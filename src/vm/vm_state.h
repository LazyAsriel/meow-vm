#pragma once

#include "pch.h"
#include <meow/common.h>
#include "runtime/execution_context.h"
#include "runtime/call_frame.h"
#include <meow/bytecode/chunk.h>
#include <meow/core/function.h>
#include <meow/bytecode/disassemble.h>

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

    Value* registers;       
    const Value* constants;       
    const uint8_t* instruction_base;
    module_t current_module = nullptr;

    std::string error_msg;
    bool has_error_ = false;

    void error(std::string_view msg, const uint8_t* current_ip = nullptr) noexcept {
        std::cerr << "Runtime Error: " << msg << "\n";
        error_msg = msg;
        has_error_ = true;
        
        if (ctx.current_frame_ && ctx.current_frame_->function_) {
            const auto& chunk = ctx.current_frame_->function_->get_proto()->get_chunk();
            
            const uint8_t* real_ip = current_ip ? current_ip : ctx.current_frame_->ip_;

            if (real_ip >= chunk.get_code() && real_ip < chunk.get_code() + chunk.get_code_size()) {
                size_t offset = real_ip - chunk.get_code();
                std::cerr << disassemble_around(chunk, offset, 3);
            } else {
                std::cerr << "[Debug] IP lệch pha (Out of bounds) - Không thể in code trace.\n";
            }
        }
    }

    bool has_error() const noexcept { return has_error_; }
    void clear_error() noexcept { has_error_ = false; error_msg.clear(); }
    std::string_view get_error_message() const noexcept { return error_msg; }

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
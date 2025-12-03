#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "runtime/execution_context.h"

namespace meow {
struct ExecutionContext;
class MemoryManager;
class ModuleManager;

struct VMState {
    ExecutionContext& ctx;
    MemoryManager& heap;
    ModuleManager& modules;

    std::string_view error_msg;
    bool has_error_ = false;

    void error(std::string_view msg) noexcept {
        error_msg = msg;
        has_error_ = true;
    }
    
    bool has_error() const noexcept { return has_error_; }
    void clear_error() noexcept { has_error_ = false; }
    std::string_view get_error_message() const noexcept { return error_msg; }

    [[gnu::always_inline]] mutable_t reg(uint16_t idx) noexcept {
        return ctx.registers_[ctx.current_base_ + idx];
    }

    [[gnu::always_inline]] return_t constant(uint16_t idx) const noexcept {
        return ctx.current_frame_->function_->get_proto()->get_chunk().get_constant(idx);
    }
};
}
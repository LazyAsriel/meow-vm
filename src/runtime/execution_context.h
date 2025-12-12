#pragma once

#include "pch.h"
#include <meow/core/objects.h>
#include <meow/value.h>
#include "runtime/call_frame.h"
#include "runtime/exception_handler.h"

namespace meow {

struct ExecutionContext {
    // --- CẤU HÌNH TĨNH ---
    static constexpr size_t STACK_SIZE = 65536; 
    static constexpr size_t FRAMES_MAX = 2048;

    // --- BỘ NHỚ VẬT LÝ ---
    Value stack_[STACK_SIZE]; 
    CallFrame call_stack_[FRAMES_MAX];

    // --- CON TRỎ QUẢN LÝ ---
    Value* stack_top_ = nullptr; 
    CallFrame* frame_ptr_ = nullptr;
    Value* current_regs_ = nullptr;

    // --- PHỤ TRỢ ---
    std::vector<upvalue_t> open_upvalues_;
    std::vector<ExceptionHandler> exception_handlers_;
    CallFrame* current_frame_ = nullptr;

    ExecutionContext() {
        reset();
    }

    inline void reset() noexcept {
        stack_top_ = stack_;
        current_regs_ = stack_;
        frame_ptr_ = call_stack_;
        current_frame_ = frame_ptr_;
        open_upvalues_.clear();
        exception_handlers_.clear();
    }

    [[gnu::always_inline]]
    inline bool check_overflow(size_t needed_slots) const noexcept {
        return (stack_top_ + needed_slots) <= (stack_ + STACK_SIZE);
    }
    
    [[gnu::always_inline]]
    inline bool check_frame_overflow() const noexcept {
        return (frame_ptr_ + 1) < (call_stack_ + FRAMES_MAX);
    }

    inline void trace(GCVisitor& visitor) const noexcept {
        for (const Value* slot = stack_; slot < stack_top_; ++slot) {
            visitor.visit_value(*slot);
        }
        for (const auto& upvalue : open_upvalues_) {
            visitor.visit_object(upvalue);
        }
    }
};
}
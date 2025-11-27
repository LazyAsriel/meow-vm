#pragma once

#include "common/pch.h"
#include "core/objects.h"
#include "core/objects/function.h"
#include "core/value.h"
#include "memory/gc_visitor.h"
#include "runtime/call_frame.h"
#include "runtime/exception_handler.h"

namespace meow {

struct ExecutionContext {
    std::vector<CallFrame> call_stack_;
    std::vector<Value> registers_;
    std::vector<upvalue_t> open_upvalues_;
    std::vector<ExceptionHandler> exception_handlers_;

    size_t current_base_ = 0;
    CallFrame* current_frame_ = nullptr;

    inline void reset() noexcept {
        call_stack_.clear();
        registers_.clear();
        open_upvalues_.clear();
        exception_handlers_.clear();
    }

    inline void trace(GCVisitor& visitor) const noexcept {
        for (const auto& reg : registers_) {
            visitor.visit_value(reg);
        }
        for (const auto& upvalue : open_upvalues_) {
            visitor.visit_object(upvalue);
        }
    }
};
}
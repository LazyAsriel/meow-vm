#pragma once

#include "pch.h"
#include "runtime/execution_context.h"
#include <meow/memory/memory_manager.h>
#include <meow/common.h>

namespace meow {
inline upvalue_t capture_upvalue(ExecutionContext* context, MemoryManager* heap, size_t register_index) noexcept {
    for (auto it = context->open_upvalues_.rbegin(); it != context->open_upvalues_.rend(); ++it) {
        upvalue_t uv = *it;
        if (uv->get_index() == register_index) return uv;
        if (uv->get_index() < register_index) break;
    }

    upvalue_t new_uv = heap->new_upvalue(register_index);
    auto it = std::lower_bound(context->open_upvalues_.begin(), context->open_upvalues_.end(), new_uv, [](auto a, auto b) { return a->get_index() < b->get_index(); });
    context->open_upvalues_.insert(it, new_uv);
    return new_uv;
}

inline void close_upvalues(ExecutionContext* context, size_t last_index) noexcept {
    while (!context->open_upvalues_.empty() && context->open_upvalues_.back()->get_index() >= last_index) {
        upvalue_t uv = context->open_upvalues_.back();
        // [FIX] Truy cập mảng tĩnh stack_ thay vì vector registers_
        uv->close(context->stack_[uv->get_index()]);
        context->open_upvalues_.pop_back();
    }
}

inline upvalue_t capture_upvalue(ExecutionContext& context, MemoryManager& heap, size_t register_index) noexcept {
    return capture_upvalue(&context, &heap, register_index);
}

inline void close_upvalues(ExecutionContext& context, size_t last_index) noexcept {
    return close_upvalues(&context, last_index);
}
}
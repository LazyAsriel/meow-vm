#pragma once

#include "common/pch.h"
#include "runtime/execution_context.h"
#include "memory/memory_manager.h"
#include "common/definitions.h"

namespace meow {
inline upvalue_t capture_upvalue(ExecutionContext* context, MemoryManager* heap, size_t register_index) noexcept {
    // Tìm kiếm các upvalue đã mở từ trên xuống dưới (chỉ số stack cao -> thấp)
    for (auto it = context->open_upvalues_.rbegin(); it != context->open_upvalues_.rend(); ++it) {
        upvalue_t uv = *it;
        if (uv->get_index() == register_index) return uv;
        if (uv->get_index() < register_index) break;  // Đã đi qua, không cần tìm nữa
    }

    // Không tìm thấy, tạo mới
    upvalue_t new_uv = heap->new_upvalue(register_index);

    // Chèn vào danh sách đã sắp xếp (theo chỉ số stack)
    auto it = std::lower_bound(context->open_upvalues_.begin(), context->open_upvalues_.end(), new_uv, [](auto a, auto b) { return a->get_index() < b->get_index(); });
    context->open_upvalues_.insert(it, new_uv);
    return new_uv;
}

inline void close_upvalues(ExecutionContext* context, size_t last_index) noexcept {
    // Đóng tất cả upvalue có chỉ số register >= last_index
    while (!context->open_upvalues_.empty() && context->open_upvalues_.back()->get_index() >= last_index) {
        upvalue_t uv = context->open_upvalues_.back();
        uv->close(context->registers_[uv->get_index()]);
        context->open_upvalues_.pop_back();
    }
}
}

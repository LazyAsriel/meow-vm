#pragma once

#include "runtime/execution_context.h"
#include "memory/memory_manager.h"
#include "runtime/upvalue.h"
#include "vm/vm_error.h"
#include "debug/print.h"

namespace meow {
inline bool recover_from_error(const VMError& e, ExecutionContext* context, MemoryManager* heap) noexcept {
    printl("Exception caught: {}", e.what());

    if (context->exception_handlers_.empty()) {
        printl("Uncaught exception! VM Halting.");
        return false; // Chết vinh quang, không cứu được
    }

    // 1. Lấy handler gần nhất
    auto& handlers = context->exception_handlers_;
    ExceptionHandler handler = handlers.back();
    handlers.pop_back();

    // 2. Unwind Call Stack
    while (context->call_stack_.size() - 1 > handler.frame_depth_) {
        CallFrame& frame = context->call_stack_.back();
        close_upvalues(context, frame.start_reg_);
        context->call_stack_.pop_back();
    }

    // 3. Khôi phục Register Stack
    context->registers_.resize(handler.stack_depth_);

    // 4. Khôi phục ExecutionContext
    context->current_frame_ = &context->call_stack_.back();
    context->current_base_ = context->current_frame_->start_reg_;
    
    // 5. Cập nhật IP để nhảy tới Catch Block
    // (Lưu ý: IP trong frame phải trỏ đúng chỗ để lần lặp sau dùng)
    const uint8_t* code_start = context->current_frame_->function_->get_proto()->get_chunk().get_code();
    context->current_frame_->ip_ = code_start + handler.catch_ip_;

    // 6. Ghi lỗi vào Register (nếu cần)
    if (handler.error_reg_ != static_cast<size_t>(-1)) {
        size_t abs_reg = context->current_base_ + handler.error_reg_;
        if (abs_reg >= context->registers_.size()) {
            context->registers_.resize(abs_reg + 1);
        }
        context->registers_[context->current_base_ + handler.error_reg_] = value_t(heap->new_string(e.what()));
    }

    return true; // Đã cứu thành công
}
}
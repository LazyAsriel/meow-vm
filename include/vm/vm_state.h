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

    // --- CACHED POINTERS (SIÊU TỐC ĐỘ) ---
    // Con trỏ này trỏ thẳng vào cửa sổ register của hàm hiện tại
    Value* registers; 
    const uint8_t* instruction_base;

    std::string_view error_msg;
    bool has_error_ = false;

    // --- Error Handling ---
    void error(std::string_view msg) noexcept {
        error_msg = msg;
        has_error_ = true;
    }
    bool has_error() const noexcept { return has_error_; }
    void clear_error() noexcept { has_error_ = false; }
    std::string_view get_error_message() const noexcept { return error_msg; }

    // --- Fast Accessors ---

    // Update lại pointer khi chuyển frame (CALL/RETURN)
    [[gnu::always_inline]]
    inline void update_pointers() noexcept {
        registers = ctx.current_regs_;
        // [NEW] Lấy code start từ frame hiện tại (hoặc function proto)
        // Cách nhanh nhất là lấy từ Function -> Proto -> Chunk (chấp nhận pointer chase ở đây vì chỉ làm 1 lần khi call)
        instruction_base = ctx.frame_ptr_->function_->get_proto()->get_chunk().get_code();
    }

    // Truy cập thanh ghi: Chỉ là cộng pointer (1 lệnh CPU)
    [[gnu::always_inline]] 
    inline Value& reg(uint16_t idx) noexcept {
        return registers[idx];
    }

    [[gnu::always_inline]] 
    inline Value reg(uint16_t idx) const noexcept {
        return registers[idx];
    }

    // Đọc hằng số (vẫn phải qua pointer chasing một chút, nhưng chấp nhận được)
    [[gnu::always_inline]] 
    inline Value constant(uint16_t idx) const noexcept {
        return ctx.frame_ptr_->function_->get_proto()->get_chunk().get_constant(idx);
    }
};
}
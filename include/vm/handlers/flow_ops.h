#pragma once
#include "vm/handlers/utils.h"

namespace meow {
namespace handlers {

    // --- PANIC (Xử lý lỗi & Exception) ---
    [[gnu::noinline]]
    static const uint8_t* impl_PANIC(const uint8_t* ip, VMState* state) {
        // Fix unused warning
        (void)ip;

        if (!state->ctx.exception_handlers_.empty()) {
            auto& handler = state->ctx.exception_handlers_.back();
            
            // Unwind stack
            while (state->ctx.call_stack_.size() - 1 > handler.frame_depth_) {
                meow::close_upvalues(state->ctx, state->ctx.call_stack_.back().start_reg_);
                state->ctx.call_stack_.pop_back();
            }
            
            // Restore registers
            state->ctx.registers_.resize(handler.stack_depth_);
            state->ctx.current_frame_ = &state->ctx.call_stack_.back();
            state->ctx.current_base_ = state->ctx.current_frame_->start_reg_;
            
            // Calculate catch IP
            const uint8_t* code_start = state->ctx.current_frame_->function_->get_proto()->get_chunk().get_code();
            const uint8_t* catch_ip = code_start + handler.catch_ip_;
            
            // Set error object
            if (handler.error_reg_ != static_cast<size_t>(-1)) {
                auto err_str = state->heap.new_string(state->get_error_message());
                state->reg(handler.error_reg_) = Value(err_str);
            }
            
            state->clear_error();
            state->ctx.exception_handlers_.pop_back();
            return catch_ip; // Nhảy đến catch block
        } 
        
        printl("VM Panic: {}", state->get_error_message());
        return nullptr; // Dừng VM
    }

    // --- UNIMPL ---
    [[gnu::noinline]]
    static const uint8_t* impl_UNIMPL(const uint8_t* ip, VMState* state) {
        state->error("Opcode chưa được hỗ trợ");
        return impl_PANIC(ip, state);
    }

    // --- HALT ---
    [[gnu::noinline]]
    static const uint8_t* impl_HALT(const uint8_t* /*ip*/, VMState* /*state*/) {
        return nullptr; // Trả về nullptr để wrapper dừng dispatch loop
    }

    // --- RETURN ---
    [[gnu::noinline]]
    static const uint8_t* impl_RETURN(const uint8_t* ip, VMState* state) {
        uint16_t ret_reg_idx = read_u16(ip);
        Value result = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : state->reg(ret_reg_idx);

        auto& stack = state->ctx.call_stack_;
        CallFrame* current = state->ctx.current_frame_;

        meow::close_upvalues(state->ctx, current->start_reg_);

        size_t target_reg = current->ret_reg_;
        size_t old_base = current->start_reg_;

        stack.pop_back();

        if (stack.empty()) return nullptr; // Hết stack -> Dừng VM

        state->ctx.current_frame_ = &stack.back();
        state->ctx.current_base_  = state->ctx.current_frame_->start_reg_;
        
        if (target_reg != static_cast<size_t>(-1)) {
            state->ctx.registers_[state->ctx.current_base_ + target_reg] = result;
        }

        state->ctx.registers_.resize(old_base);
        return state->ctx.current_frame_->ip_;
    }

    // --- JUMP_IF_TRUE ---
    [[gnu::noinline]]
    static const uint8_t* impl_JUMP_IF_TRUE(const uint8_t* ip, VMState* state) {
        uint16_t cond_reg = read_u16(ip);
        uint16_t offset   = read_u16(ip);

        Value& cond = state->reg(cond_reg);
        
        bool is_truthy = false;
        if (cond.is_bool()) is_truthy = cond.as_bool();
        else if (cond.is_int()) is_truthy = (cond.as_int() != 0);
        else if (!cond.is_null()) is_truthy = true;

        if (is_truthy) {
            const uint8_t* code_start = state->ctx.current_frame_->function_->get_proto()->get_chunk().get_code();
            return code_start + offset;
        }
        return ip;
    }

} // namespace handlers
} // namespace meow
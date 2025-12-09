#pragma once
#include "vm/handlers/utils.h"
#include "core/objects.h"

namespace meow {
namespace handlers {

    // --- PANIC (Xử lý lỗi & Exception) ---
    [[always_inline]]
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
    [[always_inline]]
    static const uint8_t* impl_UNIMPL(const uint8_t* ip, VMState* state) {
        state->error("Opcode chưa được hỗ trợ");
        return impl_PANIC(ip, state);
    }

    // --- HALT ---
    [[always_inline]]
    static const uint8_t* impl_HALT(const uint8_t* /*ip*/, VMState* /*state*/) {
        return nullptr; // Trả về nullptr để wrapper dừng dispatch loop
    }

    // --- RETURN ---
    [[always_inline]]
    static const uint8_t* impl_RETURN(const uint8_t* ip, VMState* state) {
        uint16_t ret_reg_idx = read_u16(ip);
        Value result = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : state->reg(ret_reg_idx);

        auto& stack = state->ctx.call_stack_;
        CallFrame* current = state->ctx.current_frame_;

        // Đóng các upvalue mở tại frame hiện tại trước khi pop
        meow::close_upvalues(state->ctx, current->start_reg_);

        size_t target_reg = current->ret_reg_;
        size_t old_base = current->start_reg_;

        stack.pop_back();

        if (stack.empty()) return nullptr; // Hết stack -> Dừng VM

        // Khôi phục ngữ cảnh caller
        state->ctx.current_frame_ = &stack.back();
        state->ctx.current_base_  = state->ctx.current_frame_->start_reg_;
        
        // Ghi kết quả trả về (nếu caller cần)
        if (target_reg != static_cast<size_t>(-1)) {
            state->ctx.registers_[state->ctx.current_base_ + target_reg] = result;
        }

        // Thu gọn stack thanh ghi
        state->ctx.registers_.resize(old_base);
        
        // Trả về IP của caller để tiếp tục thực thi
        return state->ctx.current_frame_->ip_;
    }

    // --- Helper template cho CALL (Native & Function) ---
    template <bool IsVoid>
    [[gnu::always_inline]] 
    static inline const uint8_t* do_call(const uint8_t* ip, VMState* state) {
        uint16_t dst = 0;
        if constexpr (!IsVoid) dst = read_u16(ip);

        uint16_t fn_reg    = read_u16(ip);
        uint16_t arg_start = read_u16(ip);
        uint16_t argc      = read_u16(ip);

        size_t ret_reg = static_cast<size_t>(-1);
        if constexpr (!IsVoid) {
            if (dst != 0xFFFF) ret_reg = dst;
        }

        Value& callee = state->reg(fn_reg);

        // 1. Native Call
        if (callee.is_native()) [[unlikely]] {
            native_t fn = callee.as_native();
            // Vì state->machine là tham chiếu (Machine&), ta lấy địa chỉ (&) để truyền pointer cho native_t
            Value result = fn(&state->machine, argc, &state->reg(arg_start));
            
            if constexpr (!IsVoid) {
                if (ret_reg != static_cast<size_t>(-1)) {
                    state->reg(ret_reg) = result;
                }
            }
            return ip;
        }

        // 2. Meow Function / Method / Class Call
        instance_t self = nullptr;
        function_t closure = nullptr;
        bool is_init = false;

        if (callee.is_function()) {
            closure = callee.as_function();
        } 
        else if (callee.is_bound_method()) {
            bound_method_t bound = callee.as_bound_method();
            self = bound->get_instance();
            closure = bound->get_function();
        } 
        else if (callee.is_class()) {
            class_t klass = callee.as_class();
            self = state->heap.new_instance(klass);
            
            if constexpr (!IsVoid) {
                if (ret_reg != static_cast<size_t>(-1)) {
                    state->reg(ret_reg) = Value(self);
                }
            }
            
            // Tìm constructor "init"
            Value init_method = klass->get_method(state->heap.new_string("init"));
            if (init_method.is_function()) {
                closure = init_method.as_function();
                is_init = true;
            } else {
                return ip; // Không có init -> khởi tạo xong
            }
        } 
        else {
            state->error("Giá trị không thể gọi được (Not callable).");
            return impl_PANIC(ip, state);
        }

        // Setup Stack Frame mới
        proto_t proto = closure->get_proto();
        size_t num_params = proto->get_num_registers();
        size_t current_top = state->ctx.registers_.size();
        
        state->ctx.registers_.resize(current_top + num_params);

        size_t arg_offset = 0;
        // Nếu có self (method/constructor), tham số đầu tiên (R0) là self
        if (self != nullptr && num_params > 0) {
            state->ctx.registers_[current_top] = Value(self);
            arg_offset = 1;
        }

        // Copy tham số
        for (size_t i = 0; i < argc; ++i) {
            if (arg_offset + i < num_params) {
                state->ctx.registers_[current_top + arg_offset + i] = state->reg(arg_start + i);
            }
        }

        // Lưu IP hiện tại vào frame cũ
        state->ctx.current_frame_->ip_ = ip;
        
        // Nếu gọi constructor, ta không quan tâm kết quả hàm init trả về (vì đã có instance rồi)
        size_t frame_ret_reg = is_init ? static_cast<size_t>(-1) : ret_reg;

        state->ctx.call_stack_.emplace_back(
            closure,
            state->ctx.current_frame_->module_,
            current_top,
            frame_ret_reg,
            proto->get_chunk().get_code()
        );

        state->ctx.current_frame_ = &state->ctx.call_stack_.back();
        state->ctx.current_base_  = current_top;

        // Nhảy tới code của hàm mới
        return state->ctx.current_frame_->ip_;
    }

    // --- CALL Handlers (Wrappers) ---
    [[always_inline]] 
    static const uint8_t* impl_CALL(const uint8_t* ip, VMState* state) {
        return do_call<false>(ip, state);
    }

    [[always_inline]] 
    static const uint8_t* impl_CALL_VOID(const uint8_t* ip, VMState* state) {
        return do_call<true>(ip, state);
    }

    // --- JUMP Handlers ---
    [[always_inline]]
    static const uint8_t* impl_JUMP(const uint8_t* ip, VMState* state) {
        uint16_t offset = read_u16(ip);
        const uint8_t* code_start = state->ctx.current_frame_->function_->get_proto()->get_chunk().get_code();
        return code_start + offset;
    }

    [[always_inline]]
    static const uint8_t* impl_JUMP_IF_TRUE(const uint8_t* ip, VMState* state) {
        uint16_t cond_reg = read_u16(ip);
        uint16_t offset   = read_u16(ip);

        if (meow::to_bool(state->reg(cond_reg))) {
            const uint8_t* code_start = state->ctx.current_frame_->function_->get_proto()->get_chunk().get_code();
            return code_start + offset;
        }
        return ip;
    }

    [[always_inline]]
    static const uint8_t* impl_JUMP_IF_FALSE(const uint8_t* ip, VMState* state) {
        uint16_t cond_reg = read_u16(ip);
        uint16_t offset   = read_u16(ip);

        if (!meow::to_bool(state->reg(cond_reg))) {
            const uint8_t* code_start = state->ctx.current_frame_->function_->get_proto()->get_chunk().get_code();
            return code_start + offset;
        }
        return ip;
    }

} // namespace handlers
} // namespace meow
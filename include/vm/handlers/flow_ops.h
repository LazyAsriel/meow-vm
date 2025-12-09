#pragma once
#include "vm/handlers/utils.h"
#include "core/objects.h"

namespace meow::handlers {

    // --- PANIC (Xử lý lỗi runtime Meow) ---
    [[gnu::always_inline]]
    inline static const uint8_t* impl_PANIC(const uint8_t* ip, VMState* state) {
        (void)ip;
        
        // 1. Tìm Exception Handler gần nhất
        if (!state->ctx.exception_handlers_.empty()) {
            auto& handler = state->ctx.exception_handlers_.back();
            
            // Unwind stack
            while (state->ctx.call_stack_.size() - 1 > handler.frame_depth_) {
                meow::close_upvalues(state->ctx, state->ctx.call_stack_.back().start_reg_);
                state->ctx.call_stack_.pop_back();
            }
            
            // Restore registers & frame
            state->ctx.registers_.resize(handler.stack_depth_);
            state->ctx.current_frame_ = &state->ctx.call_stack_.back();
            state->ctx.current_base_ = state->ctx.current_frame_->start_reg_;
            
            // Tính địa chỉ Catch
            const uint8_t* code_start = state->ctx.current_frame_->function_->get_proto()->get_chunk().get_code();
            const uint8_t* catch_ip = code_start + handler.catch_ip_;
            
            // Lưu lỗi
            if (handler.error_reg_ != static_cast<size_t>(-1)) {
                auto err_str = state->heap.new_string(state->get_error_message());
                state->reg(handler.error_reg_) = Value(err_str);
            }
            
            state->clear_error();
            state->ctx.exception_handlers_.pop_back();
            return catch_ip;
        } 
        
        // 2. Không bắt được lỗi -> Dừng VM (return nullptr)
        // Interpreter::run sẽ thoát vòng lặp
        printl("VM Panic: {}", state->get_error_message());
        return nullptr; 
    }

    // --- UNIMPL ---
    [[gnu::always_inline]]
    inline static const uint8_t* impl_UNIMPL(const uint8_t* ip, VMState* state) {
        state->error("Opcode chưa được hỗ trợ (UNIMPL)");
        return impl_PANIC(ip, state);
    }

    // --- HALT ---
    [[gnu::always_inline]]
    inline static const uint8_t* impl_HALT(const uint8_t* /*ip*/, VMState* /*state*/) {
        return nullptr; // Dừng dispatcher
    }

    // --- RETURN ---
    [[gnu::always_inline]]
    inline static const uint8_t* impl_RETURN(const uint8_t* ip, VMState* state) {
        uint16_t ret_reg_idx = read_u16(ip);
        
        // Lấy kết quả trước khi pop stack (vì stack sẽ bị thu hồi)
        Value result = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : state->reg(ret_reg_idx);

        CallFrame* current = state->ctx.current_frame_;
        meow::close_upvalues(state->ctx, current->start_reg_);

        // Lưu thông tin để khôi phục
        size_t target_reg = current->ret_reg_;
        size_t old_base = current->start_reg_;

        state->ctx.call_stack_.pop_back();

        // Nếu hết stack -> Kết thúc chương trình chính
        if (state->ctx.call_stack_.empty()) return nullptr;

        // Khôi phục ngữ cảnh Caller
        state->ctx.current_frame_ = &state->ctx.call_stack_.back();
        state->ctx.current_base_  = state->ctx.current_frame_->start_reg_;
        
        // Ghi kết quả trả về
        if (target_reg != static_cast<size_t>(-1)) {
            state->ctx.registers_[state->ctx.current_base_ + target_reg] = result;
        }

        // Thu hồi thanh ghi
        state->ctx.registers_.resize(old_base);
        
        return state->ctx.current_frame_->ip_;
    }

    // --- CALL Helper ---
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
            // CHÚ Ý: state->machine là tham chiếu (Machine&), cần lấy địa chỉ (&) để truyền pointer
            Value result = fn(&state->machine, argc, &state->reg(arg_start));
            
            if constexpr (!IsVoid) {
                if (ret_reg != static_cast<size_t>(-1)) {
                    state->reg(ret_reg) = result;
                }
            }
            return ip;
        }

        // 2. Meow Call (Function / Method / Class)
        // Code đoạn này giữ nguyên logic như cũ, chỉ tối ưu memory access nếu cần
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
                if (ret_reg != static_cast<size_t>(-1)) state->reg(ret_reg) = Value(self);
            }
            
            // TODO: Cache chuỗi "init" vào VMState để tránh new_string liên tục
            Value init_method = klass->get_method(state->heap.new_string("init"));
            if (init_method.is_function()) {
                closure = init_method.as_function();
                is_init = true;
            } else {
                return ip;
            }
        } 
        else {
            state->error("Not callable");
            return impl_PANIC(ip, state);
        }

        // Setup Frame
        proto_t proto = closure->get_proto();
        size_t num_params = proto->get_num_registers();
        size_t current_top = state->ctx.registers_.size();
        
        // Resize 1 lần duy nhất
        state->ctx.registers_.resize(current_top + num_params);

        size_t arg_offset = 0;
        if (self != nullptr && num_params > 0) {
            state->ctx.registers_[current_top] = Value(self);
            arg_offset = 1;
        }

        // Copy args (Loop unrolling có thể được compiler tự làm)
        for (size_t i = 0; i < argc; ++i) {
            if (arg_offset + i < num_params) {
                state->ctx.registers_[current_top + arg_offset + i] = state->reg(arg_start + i);
            }
        }

        state->ctx.current_frame_->ip_ = ip; // Save return address
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

        return state->ctx.current_frame_->ip_;
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_CALL(const uint8_t* ip, VMState* state) {
        return do_call<false>(ip, state);
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_CALL_VOID(const uint8_t* ip, VMState* state) {
        return do_call<true>(ip, state);
    }

    // --- JUMP ---
    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP(const uint8_t* ip, VMState* state) {
        uint16_t offset = read_u16(ip);
        const uint8_t* code_start = state->ctx.current_frame_->function_->get_proto()->get_chunk().get_code();
        return code_start + offset;
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_TRUE(const uint8_t* ip, VMState* state) {
        uint16_t cond_reg = read_u16(ip);
        uint16_t offset   = read_u16(ip);
        Value& cond = state->reg(cond_reg);

        // Fast path check bool/int
        bool truthy = false;
        if (cond.is_bool()) [[likely]] truthy = cond.as_bool();
        else if (cond.is_int()) truthy = (cond.as_int() != 0);
        else truthy = meow::to_bool(cond);

        if (truthy) {
            const uint8_t* code_start = state->ctx.current_frame_->function_->get_proto()->get_chunk().get_code();
            return code_start + offset;
        }
        return ip;
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_FALSE(const uint8_t* ip, VMState* state) {
        uint16_t cond_reg = read_u16(ip);
        uint16_t offset   = read_u16(ip);
        Value& cond = state->reg(cond_reg);

        bool truthy = false;
        if (cond.is_bool()) [[likely]] truthy = cond.as_bool();
        else if (cond.is_int()) truthy = (cond.as_int() != 0);
        else truthy = meow::to_bool(cond);

        if (!truthy) {
            const uint8_t* code_start = state->ctx.current_frame_->function_->get_proto()->get_chunk().get_code();
            return code_start + offset;
        }
        return ip;
    }

} // namespace handlers

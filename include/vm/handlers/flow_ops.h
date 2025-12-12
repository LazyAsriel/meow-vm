#pragma once
#include "vm/handlers/utils.h"
#include "core/objects.h"
#include <cstring> // Cần cho memcpy nếu fallback, nhưng ở đây ta dùng packed

namespace meow::handlers {

    // --- Cấu trúc giải mã ---
    struct JumpCondArgs { 
        uint16_t cond; 
        uint16_t offset; 
    } __attribute__((packed));

    struct JumpCondArgsB { 
        uint8_t cond; 
        uint16_t offset; 
    } __attribute__((packed));

    // --- Inline Cache Structure (Packed) ---
    // [OPTIMIZATION] __attribute__((packed)) giúp compiler tự xử lý unaligned access
    // mà không cần memcpy thủ công. Hiệu năng cực cao trên x86_64.
    struct CallIC {
        void* check_tag;   // 8 bytes
        void* destination; // 8 bytes (Padding/Future JIT target)
    } __attribute__((packed)); 

    // Helper đọc IC an toàn
    [[gnu::always_inline]]
    inline static CallIC* get_call_ic(const uint8_t*& ip) {
        // Lấy con trỏ, ép kiểu sang packed struct
        auto* ic = reinterpret_cast<CallIC*>(const_cast<uint8_t*>(ip));
        ip += sizeof(CallIC); // Nhảy qua 16 bytes
        return ic;
    }

    // ========================================================================
    // BASIC HANDLERS
    // ========================================================================

    [[gnu::always_inline]]
    inline static const uint8_t* impl_PANIC(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
        (void)ip; (void)regs; (void)constants;
        
        if (!state->ctx.exception_handlers_.empty()) {
            auto& handler = state->ctx.exception_handlers_.back();
            long current_depth = state->ctx.frame_ptr_ - state->ctx.call_stack_;
            
            while (current_depth > (long)handler.frame_depth_) {
                size_t reg_idx = state->ctx.frame_ptr_->regs_base_ - state->ctx.stack_;
                meow::close_upvalues(state->ctx, reg_idx);
                state->ctx.frame_ptr_--;
                current_depth--;
            }
            
            state->ctx.stack_top_ = state->ctx.stack_ + handler.stack_depth_;
            state->ctx.current_regs_ = state->ctx.frame_ptr_->regs_base_;
            state->ctx.current_frame_ = state->ctx.frame_ptr_; 
            state->update_pointers();

            const uint8_t* catch_ip = state->instruction_base + handler.catch_ip_;
            
            if (handler.error_reg_ != static_cast<size_t>(-1)) {
                auto err_str = state->heap.new_string(state->get_error_message());
                regs[handler.error_reg_] = Value(err_str);
            }
            
            state->clear_error();
            state->ctx.exception_handlers_.pop_back();
            return catch_ip;
        } 
        
        printl("VM Panic: {}", state->get_error_message());
        return nullptr; 
    }

    [[gnu::always_inline]]
    inline static const uint8_t* impl_UNIMPL(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
        state->error("Opcode chưa được hỗ trợ (UNIMPL)");
        return impl_PANIC(ip, regs, constants, state);
    }

    [[gnu::always_inline]]
    inline static const uint8_t* impl_HALT(const uint8_t*, Value*, Value*, VMState*) {
        return nullptr;
    }

    // ========================================================================
    // JUMPS
    // ========================================================================

    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
        uint16_t offset = *reinterpret_cast<const uint16_t*>(ip);
        return state->instruction_base + offset;
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_TRUE(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
        const auto& args = *reinterpret_cast<const JumpCondArgs*>(ip);
        Value& cond = regs[args.cond];

        bool truthy;
        if (cond.is_bool()) [[likely]] truthy = cond.as_bool();
        else if (cond.is_int()) truthy = (cond.as_int() != 0);
        else truthy = meow::to_bool(cond);

        if (truthy) return state->instruction_base + args.offset;
        return ip + sizeof(JumpCondArgs);
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_FALSE(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
        const auto& args = *reinterpret_cast<const JumpCondArgs*>(ip);
        Value& cond = regs[args.cond];

        bool truthy;
        if (cond.is_bool()) [[likely]] truthy = cond.as_bool();
        else if (cond.is_int()) truthy = (cond.as_int() != 0);
        else truthy = meow::to_bool(cond);

        if (!truthy) return state->instruction_base + args.offset;
        return ip + sizeof(JumpCondArgs);
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_TRUE_B(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
        const auto& args = *reinterpret_cast<const JumpCondArgsB*>(ip);
        Value& cond = regs[args.cond];
        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
        if (truthy) return state->instruction_base + args.offset;
        return ip + sizeof(JumpCondArgsB);
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_FALSE_B(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
        const auto& args = *reinterpret_cast<const JumpCondArgsB*>(ip);
        Value& cond = regs[args.cond];
        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
        if (!truthy) return state->instruction_base + args.offset;
        return ip + sizeof(JumpCondArgsB);
    }

    // ========================================================================
    // RETURN
    // ========================================================================

    [[gnu::always_inline]]
    inline static const uint8_t* impl_RETURN(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
        uint16_t ret_reg_idx = read_u16(ip);
        Value result = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : regs[ret_reg_idx];

        size_t base_idx = state->ctx.current_regs_ - state->ctx.stack_;
        meow::close_upvalues(state->ctx, base_idx);

        if (state->ctx.frame_ptr_ == state->ctx.call_stack_) {
            return nullptr; 
        }

        CallFrame* popped_frame = state->ctx.frame_ptr_;
        state->ctx.frame_ptr_--;
        CallFrame* caller = state->ctx.frame_ptr_;
        
        state->ctx.stack_top_ = popped_frame->regs_base_;
        state->ctx.current_regs_ = caller->regs_base_;
        state->ctx.current_frame_ = caller; 
        
        state->update_pointers(); 

        if (popped_frame->ret_dest_ != nullptr) {
            *popped_frame->ret_dest_ = result;
        }

        return popped_frame->ip_; 
    }

    // ========================================================================
    // CALL (OPTIMIZED WITH PACKED INLINE CACHE)
    // ========================================================================

    template <bool IsVoid>
    [[gnu::always_inline]] 
    static inline const uint8_t* do_call(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
        uint16_t dst = 0;
        if constexpr (!IsVoid) dst = read_u16(ip);
        uint16_t fn_reg    = read_u16(ip);
        uint16_t arg_start = read_u16(ip);
        uint16_t argc      = read_u16(ip);

        // [FIX] Lấy packed struct (An toàn & Nhanh)
        CallIC* ic = get_call_ic(ip);

        Value& callee = regs[fn_reg];

        std::println("DEBUG: CALL trying to invoke: {}", to_string(callee));
        if (callee.is_bool()) {
            std::println("!!! ALARM !!! Callee is BOOL: {}", callee.as_bool());
        }
        // ---------------------------------------------------------
        // FAST PATH: Function Call
        // ---------------------------------------------------------
        if (callee.is_function()) {
            function_t closure = callee.as_function();
            proto_t proto = closure->get_proto();

            // So sánh trực tiếp (Compiler sẽ sinh lệnh load unaligned an toàn)
            if (ic->check_tag == proto) {
                size_t num_params = proto->get_num_registers();
                
                if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
                    state->error("Stack Overflow!");
                    return impl_PANIC(ip, regs, constants, state);
                }

                Value* new_base = state->ctx.stack_top_;
                
                for (size_t i = 0; i < argc; ++i) {
                    if (i < num_params) new_base[i] = regs[arg_start + i];
                }

                state->ctx.frame_ptr_++;
                *state->ctx.frame_ptr_ = CallFrame(
                    closure,
                    new_base,
                    IsVoid ? nullptr : &regs[dst], 
                    ip 
                );
                
                state->ctx.current_regs_ = new_base;
                state->ctx.stack_top_ += num_params;
                state->ctx.current_frame_ = state->ctx.frame_ptr_;
                state->update_pointers(); 

                return state->instruction_base;
            }
            
            // CACHE MISS: Cập nhật tag (Ghi đè an toàn)
            ic->check_tag = proto;
        } 
        // ---------------------------------------------------------
        // FAST PATH: Native Call
        // ---------------------------------------------------------
        else if (callee.is_native()) {
            native_t fn = callee.as_native();
            
            if (ic->check_tag == (void*)fn) {
                Value result = fn(&state->machine, argc, &regs[arg_start]);
                if constexpr (!IsVoid) regs[dst] = result;
                return ip; 
            }
            
            ic->check_tag = (void*)fn;
        }

        // ---------------------------------------------------------
        // SLOW PATH / FALLBACK
        // ---------------------------------------------------------
        
        Value* ret_dest_ptr = nullptr;
        if constexpr (!IsVoid) {
            if (dst != 0xFFFF) ret_dest_ptr = &regs[dst];
        }

        if (callee.is_native()) {
            native_t fn = callee.as_native();
            Value result = fn(&state->machine, argc, &regs[arg_start]);
            if (ret_dest_ptr) *ret_dest_ptr = result;
            return ip;
        }

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
            self = state->heap.new_instance(klass, state->heap.get_empty_shape());
            if (ret_dest_ptr) *ret_dest_ptr = Value(self);
            
            Value init_method = klass->get_method(state->heap.new_string("init"));
            if (init_method.is_function()) {
                closure = init_method.as_function();
                is_init = true;
            } else {
                return ip; 
            }
        } 
        else {
            state->error(std::format("Giá trị loại '{}' không thể gọi được (Not callable).", to_string(callee)));
            return impl_PANIC(ip, regs, constants, state);
        }

        proto_t proto = closure->get_proto();
        size_t num_params = proto->get_num_registers();

        if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
            state->error("Stack Overflow!");
            return impl_PANIC(ip, regs, constants, state);
        }

        Value* new_base = state->ctx.stack_top_;
        size_t arg_offset = 0;
        
        if (self != nullptr && num_params > 0) {
            new_base[0] = Value(self);
            arg_offset = 1;
        }

        for (size_t i = 0; i < argc; ++i) {
            if (arg_offset + i < num_params) {
                new_base[arg_offset + i] = regs[arg_start + i];
            }
        }

        state->ctx.frame_ptr_++;
        *state->ctx.frame_ptr_ = CallFrame(
            closure,
            new_base,                          
            is_init ? nullptr : ret_dest_ptr,  
            ip                                 
        );

        state->ctx.current_regs_ = new_base;
        state->ctx.stack_top_ += num_params;
        state->ctx.current_frame_ = state->ctx.frame_ptr_;
        state->update_pointers(); 

        return state->instruction_base;
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_CALL(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
        return do_call<false>(ip, regs, constants, state);
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_CALL_VOID(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
        return do_call<true>(ip, regs, constants, state);
    }

} // namespace meow::handlers
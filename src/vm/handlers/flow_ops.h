#pragma once
#include "vm/handlers/utils.h"
#include <meow/core/objects.h>
#include <meow/machine.h>
#include <cstring>
#include <vector>

namespace meow::handlers {

    // Giữ lại CallIC vì nó được patch trực tiếp vào bytecode khi chạy
    struct CallIC {
        void* check_tag;
        void* destination;
    } __attribute__((packed)); 

    // Helper: Push Stack Frame (Giữ nguyên logic nhưng cleanup code)
    [[gnu::always_inline]]
    inline static const uint8_t* push_call_frame(
        VMState* state, 
        function_t closure, 
        int argc, 
        Value* args_src,       
        Value* receiver,       
        Value* ret_dest,       
        const uint8_t* ret_ip, 
        const uint8_t* err_ip  
    ) {
        proto_t proto = closure->get_proto();
        size_t num_params = proto->get_num_registers();

        // Check Stack Overflow
        if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
            state->error("Stack Overflow!", err_ip); 
            return nullptr;
        }

        Value* new_base = state->ctx.stack_top_;
        size_t arg_offset = 0;

        // Setup Receiver (this)
        if (receiver != nullptr && num_params > 0) {
            new_base[0] = *receiver;
            arg_offset = 1; 
        }

        // Copy Arguments
        size_t copy_count = (argc < (num_params - arg_offset)) ? argc : (num_params - arg_offset);
        if (copy_count > 0) {
            // Dùng memcpy hoặc loop tùy compiler optimize, ở đây loop cho an toàn type
            for (size_t i = 0; i < copy_count; ++i) {
                new_base[arg_offset + i] = args_src[i];
            }
        }

        // Fill Null cho các param còn thiếu
        size_t filled = arg_offset + argc;
        for (size_t i = filled; i < num_params; ++i) {
            new_base[i] = Value(null_t{});
        }

        // Push Frame
        state->ctx.frame_ptr_++;
        *state->ctx.frame_ptr_ = CallFrame(closure, new_base, ret_dest, ret_ip);
        
        // Update State Pointers
        state->ctx.current_regs_ = new_base;
        state->ctx.stack_top_ += num_params;
        state->ctx.current_frame_ = state->ctx.frame_ptr_;
        state->update_pointers(); 

        // Jump to function code
        return state->instruction_base; 
    }

    // --- BASIC OPS ---

    [[gnu::always_inline]]
    inline static const uint8_t* impl_NOP(const uint8_t* ip, Value*, const Value*, VMState*) { return ip; }

    [[gnu::always_inline]]
    inline static const uint8_t* impl_HALT(const uint8_t*, Value*, const Value*, VMState*) { return nullptr; }

    // --- PANIC HANDLER (Exception Unwinding) ---
    [[gnu::always_inline]]
    inline const uint8_t* impl_PANIC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        (void)ip; (void)regs; (void)constants;
        
        // Nếu có Exception Handler (Try-Catch)
        if (!state->ctx.exception_handlers_.empty()) {
            auto& handler = state->ctx.exception_handlers_.back();
            
            // Unwind Stack Frames
            long current_depth = state->ctx.frame_ptr_ - state->ctx.call_stack_;
            while (current_depth > (long)handler.frame_depth_) {
                size_t reg_idx = state->ctx.frame_ptr_->regs_base_ - state->ctx.stack_;
                meow::close_upvalues(state->ctx, reg_idx);
                state->ctx.frame_ptr_--;
                current_depth--;
            }
            
            // Restore State
            state->ctx.stack_top_ = state->ctx.stack_ + handler.stack_depth_;
            state->ctx.current_regs_ = state->ctx.frame_ptr_->regs_base_;
            state->ctx.current_frame_ = state->ctx.frame_ptr_; 
            state->update_pointers();
            
            const uint8_t* catch_ip = state->instruction_base + handler.catch_ip_;
            
            // Push Error Object
            if (handler.error_reg_ != static_cast<size_t>(-1)) {
                auto err_str = state->heap.new_string(state->get_error_message());
                regs[handler.error_reg_] = Value(err_str);
            }
            
            state->clear_error();
            state->ctx.exception_handlers_.pop_back();
            return catch_ip;
        } 
        
        // Crash nếu không bắt được lỗi
        std::cerr << "VM Panic: " << state->get_error_message() << "\n";
        return nullptr; 
    }

    [[gnu::always_inline]]
    inline static const uint8_t* impl_UNIMPL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        state->error("Opcode chưa được hỗ trợ (UNIMPL)", ip);
        return impl_PANIC(ip, regs, constants, state);
    }

    // --- JUMP OPS ---

    [[gnu::always_inline]] 
    inline static const uint8_t* impl_JUMP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        // Load i16 -> IP đã tự tăng 2 byte
        auto [offset] = decode::args<i16>(ip);
        return ip + offset; 
    }

    [[gnu::always_inline]] 
    inline static const uint8_t* impl_JUMP_IF_TRUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        // 2 * u16 = 4 bytes -> Load u32
        auto [cond_idx, offset] = decode::args<u16, i16>(ip);
        
        Value& cond = regs[cond_idx];
        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
        
        if (truthy) return ip + offset;
        return ip;
    }

    [[gnu::always_inline]] 
    inline static const uint8_t* impl_JUMP_IF_FALSE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        auto [cond_idx, offset] = decode::args<u16, i16>(ip);
        
        Value& cond = regs[cond_idx];
        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
        
        if (!truthy) return ip + offset;
        return ip;
    }

    // --- BYTE JUMPS (_B) ---

    [[gnu::always_inline, gnu::hot]]
    inline static const uint8_t* impl_JUMP_IF_TRUE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        // u8 + i16 = 3 bytes -> Load u32 (tận dụng padding)
        auto [cond_idx, offset] = decode::args<u8, i16>(ip);
        
        Value& cond = regs[cond_idx];
        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
        
        if (truthy) return ip + offset;
        return ip;
    }

    [[gnu::always_inline]] 
    inline static const uint8_t* impl_JUMP_IF_FALSE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        auto [cond_idx, offset] = decode::args<u8, i16>(ip);
        
        Value& cond = regs[cond_idx];
        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
        
        if (!truthy) return ip + offset;
        return ip;
    }

    // --- RETURN ---

    [[gnu::always_inline]]
    inline static const uint8_t* impl_RETURN(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        auto [ret_reg_idx] = decode::args<u16>(ip);
        
        Value result = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : regs[ret_reg_idx];

        size_t base_idx = state->ctx.current_regs_ - state->ctx.stack_;
        meow::close_upvalues(state->ctx, base_idx);

        if (state->ctx.frame_ptr_ == state->ctx.call_stack_) [[unlikely]] return nullptr; 

        CallFrame* popped_frame = state->ctx.frame_ptr_;
        
        // Module Execution Flag
        if (state->current_module) [[likely]] {
             if (popped_frame->function_->get_proto() == state->current_module->get_main_proto()) [[unlikely]] {
                 state->current_module->set_executed();
             }
        }

        state->ctx.frame_ptr_--;
        CallFrame* caller = state->ctx.frame_ptr_;
        
        state->ctx.stack_top_ = popped_frame->regs_base_;
        state->ctx.current_regs_ = caller->regs_base_;
        state->ctx.current_frame_ = caller; 
        state->update_pointers(); 

        if (popped_frame->ret_dest_ != nullptr) *popped_frame->ret_dest_ = result;
        return popped_frame->ip_; 
    }

    // --- CALL INFRASTRUCTURE ---

template <bool IsVoid>
    [[gnu::always_inline]] 
    static inline const uint8_t* do_call(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        u16 dst = 0xFFFF;
        u16 fn_reg, arg_start, argc;

        // 1. Decode Arguments
        if constexpr (!IsVoid) {
            auto [d, f, s, c] = decode::args<u16, u16, u16, u16>(ip);
            dst       = d;
            fn_reg    = f;
            arg_start = s;
            argc      = c;
        } else {
            auto [f, s, c] = decode::args<u16, u16, u16>(ip);
            fn_reg    = f;
            arg_start = s;
            argc      = c;
        }

        // 2. Decode Inline Cache
        const CallIC* ic = decode::as_struct<CallIC>(ip);
        
        // Tính offset để báo lỗi chính xác
        constexpr size_t ErrOffset = (IsVoid ? 6 : 8) + 16;

        Value& callee = regs[fn_reg];
        function_t closure = nullptr;

        // [SỬA LỖI Ở ĐÂY]: Tách logic constexpr ra khỏi dòng khởi tạo
        Value* ret_dest_ptr = nullptr;
        if constexpr (!IsVoid) {
            if (dst != 0xFFFF) {
                ret_dest_ptr = &regs[dst];
            }
        }

        // --- A. Function Call ---
        if (callee.is_function()) [[likely]] {
            closure = callee.as_function();
            if (ic->check_tag == closure->get_proto()) [[likely]] goto SETUP_FRAME;
            const_cast<CallIC*>(ic)->check_tag = closure->get_proto();
            goto SETUP_FRAME;
        }

        // --- B. Native Call ---
        if (callee.is_native()) {
            native_t fn = callee.as_native();
            if (ic->check_tag != (void*)fn) const_cast<CallIC*>(ic)->check_tag = (void*)fn;
            
            Value result = fn(&state->machine, argc, &regs[arg_start]);
            
            if (state->machine.has_error()) [[unlikely]] {
                state->error(std::string(state->machine.get_error_message()), ip - ErrOffset - 1);
                state->machine.clear_error();
                return impl_PANIC(ip, regs, constants, state);
            }
            if constexpr (!IsVoid) {
                if (dst != 0xFFFF) regs[dst] = result;
            }
            return ip;
        }

        // --- C. Bound Method ---
        if (callee.is_bound_method()) {
            bound_method_t bound = callee.as_bound_method();
            Value method = bound->get_method();
            Value receiver = bound->get_receiver();

            if (method.is_function()) {
                closure = method.as_function();
                proto_t proto = closure->get_proto();
                size_t num_params = proto->get_num_registers();

                if (!state->ctx.check_frame_overflow()) 
                     return ERROR<ErrOffset>(ip, regs, constants, state, 90, "Stack Overflow");

                Value* new_base = state->ctx.stack_top_;
                
                new_base[0] = receiver; 
                size_t safe_argc = static_cast<size_t>(argc);
                size_t copy_cnt = (safe_argc < num_params - 1) ? safe_argc : num_params - 1;
                
                if (copy_cnt > 0) std::memcpy((void*)(new_base + 1), &regs[arg_start], copy_cnt * sizeof(Value));
                
                size_t filled = 1 + copy_cnt;
                for (size_t i = filled; i < num_params; ++i) new_base[i] = Value(null_t{});

                state->ctx.frame_ptr_++;
                *state->ctx.frame_ptr_ = CallFrame(closure, new_base, ret_dest_ptr, ip);
                
                state->ctx.current_regs_ = new_base;
                state->ctx.stack_top_ += num_params;
                state->ctx.current_frame_ = state->ctx.frame_ptr_;
                state->update_pointers(); 
                return state->instruction_base;
            }
            
            if (method.is_native()) {
                native_t fn = method.as_native();
                constexpr size_t STATIC_ARG_LIMIT = 64;
                Value stack_args[STATIC_ARG_LIMIT];
                Value* arg_ptr = stack_args;
                
                std::vector<Value> heap_args;
                if (argc + 1 > STATIC_ARG_LIMIT) {
                        heap_args.resize(argc + 1);
                        arg_ptr = heap_args.data();
                }
                arg_ptr[0] = receiver;
                if (argc > 0) std::memcpy((void*)(arg_ptr + 1), &regs[arg_start], argc * sizeof(Value));

                Value result = fn(&state->machine, argc + 1, arg_ptr);
                
                if (state->machine.has_error()) return impl_PANIC(ip, regs, constants, state);
                if constexpr (!IsVoid) if (dst != 0xFFFF) regs[dst] = result;
                return ip;
            }
        }
        
        // --- D. Class Constructor ---
        else if (callee.is_class()) {
            class_t klass = callee.as_class();
            instance_t self = state->heap.new_instance(klass, state->heap.get_empty_shape());
            if (ret_dest_ptr) *ret_dest_ptr = Value(self);
            
            Value init_method = klass->get_method(state->heap.new_string("init"));
            if (init_method.is_function()) {
                closure = init_method.as_function();
                proto_t proto = closure->get_proto();
                size_t num_params = proto->get_num_registers();
                
                if (!state->ctx.check_frame_overflow()) 
                    return ERROR<ErrOffset>(ip, regs, constants, state, 90, "Stack Overflow");
                
                Value* new_base = state->ctx.stack_top_;
                new_base[0] = Value(self);
                
                size_t safe_argc = static_cast<size_t>(argc);
                size_t copy_cnt = (safe_argc < num_params - 1) ? safe_argc : num_params - 1;
                
                if (copy_cnt > 0) std::memcpy((void*)(new_base + 1), &regs[arg_start], copy_cnt * sizeof(Value));
                
                for (size_t i = 1 + copy_cnt; i < num_params; ++i) new_base[i] = Value(null_t{});
                
                state->ctx.frame_ptr_++;
                *state->ctx.frame_ptr_ = CallFrame(closure, new_base, nullptr, ip);
                state->ctx.current_regs_ = new_base;
                state->ctx.stack_top_ += num_params;
                state->ctx.current_frame_ = state->ctx.frame_ptr_;
                state->update_pointers();
                return state->instruction_base;
            } 
            return ip;
        }

        return ERROR<ErrOffset>(ip, regs, constants, state, 91, "Value '{}' is not callable.", to_string(callee));

    SETUP_FRAME:
        {
            proto_t proto = closure->get_proto();
            size_t num_params = proto->get_num_registers();

            if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
                return ERROR<ErrOffset>(ip, regs, constants, state, 90, "Stack Overflow");
            }

            Value* new_base = state->ctx.stack_top_;
            
            size_t safe_argc = static_cast<size_t>(argc);
            size_t copy_count = (safe_argc < num_params) ? safe_argc : num_params;
            
            if (copy_count > 0) {
                std::memcpy((void*)new_base, &regs[arg_start], copy_count * sizeof(Value));
            }

            for (size_t i = copy_count; i < num_params; ++i) {
                new_base[i] = Value(null_t{});
            }

            state->ctx.frame_ptr_++;
            *state->ctx.frame_ptr_ = CallFrame(closure, new_base, ret_dest_ptr, ip);
            
            state->ctx.current_regs_ = new_base;
            state->ctx.stack_top_ += num_params;
            state->ctx.current_frame_ = state->ctx.frame_ptr_;
            state->update_pointers(); 

            return state->instruction_base;
        }
    }
    
    // --- WRAPPERS ---

    [[gnu::always_inline]] 
    inline static const uint8_t* impl_CALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        return do_call<false>(ip, regs, constants, state);
    }

    [[gnu::always_inline]] 
    inline static const uint8_t* impl_CALL_VOID(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        return do_call<true>(ip, regs, constants, state);
    }

    [[gnu::always_inline]] 
    static const uint8_t* impl_TAIL_CALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        // 4 * u16 = 8 bytes
        auto [dst, fn_reg, arg_start, argc] = decode::args<u16, u16, u16, u16>(ip);
        
        // Skip IC (16 bytes) vì Tail Call không dùng IC (hoặc chưa implement)
        ip += 16; 

        Value& callee = regs[fn_reg];
        if (!callee.is_function()) [[unlikely]] {
            // Offset = 8 bytes args + 16 bytes IC = 24
            return ERROR<24>(ip, regs, constants, state, 91, "TAIL_CALL target is not a function");
        }
        
        function_t closure = callee.as_function();
        proto_t proto = closure->get_proto();
        size_t num_params = proto->get_num_registers();

        size_t current_base_idx = regs - state->ctx.stack_;
        meow::close_upvalues(state->ctx, current_base_idx);

        size_t copy_count = (argc < num_params) ? argc : num_params;
        for (size_t i = 0; i < copy_count; ++i) regs[i] = regs[arg_start + i];
        for (size_t i = copy_count; i < num_params; ++i) regs[i] = Value(null_t{});

        CallFrame* current_frame = state->ctx.frame_ptr_;
        current_frame->function_ = closure;
        state->ctx.stack_top_ = regs + num_params;
        state->update_pointers();

        return proto->get_chunk().get_code();
    }

    // --- JUMP COMPARE LOGIC (Refactored) ---

    #define IMPL_CMP_JUMP(OP_NAME, OP_ENUM, OPERATOR) \
    [[gnu::always_inline]] \
    inline static const uint8_t* impl_##OP_NAME(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
        auto [lhs, rhs, offset] = decode::args<u16, u16, i16>(ip); \
        Value& left = regs[lhs]; \
        Value& right = regs[rhs]; \
        bool condition = false; \
        if (left.holds_both<int_t>(right)) [[likely]] { condition = (left.as_int() OPERATOR right.as_int()); } \
        else if (left.holds_both<float_t>(right)) { condition = (left.as_float() OPERATOR right.as_float()); } \
        else [[unlikely]] { \
            Value res = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
            condition = meow::to_bool(res); \
        } \
        if (condition) return ip + offset; \
        return ip; \
    }

#define IMPL_CMP_JUMP_B(OP_NAME, OP_ENUM, OPERATOR) \
HOT_HANDLER impl_##OP_NAME##_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) { \
    /* 1. Decode siêu tốc */ \
    /* args() đọc [u8 LHS, u8 RHS, i16 Offset] = 4 bytes */ \
    /* ip tự động tăng 4 đơn vị */ \
    auto [lhs, rhs, offset] = decode::args<u8, u8, i16>(ip); \
    \
    Value& left = regs[lhs]; \
    Value& right = regs[rhs]; \
    \
    bool condition = false; \
    \
    /* 2. Fast Path Comparison */ \
    if (left.holds_both<int_t>(right)) [[likely]] { \
        condition = (left.as_int() OPERATOR right.as_int()); \
    } \
    else if (left.holds_both<float_t>(right)) { \
        condition = (left.as_float() OPERATOR right.as_float()); \
    } \
    else [[unlikely]] { \
        Value res = OperatorDispatcher::find(OpCode::OP_ENUM, left, right)(&state->heap, left, right); \
        condition = meow::to_bool(res); \
    } \
    \
    /* 3. Zero-Cost Branching */ \
    /* Vì ip đã tăng sẵn tới lệnh kế tiếp, ta chỉ cần cộng offset */ \
    if (condition) return ip + offset; \
    \
    return ip; \
}
    IMPL_CMP_JUMP(JUMP_IF_EQ, EQ, ==)
    IMPL_CMP_JUMP(JUMP_IF_NEQ, NEQ, !=)
    IMPL_CMP_JUMP(JUMP_IF_LT, LT, <)
    IMPL_CMP_JUMP(JUMP_IF_LE, LE, <=)
    IMPL_CMP_JUMP(JUMP_IF_GT, GT, >)
    IMPL_CMP_JUMP(JUMP_IF_GE, GE, >=)

    IMPL_CMP_JUMP_B(JUMP_IF_EQ, EQ, ==)
    IMPL_CMP_JUMP_B(JUMP_IF_NEQ, NEQ, !=)
    IMPL_CMP_JUMP_B(JUMP_IF_LT, LT, <)
    IMPL_CMP_JUMP_B(JUMP_IF_LE, LE, <=)
    IMPL_CMP_JUMP_B(JUMP_IF_GT, GT, >)
    IMPL_CMP_JUMP_B(JUMP_IF_GE, GE, >=)

    #undef IMPL_CMP_JUMP
    #undef IMPL_CMP_JUMP_B

} // namespace meow::handlers
#pragma once
#include "vm/handlers/utils.h"
#include <meow/core/objects.h>
#include <meow/machine.h>
#include <cstring>

namespace meow::handlers {

    struct JumpCondArgs { 
        uint16_t cond; 
        uint16_t offset; 
    } __attribute__((packed));

    struct JumpCondArgsB { 
        uint8_t cond; 
        uint16_t offset; 
    } __attribute__((packed));

    struct CallIC {
        void* check_tag;
        void* destination;
    } __attribute__((packed)); 

    [[gnu::always_inline]]
    inline static CallIC* get_call_ic(const uint8_t*& ip) {
        auto* ic = reinterpret_cast<CallIC*>(const_cast<uint8_t*>(ip));
        ip += sizeof(CallIC); 
        return ic;
    }

    [[gnu::always_inline]]
    inline static const uint8_t* push_call_frame(
        VMState* state, 
        function_t closure, 
        int argc, 
        Value* args_src,       // Nguồn arguments (đối với CALL thường)
        Value* receiver,       // 'this' (Nếu có - dành cho INVOKE)
        Value* ret_dest,       // Nơi lưu kết quả trả về
        const uint8_t* ret_ip, // Địa chỉ return (Sau lệnh call)
        const uint8_t* err_ip  // IP hiện tại để báo lỗi nếu Stack Overflow
    ) {
        proto_t proto = closure->get_proto();
        size_t num_params = proto->get_num_registers();

        // 1. Check Stack Overflow
        if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
            state->ctx.current_frame_->ip_ = err_ip;
            state->error("Stack Overflow!");
            return nullptr; // Báo hiệu lỗi
        }

        Value* new_base = state->ctx.stack_top_;
        size_t arg_offset = 0;

        // 2. Setup 'this' (Nếu là method call)
        if (receiver != nullptr && num_params > 0) {
            new_base[0] = *receiver;
            arg_offset = 1; // Các arg sau sẽ lùi lại 1 slot
        }

        // 3. Copy Arguments
        // INVOKE: args_src trỏ đến arg đầu tiên, receiver đã xử lý riêng
        // CALL: args_src trỏ đến arg đầu tiên, không có receiver
        size_t copy_count = (argc < (num_params - arg_offset)) ? argc : (num_params - arg_offset);
        
        for (size_t i = 0; i < copy_count; ++i) {
            new_base[arg_offset + i] = args_src[i];
        }

        // 4. Fill Null (Nếu thiếu args)
        size_t filled = arg_offset + argc;
        for (size_t i = filled; i < num_params; ++i) {
            new_base[i] = Value(null_t{});
        }

        // 5. Push Frame
        state->ctx.frame_ptr_++;
        *state->ctx.frame_ptr_ = CallFrame(
            closure,
            new_base,
            ret_dest, 
            ret_ip 
        );
        
        // 6. Update Pointers
        state->ctx.current_regs_ = new_base;
        state->ctx.stack_top_ += num_params;
        state->ctx.current_frame_ = state->ctx.frame_ptr_;
        state->update_pointers(); 

        return state->instruction_base; // Nhảy đến đầu hàm callee
    }

    [[gnu::always_inline]]
    inline static const uint8_t* impl_PANIC(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
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
        
        std::println("VM Panic: {}", state->get_error_message());
        return nullptr; 
    }

    [[gnu::always_inline]]
    inline static const uint8_t* impl_UNIMPL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        state->error("Opcode chưa được hỗ trợ (UNIMPL)");
        return impl_PANIC(ip, regs, constants, state);
    }

    [[gnu::always_inline]]
    inline static const uint8_t* impl_HALT(const uint8_t*, Value*, const Value*, VMState*) {
        return nullptr;
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        uint16_t offset = *reinterpret_cast<const uint16_t*>(ip);
        return state->instruction_base + offset;
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_TRUE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        const auto& args = *reinterpret_cast<const JumpCondArgs*>(ip);
        Value& cond = regs[args.cond];

        bool truthy;
        if (cond.is_bool()) [[likely]] truthy = cond.as_bool();
        else if (cond.is_int()) truthy = (cond.as_int() != 0);
        else [[unlikely]] truthy = meow::to_bool(cond);

        if (truthy) return state->instruction_base + args.offset;
        return ip + sizeof(JumpCondArgs);
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_FALSE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        const auto& args = *reinterpret_cast<const JumpCondArgs*>(ip);
        Value& cond = regs[args.cond];

        bool truthy;
        if (cond.is_bool()) [[likely]] truthy = cond.as_bool();
        else if (cond.is_int()) truthy = (cond.as_int() != 0);
        else [[unlikely]] truthy = meow::to_bool(cond);

        if (!truthy) return state->instruction_base + args.offset;
        return ip + sizeof(JumpCondArgs);
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_TRUE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        const auto& args = *reinterpret_cast<const JumpCondArgsB*>(ip);
        Value& cond = regs[args.cond];
        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
        if (truthy) return state->instruction_base + args.offset;
        return ip + sizeof(JumpCondArgsB);
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_FALSE_B(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        const auto& args = *reinterpret_cast<const JumpCondArgsB*>(ip);
        Value& cond = regs[args.cond];
        bool truthy = cond.is_bool() ? cond.as_bool() : (cond.is_int() ? (cond.as_int() != 0) : meow::to_bool(cond));
        if (!truthy) return state->instruction_base + args.offset;
        return ip + sizeof(JumpCondArgsB);
    }

    [[gnu::always_inline]]
    inline static const uint8_t* impl_RETURN(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        uint16_t ret_reg_idx = read_u16(ip);
        Value result = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : regs[ret_reg_idx];

        size_t base_idx = state->ctx.current_regs_ - state->ctx.stack_;
        meow::close_upvalues(state->ctx, base_idx);

        if (state->ctx.frame_ptr_ == state->ctx.call_stack_) [[unlikely]] {
            return nullptr; 
        }

        CallFrame* popped_frame = state->ctx.frame_ptr_;
        
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

        if (popped_frame->ret_dest_ != nullptr) {
            *popped_frame->ret_dest_ = result;
        }

        return popped_frame->ip_; 
    }

    template <bool IsVoid>
    [[gnu::always_inline]] 
    static inline const uint8_t* do_call(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        const uint8_t* start_ip = ip - 1; 

        uint16_t dst = 0;
        if constexpr (!IsVoid) dst = read_u16(ip);
        uint16_t fn_reg    = read_u16(ip);
        uint16_t arg_start = read_u16(ip);
        uint16_t argc      = read_u16(ip);

        CallIC* ic = get_call_ic(ip);
        Value& callee = regs[fn_reg];

        if (callee.is_function()) [[likely]] {
            function_t closure = callee.as_function();
            proto_t proto = closure->get_proto();

            if (ic->check_tag == proto) [[likely]] {
                size_t num_params = proto->get_num_registers();
                
                if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
                    state->ctx.current_frame_->ip_ = start_ip;
                    state->error("Stack Overflow!");
                    return impl_PANIC(ip, regs, constants, state);
                }

                Value* new_base = state->ctx.stack_top_;
                
                size_t copy_count = (argc < num_params) ? argc : num_params;
                for (size_t i = 0; i < copy_count; ++i) {
                    new_base[i] = regs[arg_start + i];
                }

                for (size_t i = copy_count; i < num_params; ++i) {
                    new_base[i] = Value(null_t{});
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
            
            ic->check_tag = proto;
        } 
        
        if (callee.is_native()) {
            native_t fn = callee.as_native();
            if (ic->check_tag != (void*)fn) {
                ic->check_tag = (void*)fn;
            }

            Value result = fn(&state->machine, argc, &regs[arg_start]);
            
            if (state->machine.has_error()) [[unlikely]] {
                state->error(std::string(state->machine.get_error_message()));
                state->machine.clear_error();
                return impl_PANIC(ip, regs, constants, state);
            }

            if constexpr (!IsVoid) {
                if (dst != 0xFFFF) regs[dst] = result;
            }
            return ip;
        }

        Value* ret_dest_ptr = nullptr;
        if constexpr (!IsVoid) {
            if (dst != 0xFFFF) ret_dest_ptr = &regs[dst];
        }

        instance_t self = nullptr;
        function_t closure = nullptr;
        bool is_init = false;

        if (callee.is_function()) {
            closure = callee.as_function();
        } 
        else if (callee.is_bound_method()) {
            bound_method_t bound = callee.as_bound_method();
            Value receiver = bound->get_receiver();
            Value method = bound->get_method();

            if (method.is_native()) {
                native_t fn = method.as_native();
                
                std::vector<Value> args;
                args.reserve(argc + 1);
                args.push_back(receiver);
                
                for (size_t i = 0; i < argc; ++i) {
                    args.push_back(regs[arg_start + i]);
                }

                Value result = fn(&state->machine, static_cast<int>(args.size()), args.data());
                
                if (state->machine.has_error()) {
                     return impl_PANIC(ip, regs, constants, state);
                }

                if constexpr (!IsVoid) regs[dst] = result;
                return ip;
            }
            else if (method.is_function()) {
                closure = method.as_function();
                if (receiver.is_instance()) self = receiver.as_instance();
            }
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
        else [[unlikely]] {
            state->ctx.current_frame_->ip_ = start_ip;
            state->error(std::format("Giá trị loại '{}' không thể gọi được (Not callable).", to_string(callee)));
            return impl_PANIC(ip, regs, constants, state);
        }

        proto_t proto = closure->get_proto();
        size_t num_params = proto->get_num_registers();

        if (!state->ctx.check_frame_overflow() || !state->ctx.check_overflow(num_params)) [[unlikely]] {
            state->ctx.current_frame_->ip_ = start_ip;
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

        size_t filled_count = arg_offset + argc;
        if (filled_count > num_params) filled_count = num_params;

        for (size_t i = filled_count; i < num_params; ++i) {
            new_base[i] = Value(null_t{});
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

    [[gnu::always_inline]] inline static const uint8_t* impl_CALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        return do_call<false>(ip, regs, constants, state);
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_CALL_VOID(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        return do_call<true>(ip, regs, constants, state);
    }

[[gnu::always_inline]] 
static const uint8_t* impl_TAIL_CALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    const uint8_t* start_ip = ip - 1;

    uint16_t dst = read_u16(ip); (void)dst;
    uint16_t fn_reg = read_u16(ip);
    uint16_t arg_start = read_u16(ip);
    uint16_t argc = read_u16(ip);
    
    ip += 16;

    Value& callee = regs[fn_reg];
    if (!callee.is_function()) [[unlikely]] {
        state->ctx.current_frame_->ip_ = start_ip;
        state->error("TAIL_CALL: Target không phải là Function.");
        return nullptr;
    }

    function_t closure = callee.as_function();
    proto_t proto = closure->get_proto();
    size_t num_params = proto->get_num_registers();

    size_t current_base_idx = regs - state->ctx.stack_;
    meow::close_upvalues(state->ctx, current_base_idx);

    size_t copy_count = (argc < num_params) ? argc : num_params;

    for (size_t i = 0; i < copy_count; ++i) {
        regs[i] = regs[arg_start + i];
    }

    for (size_t i = copy_count; i < num_params; ++i) {
        regs[i] = Value(null_t{});
    }

    CallFrame* current_frame = state->ctx.frame_ptr_;
    current_frame->function_ = closure;
    
    state->ctx.stack_top_ = regs + num_params;
    state->update_pointers();

    return proto->get_chunk().get_code();
}

} // namespace meow::handlers
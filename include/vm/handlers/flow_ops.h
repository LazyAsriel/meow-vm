#pragma once
#include "vm/handlers/utils.h"
#include "core/objects.h"

namespace meow::handlers {

    // --- PANIC (Xử lý lỗi runtime Meow) ---
    [[gnu::always_inline]]
    inline static const uint8_t* impl_PANIC(const uint8_t* ip, VMState* state) {
        (void)ip;
        
        // Tìm Handler gần nhất
        if (!state->ctx.exception_handlers_.empty()) {
            auto& handler = state->ctx.exception_handlers_.back();
            
            // Tính toán số lượng frame cần unwind dựa trên khoảng cách con trỏ
            long current_depth = state->ctx.frame_ptr_ - state->ctx.call_stack_;
            
            // Unwind stack
            while (current_depth > (long)handler.frame_depth_) {
                // Đóng upvalue của frame hiện tại trước khi hủy (dùng index tuyệt đối)
                size_t reg_idx = state->ctx.frame_ptr_->regs_base_ - state->ctx.stack_;
                meow::close_upvalues(state->ctx, reg_idx);
                
                state->ctx.frame_ptr_--; // Lùi frame pointer
                current_depth--;
            }
            
            // Khôi phục Stack Top & Context
            state->ctx.stack_top_ = state->ctx.stack_ + handler.stack_depth_;
            state->ctx.current_regs_ = state->ctx.frame_ptr_->regs_base_;
            
            // Sync legacy & Cache
            state->ctx.current_frame_ = state->ctx.frame_ptr_; 
            state->update_pointers(); // [QUAN TRỌNG] Cập nhật lại registers và instruction_base

            // Tính địa chỉ Catch (Dùng instruction_base mới khôi phục + offset đã lưu)
            const uint8_t* catch_ip = state->instruction_base + handler.catch_ip_;
            
            // Lưu lỗi vào register (nếu cần)
            if (handler.error_reg_ != static_cast<size_t>(-1)) {
                auto err_str = state->heap.new_string(state->get_error_message());
                state->reg(handler.error_reg_) = Value(err_str);
            }
            
            state->clear_error();
            state->ctx.exception_handlers_.pop_back();
            return catch_ip;
        } 
        
        // Không bắt được lỗi -> In ra console và Dừng VM
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
    inline static const uint8_t* impl_HALT(const uint8_t*, VMState*) {
        return nullptr; // Dừng dispatcher
    }

    // --- RETURN ---
    [[gnu::always_inline]]
    inline static const uint8_t* impl_RETURN(const uint8_t* ip, VMState* state) {
        uint16_t ret_reg_idx = read_u16(ip);
        
        // 1. Lấy kết quả (nếu có)
        Value result = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : state->reg(ret_reg_idx);

        // 2. Đóng Upvalues (tính index từ đáy stack)
        size_t base_idx = state->ctx.current_regs_ - state->ctx.stack_;
        meow::close_upvalues(state->ctx, base_idx);

        // 3. Check Main Frame (Nếu là frame cuối cùng -> Exit)
        if (state->ctx.frame_ptr_ == state->ctx.call_stack_) {
            return nullptr; 
        }

        // 4. Pop Frame
        CallFrame* popped_frame = state->ctx.frame_ptr_;
        state->ctx.frame_ptr_--;
        
        // 5. Khôi phục trạng thái Caller
        CallFrame* caller = state->ctx.frame_ptr_;
        
        // Stack Top mới = Chỗ bắt đầu của hàm vừa return (thu hồi toàn bộ bộ nhớ của hàm con)
        state->ctx.stack_top_ = popped_frame->regs_base_;
        state->ctx.current_regs_ = caller->regs_base_;
        
        // Sync
        state->ctx.current_frame_ = caller; 
        state->update_pointers(); // [QUAN TRỌNG] Restore instruction_base của caller

        // 6. Ghi kết quả vào biến hứng (nếu caller có hứng)
        if (popped_frame->ret_dest_ != nullptr) {
            *popped_frame->ret_dest_ = result;
        }

        // Nhảy về địa chỉ đã lưu
        return popped_frame->ip_;
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

        // Xác định nơi hứng kết quả ở frame hiện tại
        Value* ret_dest_ptr = nullptr;
        if constexpr (!IsVoid) {
            if (dst != 0xFFFF) ret_dest_ptr = &state->reg(dst);
        }

        Value& callee = state->reg(fn_reg);

        // 1. Native Call (Optimized)
        if (callee.is_native()) [[unlikely]] {
            native_t fn = callee.as_native();
            // Truyền trực tiếp địa chỉ mảng tham số (stack liền mạch)
            Value result = fn(&state->machine, argc, &state->reg(arg_start));
            
            if (ret_dest_ptr) *ret_dest_ptr = result;
            return ip;
        }

        // 2. Meow Call (Function / Method / Class)
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
            
            if (ret_dest_ptr) *ret_dest_ptr = Value(self);
            
            Value init_method = klass->get_method(state->heap.new_string("init"));
            if (init_method.is_function()) {
                closure = init_method.as_function();
                is_init = true;
            } else {
                return ip; // Không có init -> xong, trả về instance
            }
        } 
        else {
            state->error("Not callable");
            return impl_PANIC(ip, state);
        }

        proto_t proto = closure->get_proto();
        size_t num_params = proto->get_num_registers();

        // --- CHECK OVERFLOW (POINTER CHECK - Siêu nhanh) ---
        if (!state->ctx.check_frame_overflow()) [[unlikely]] {
            state->error("Stack Overflow (Call Stack)!");
            return impl_PANIC(ip, state);
        }
        if (!state->ctx.check_overflow(num_params)) [[unlikely]] {
            state->error("Stack Overflow (Registers)!");
            return impl_PANIC(ip, state);
        }

        // --- PREPARE NEW FRAME ---
        Value* new_base = state->ctx.stack_top_;
        
        // Copy tham số
        size_t arg_offset = 0;
        if (self != nullptr && num_params > 0) {
            new_base[0] = Value(self);
            arg_offset = 1;
        }

        // Copy args (Vector hóa tốt vì bộ nhớ liền nhau)
        for (size_t i = 0; i < argc; ++i) {
            if (arg_offset + i < num_params) {
                new_base[arg_offset + i] = state->reg(arg_start + i);
            }
        }

        state->ctx.frame_ptr_++; // Push CallStack
        
        // Constructor CallFrame
        *state->ctx.frame_ptr_ = CallFrame(
            closure,
            state->ctx.frame_ptr_[-1].module_, 
            new_base,                          
            is_init ? nullptr : ret_dest_ptr,  // Init luôn void
            ip                                 // Return Address
        );

        // Update Context & Cache
        state->ctx.current_regs_ = new_base;
        state->ctx.stack_top_ += num_params;
        state->ctx.current_frame_ = state->ctx.frame_ptr_;
        state->update_pointers(); // [QUAN TRỌNG] Cập nhật instruction_base mới

        // Jump to new code start (vừa được cache vào instruction_base)
        return state->instruction_base;
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_CALL(const uint8_t* ip, VMState* state) {
        return do_call<false>(ip, state);
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_CALL_VOID(const uint8_t* ip, VMState* state) {
        return do_call<true>(ip, state);
    }

    // --- JUMP (Đã tối ưu hóa với instruction_base) ---
    
    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP(const uint8_t* ip, VMState* state) {
        uint16_t offset = read_u16(ip);
        // Thay vì truy cập ctx->frame->func->proto->chunk->code, dùng cache:
        return state->instruction_base + offset;
    }

    [[gnu::always_inline]] inline static const uint8_t* impl_JUMP_IF_TRUE(const uint8_t* ip, VMState* state) {
        uint16_t cond_reg = read_u16(ip);
        uint16_t offset   = read_u16(ip);
        Value& cond = state->reg(cond_reg);

        bool truthy = false;
        if (cond.is_bool()) [[likely]] truthy = cond.as_bool();
        else if (cond.is_int()) truthy = (cond.as_int() != 0);
        else truthy = meow::to_bool(cond);

        if (truthy) {
            return state->instruction_base + offset;
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
            return state->instruction_base + offset;
        }
        return ip;
    }

} // namespace handlers
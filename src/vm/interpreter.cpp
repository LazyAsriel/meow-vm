// #include "vm/interpreter.h"
// #include "bytecode/op_codes.h"
// #include "core/value.h"
// #include "runtime/call_frame.h"
// #include "core/objects/function.h"

// #include "runtime/execution_context.h"
// #include "runtime/operator_dispatcher.h"

// namespace meow {

// // Helper access nhanh
// [[gnu::always_inline]]
// static inline Value& reg(VMState& state, uint16_t idx) noexcept {
//     return state.ctx.registers_[state.ctx.current_base_ + idx];
// }

// [[gnu::always_inline]]
// static inline const Value& constant(VMState& state, uint16_t idx) const noexcept {
//     return state.ctx.current_frame_->function_->get_proto()->get_chunk().get_constant(idx);
// }

// void Interpreter::op_LOAD_CONST(const uint8_t* ip, VMState state) noexcept {
//     uint16_t dst = read_u16(ip);
//     uint16_t idx = read_u16(ip);
    
//     reg(state, dst) = constant(state, idx);
    
//     [[clang::musttail]] return dispatch(ip, state);
// }

// void Interpreter::op_ADD(const uint8_t* ip, VMState state) noexcept {
//     uint16_t dst = read_u16(ip);
//     uint16_t r1  = read_u16(ip);
//     uint16_t r2  = read_u16(ip);

//     Value& left  = reg(state, r1);
//     Value& right = reg(state, r2);

//     // Gọi logic xử lý (giả sử OperatorDispatcher đã static và noexcept)
//     // Nếu OperatorDispatcher phát hiện lỗi (chia 0, sai kiểu), nó không throw nữa 
//     // mà trả về Value lỗi hoặc set state.error()
    
//     // Giả sử ta check kiểu thủ công ở đây (Fast path)
//     if (left.is_int() && right.is_int()) [[likely]] {
//         reg(state, dst) = Value(left.as_int() + right.as_int());
//     } 
//     else {
//         // Slow path / Error check
//         auto func = OperatorDispatcher::get_binary(OpCode::ADD, left, right);
//         if (func) {
//             reg(state, dst) = func(state.heap, left, right);
//         } else {
//             state.error("Invalid operands for ADD");
//             [[clang::musttail]] return op_PANIC(ip, state); // Chuyển sang xử lý lỗi
//         }
//     }

//     [[clang::musttail]] return dispatch(ip, state);
// }

// void Interpreter::op_RETURN(const uint8_t* ip, VMState state) noexcept {
//     uint16_t ret_reg = read_u16(ip);
//     Value result = (ret_reg == 0xFFFF) ? Value(null_t{}) : reg(state, ret_reg);

//     auto& stack = state.ctx->call_stack_;
    
//     // 1. Đóng upvalues
//     // close_upvalues(state.ctx, stack.back().start_reg_);

//     // 2. Pop frame
//     stack.pop_back();

//     if (stack.empty()) {
//         // Hết chương trình -> Halt
//         [[clang::musttail]] return op_HALT(ip, state);
//     }

//     // 3. Khôi phục state
//     state.ctx->current_frame_ = &stack.back();
//     state.ctx->current_base_  = state.ctx->current_frame_->start_reg_;
    
//     // 4. Lấy địa chỉ return từ frame cha
//     const uint8_t* return_ip = state.ctx->current_frame_->ip_;
    
//     // 5. Ghi kết quả trả về
//     size_t target_reg = stack.back().ret_reg_; // Cần lưu cái này ở frame trước khi call
//     if (target_reg != static_cast<size_t>(-1)) {
//         state.ctx->registers_[state.ctx->current_base_ + target_reg] = result;
//     }

//     [[clang::musttail]] return dispatch(return_ip, state);
// }

// void Interpreter::op_PANIC(const uint8_t* ip, VMState state) noexcept {
//     // Logic "Try-Catch" của VM nằm ở đây
    
//     // 1. Check xem có Exception Handler nào đang active không
//     if (!state.ctx->exception_handlers_.empty()) {
//         auto& handler = state.ctx->exception_handlers_.back();
        
//         // Unwind stack về đúng mức handler
//         state.ctx->call_stack_.resize(handler.frame_depth_ + 1);
//         state.ctx->current_frame_ = &state.ctx->call_stack_.back();
//         state.ctx->current_base_ = state.ctx->current_frame_->start_reg_;
        
//         // Nhảy tới IP của catch block
//         const uint8_t* catch_ip = state.ctx->current_frame_->function_->get_proto()->get_chunk().get_code() + handler.catch_ip_;
        
//         // Ghi lỗi vào register (nếu cần)
//         if (handler.error_reg_ != 0xFFFF) {
//              // state.ctx->registers_[...] = state.ctx->get_error_object();
//         }
        
//         state.ctx->clear_error();
//         state.ctx->exception_handlers_.pop_back();

//         // Resume execution
//         [[clang::musttail]] return dispatch(catch_ip, state);
//     }

//     // 2. Nếu không có handler -> Chết vinh quang (In lỗi và thoát)
//     printl("VM Panic: {}", state.ctx->get_error_message());
//     [[clang::musttail]] return op_HALT(ip, state);
// }

// void Interpreter::op_HALT(const uint8_t* ip, VMState state) noexcept {
//     return; // Kết thúc chuỗi gọi hàm, quay về run() -> main()
// }

// // --- Dispatch Table Setup ---
// const std::array<Interpreter::OpHandler, 256> Interpreter::dispatch_table = []{
//     std::array<Interpreter::OpHandler, 256> t;
//     t.fill(op_HALT); // Mặc định HALT cho an toàn
    
//     t[+OpCode::LOAD_CONST] = op_LOAD_CONST;
//     t[+OpCode::ADD]        = op_ADD;
//     t[+OpCode::RETURN]     = op_RETURN;
//     t[+OpCode::HALT]       = op_HALT;
//     // ... Fill more ...
//     return t;
// }();

// void Interpreter::run(VMState state) noexcept {
//     if (!state.ctx->current_frame_) return;
//     const uint8_t* ip = state.ctx->current_frame_->ip_;
//     dispatch(ip, state);
// }

// }
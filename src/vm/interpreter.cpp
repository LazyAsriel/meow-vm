// #include "vm/interpreter.h"
// #include "bytecode/op_codes.h"
// #include "core/value.h"
// #include "runtime/call_frame.h"
// #include "core/objects/function.h"
// #include "memory/memory_manager.h"
// #include "runtime/execution_context.h"
// #include "runtime/operator_dispatcher.h"
// #include "runtime/upvalue.h"
// #include "debug/print.h"
// #include <array>

// namespace meow {

// namespace {

//     // Forward declaration: Dùng con trỏ (VMState*) để đảm bảo 100% POD
//     [[gnu::always_inline]]
//     static void dispatch(const uint8_t* ip, VMState* state) noexcept;

//     [[gnu::always_inline]]
//     static uint16_t read_u16(const uint8_t*& ip) noexcept {
//         uint16_t val = static_cast<uint16_t>(ip[0]) | (static_cast<uint16_t>(ip[1]) << 8);
//         ip += 2;
//         return val;
//     }

//     // Panic handler
//     static void op_PANIC(const uint8_t* ip, VMState* state) noexcept;

//     static void op_UNIMPL(const uint8_t* ip, VMState* state) noexcept {
//         // Gán lỗi trực tiếp, tránh tạo biến tạm phức tạp trước khi nhảy
//         state->error("Opcode chưa được hỗ trợ trong Interpreter Mode");
//         [[clang::musttail]] return op_PANIC(ip, state);
//     }

//     // --- Opcode Handlers ---

//     static void op_LOAD_CONST(const uint8_t* ip, VMState* state) noexcept {
//         {
//             uint16_t dst = read_u16(ip);
//             uint16_t idx = read_u16(ip);
//             // Dùng con trỏ state->
//             state->reg(dst) = state->constant(idx);
//         }
//         [[clang::musttail]] return dispatch(ip, state);
//     }

//     static void op_ADD(const uint8_t* ip, VMState* state) noexcept {
//         {
//             uint16_t dst = read_u16(ip);
//             uint16_t r1  = read_u16(ip);
//             uint16_t r2  = read_u16(ip);

//             Value& left  = state->reg(r1);
//             Value& right = state->reg(r2);

//             if (left.is_int() && right.is_int()) [[likely]] {
//                 state->reg(dst) = Value(left.as_int() + right.as_int());
//             } 
//             else if (left.is_float() && right.is_float()) {
//                 state->reg(dst) = Value(left.as_float() + right.as_float());
//             }
//             else {
//                 auto func = OperatorDispatcher::find(OpCode::ADD, left, right);
//                 if (func) {
//                     state->reg(dst) = func(&state->heap, left, right);
//                 } else {
//                     state->error("Toán hạng không hợp lệ cho phép cộng (+)");
//                     goto handle_error;
//                 }
//             }
//         }
//         [[clang::musttail]] return dispatch(ip, state);

//     handle_error:
//         [[clang::musttail]] return op_PANIC(ip, state);
//     }

//     static void op_RETURN(const uint8_t* ip, VMState* state) noexcept {
//         const uint8_t* return_ip = nullptr;
//         {
//             uint16_t ret_reg_idx = read_u16(ip);
//             Value result = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : state->reg(ret_reg_idx);

//             auto& stack = state->ctx.call_stack_;
//             CallFrame* current = state->ctx.current_frame_;

//             meow::close_upvalues(state->ctx, current->start_reg_);

//             size_t target_reg = current->ret_reg_;
//             size_t old_base = current->start_reg_;

//             stack.pop_back();

//             if (stack.empty()) {
//                 return; 
//             }

//             state->ctx.current_frame_ = &stack.back();
//             state->ctx.current_base_  = state->ctx.current_frame_->start_reg_;
            
//             if (target_reg != static_cast<size_t>(-1)) {
//                 state->ctx.registers_[state->ctx.current_base_ + target_reg] = result;
//             }

//             state->ctx.registers_.resize(old_base);
//             return_ip = state->ctx.current_frame_->ip_;
//         }
//         [[clang::musttail]] return dispatch(return_ip, state);
//     }

//     static void op_HALT(const uint8_t* ip, VMState* state) noexcept {
//         return;
//     }

//     static void op_PANIC(const uint8_t* ip, VMState* state) noexcept {
//         const uint8_t* catch_ip = nullptr;
//         {
//             if (!state->ctx.exception_handlers_.empty()) {
//                 auto& handler = state->ctx.exception_handlers_.back();
                
//                 while (state->ctx.call_stack_.size() - 1 > handler.frame_depth_) {
//                     meow::close_upvalues(state->ctx, state->ctx.call_stack_.back().start_reg_);
//                     state->ctx.call_stack_.pop_back();
//                 }
                
//                 state->ctx.registers_.resize(handler.stack_depth_);
//                 state->ctx.current_frame_ = &state->ctx.call_stack_.back();
//                 state->ctx.current_base_ = state->ctx.current_frame_->start_reg_;
                
//                 const uint8_t* code_start = state->ctx.current_frame_->function_->get_proto()->get_chunk().get_code();
//                 catch_ip = code_start + handler.catch_ip_;
                
//                 if (handler.error_reg_ != static_cast<size_t>(-1)) {
//                     auto err_str = state->heap.new_string(state->get_error_message());
//                     state->reg(handler.error_reg_) = Value(err_str);
//                 }
                
//                 state->clear_error();
//                 state->ctx.exception_handlers_.pop_back();
//             } else {
//                 printl("VM Panic: {}", state->get_error_message());
//                 return;
//             }
//         }
//         [[clang::musttail]] return dispatch(catch_ip, state);
//     }

//     // Dùng con trỏ cho OpHandler
//     using OpHandler = void (*)(const uint8_t*, VMState*);

//     static const std::array<OpHandler, 256> dispatch_table = []{
//         std::array<OpHandler, 256> t;
//         t.fill(op_UNIMPL);
//         t[static_cast<size_t>(OpCode::LOAD_CONST)] = op_LOAD_CONST;
//         t[static_cast<size_t>(OpCode::ADD)]        = op_ADD;
//         t[static_cast<size_t>(OpCode::RETURN)]     = op_RETURN;
//         t[static_cast<size_t>(OpCode::HALT)]       = op_HALT;
//         return t;
//     }();

//     [[gnu::always_inline]]
//     static void dispatch(const uint8_t* ip, VMState* state) noexcept {
//         uint8_t opcode = *ip++;
//         [[clang::musttail]] return dispatch_table[opcode](ip, state);
//     }

// } // namespace anonymous

// void Interpreter::run(VMState state) noexcept {
//     if (!state.ctx.current_frame_) return;
//     const uint8_t* ip = state.ctx.current_frame_->ip_;
    
//     // Truyền địa chỉ của state (&state) -> tạo thành con trỏ
//     dispatch(ip, &state);
// }

// } // namespace meow
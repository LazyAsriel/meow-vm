Kết quả benchmark Computed Gotos và Musttail
Hiện tại đang dùng Musttail

lazycat@lazy-cat:~/Projects/meow-vm$ cd ..
lazycat@lazy-cat:~/Projects$ cd cpp/meow-vm
lazycat@lazy-cat:~/Projects/cpp/meow-vm$ cat src/vm/interpreter.cpp
#include "vm/interpreter.h"
#include <array>

// --- Include toàn bộ các bộ handler ---
#include "vm/handlers/data_ops.h"
#include "vm/handlers/math_ops.h"
#include "vm/handlers/flow_ops.h"
#include "vm/handlers/memory_ops.h"
#include "vm/handlers/oop_ops.h"
#include "vm/handlers/module_ops.h"
#include "vm/handlers/exception_ops.h"

namespace meow {
namespace {

    // --- Định nghĩa Types ---
    // OpHandler: Hàm void dùng cho bảng dispatch (để gọi musttail)
    using OpHandler = void (*)(const uint8_t*, VMState*);
    
    // OpImpl: Hàm trả về ip tiếp theo (logic thực tế của từng opcode)
    using OpImpl = const uint8_t* (*)(const uint8_t*, VMState*);

    // Bảng dispatch toàn cục (256 slots)
    static OpHandler dispatch_table[256];

    // --- Dispatcher Core (Trái tim của VM) ---
    // Sử dụng [[gnu::always_inline]] để đảm bảo compiler inline hàm này vào mọi nơi gọi nó
    [[gnu::noinline, gnu::hot]]
    static void dispatch(const uint8_t* ip, VMState* state) {
        // Đọc opcode và tăng ip
        uint8_t opcode = *ip++;
        
        // Nhảy đến handler tương ứng (Tail Call Optimization)
        // Đây là phép thuật giúp nó nhanh hơn Computed Goto!
        [[clang::musttail]] return dispatch_table[opcode](ip, state);
    }

    // --- Template Wrapper ---
    // Cầu nối giữa logic (OpImpl) và cơ chế nhảy (dispatch)
    template <OpImpl ImplFn>
    static void op_wrapper(const uint8_t* ip, VMState* state) {
        // 1. Thực thi logic của Opcode -> Lấy địa chỉ lệnh tiếp theo (next_ip)
        if (const uint8_t* next_ip = ImplFn(ip, state)) {
            // 2. Nếu next_ip hợp lệ -> Tiếp tục dispatch (Tail Call)
            [[clang::musttail]] return dispatch(next_ip, state);
        }
        // 3. Nếu next_ip == nullptr (ví dụ HALT hoặc PANIC) -> Dừng chuỗi gọi, return void.
    }

    // --- Khởi tạo bảng Dispatch ---
    struct TableInitializer {
        TableInitializer() {
            // 1. Khởi tạo mặc định: Gán tất cả về UNIMPL (Unimplemented)
            for (int i = 0; i < 256; ++i) {
                dispatch_table[i] = op_wrapper<handlers::impl_UNIMPL>;
            }

            // Helper lambda để đăng ký gọn hơn
            auto reg = [](OpCode op, OpHandler handler) {
                dispatch_table[static_cast<size_t>(op)] = handler;
            };

            // 2. Đăng ký từng nhóm OpCode

            // --- LOAD / DATA ---
            reg(OpCode::LOAD_CONST,   op_wrapper<handlers::impl_LOAD_CONST>);
            reg(OpCode::LOAD_NULL,    op_wrapper<handlers::impl_LOAD_NULL>);
            reg(OpCode::LOAD_TRUE,    op_wrapper<handlers::impl_LOAD_TRUE>);
            reg(OpCode::LOAD_FALSE,   op_wrapper<handlers::impl_LOAD_FALSE>);
            reg(OpCode::LOAD_INT,     op_wrapper<handlers::impl_LOAD_INT>);
            reg(OpCode::LOAD_FLOAT,   op_wrapper<handlers::impl_LOAD_FLOAT>);
            reg(OpCode::MOVE,         op_wrapper<handlers::impl_MOVE>);

            // --- MATH & LOGIC ---
            reg(OpCode::ADD,          op_wrapper<handlers::impl_ADD>);
            reg(OpCode::SUB,          op_wrapper<handlers::impl_SUB>);
            reg(OpCode::MUL,          op_wrapper<handlers::impl_MUL>);
            reg(OpCode::DIV,          op_wrapper<handlers::impl_DIV>);
            reg(OpCode::MOD,          op_wrapper<handlers::impl_MOD>);
            reg(OpCode::POW,          op_wrapper<handlers::impl_POW>);
            reg(OpCode::EQ,           op_wrapper<handlers::impl_EQ>);
            reg(OpCode::NEQ,          op_wrapper<handlers::impl_NEQ>);
            reg(OpCode::GT,           op_wrapper<handlers::impl_GT>);
            reg(OpCode::GE,           op_wrapper<handlers::impl_GE>);
            reg(OpCode::LT,           op_wrapper<handlers::impl_LT>);
            reg(OpCode::LE,           op_wrapper<handlers::impl_LE>);
            reg(OpCode::NEG,          op_wrapper<handlers::impl_NEG>);
            reg(OpCode::NOT,          op_wrapper<handlers::impl_NOT>);
            reg(OpCode::BIT_AND,      op_wrapper<handlers::impl_BIT_AND>);
            reg(OpCode::BIT_OR,       op_wrapper<handlers::impl_BIT_OR>);
            reg(OpCode::BIT_XOR,      op_wrapper<handlers::impl_BIT_XOR>);
            reg(OpCode::BIT_NOT,      op_wrapper<handlers::impl_BIT_NOT>);
            reg(OpCode::LSHIFT,       op_wrapper<handlers::impl_LSHIFT>);
            reg(OpCode::RSHIFT,       op_wrapper<handlers::impl_RSHIFT>);

            // --- MEMORY / VARIABLES ---
            reg(OpCode::GET_GLOBAL,   op_wrapper<handlers::impl_GET_GLOBAL>);
            reg(OpCode::SET_GLOBAL,   op_wrapper<handlers::impl_SET_GLOBAL>);
            reg(OpCode::GET_UPVALUE,  op_wrapper<handlers::impl_GET_UPVALUE>);
            reg(OpCode::SET_UPVALUE,  op_wrapper<handlers::impl_SET_UPVALUE>);
            reg(OpCode::CLOSURE,      op_wrapper<handlers::impl_CLOSURE>);
            reg(OpCode::CLOSE_UPVALUES, op_wrapper<handlers::impl_CLOSE_UPVALUES>);

            // --- FLOW CONTROL ---
            reg(OpCode::JUMP,         op_wrapper<handlers::impl_JUMP>);
            reg(OpCode::JUMP_IF_FALSE,op_wrapper<handlers::impl_JUMP_IF_FALSE>);
            reg(OpCode::JUMP_IF_TRUE, op_wrapper<handlers::impl_JUMP_IF_TRUE>);
            reg(OpCode::CALL,         op_wrapper<handlers::impl_CALL>);
            reg(OpCode::CALL_VOID,    op_wrapper<handlers::impl_CALL_VOID>);
            reg(OpCode::RETURN,       op_wrapper<handlers::impl_RETURN>);
            reg(OpCode::HALT,         op_wrapper<handlers::impl_HALT>);

            // --- DATA STRUCTURES ---
            reg(OpCode::NEW_ARRAY,    op_wrapper<handlers::impl_NEW_ARRAY>);
            reg(OpCode::NEW_HASH,     op_wrapper<handlers::impl_NEW_HASH>);
            reg(OpCode::GET_INDEX,    op_wrapper<handlers::impl_GET_INDEX>);
            reg(OpCode::SET_INDEX,    op_wrapper<handlers::impl_SET_INDEX>);
            reg(OpCode::GET_KEYS,     op_wrapper<handlers::impl_GET_KEYS>);
            reg(OpCode::GET_VALUES,   op_wrapper<handlers::impl_GET_VALUES>);

            // --- OOP ---
            reg(OpCode::NEW_CLASS,    op_wrapper<handlers::impl_NEW_CLASS>);
            reg(OpCode::NEW_INSTANCE, op_wrapper<handlers::impl_NEW_INSTANCE>);
            reg(OpCode::GET_PROP,     op_wrapper<handlers::impl_GET_PROP>);
            reg(OpCode::SET_PROP,     op_wrapper<handlers::impl_SET_PROP>);
            reg(OpCode::SET_METHOD,   op_wrapper<handlers::impl_SET_METHOD>);
            reg(OpCode::INHERIT,      op_wrapper<handlers::impl_INHERIT>);
            reg(OpCode::GET_SUPER,    op_wrapper<handlers::impl_GET_SUPER>);

            // --- EXCEPTION ---
            reg(OpCode::THROW,        op_wrapper<handlers::impl_THROW>);
            reg(OpCode::SETUP_TRY,    op_wrapper<handlers::impl_SETUP_TRY>);
            reg(OpCode::POP_TRY,      op_wrapper<handlers::impl_POP_TRY>);

            // --- MODULES ---
            reg(OpCode::IMPORT_MODULE,op_wrapper<handlers::impl_IMPORT_MODULE>);
            reg(OpCode::EXPORT,       op_wrapper<handlers::impl_EXPORT>);
            reg(OpCode::GET_EXPORT,   op_wrapper<handlers::impl_GET_EXPORT>);
            reg(OpCode::IMPORT_ALL,   op_wrapper<handlers::impl_IMPORT_ALL>);
        }
    };
    
    // Kích hoạt khởi tạo bảng dispatch ngay khi chương trình chạy
    static TableInitializer init_trigger;

} // namespace anonymous

// --- Public API ---
void Interpreter::run(VMState state) noexcept {
    // Kiểm tra xem có frame nào để chạy không
    if (!state.ctx.current_frame_) return;
    
    // Lấy IP bắt đầu từ frame hiện tại
    const uint8_t* ip = state.ctx.current_frame_->ip_;
    
    // Bắt đầu chuỗi dispatch (Jump vào loop)
    dispatch(ip, &state);
}

} // namespace meowlazycat@lazy-cat:~/Projects/cpp/meow-vm$ cat src/vm/machine.cpp
#include "vm/machine.h"
#include "common/pch.h"
#include "bytecode/op_codes.h"
#include "memory/mark_sweep_gc.h"
#include "memory/memory_manager.h"
#include "module/module_manager.h"
#include "runtime/execution_context.h"
#include "runtime/operator_dispatcher.h"
#include "runtime/upvalue.h"
#include "vm/macros.h"
#include "common/cast.h"
#include "debug/print.h"
#include "runtime/error_recovery.h"

#include "core/objects/array.h"
#include "core/objects/function.h"
#include "core/objects/hash_table.h"
#include "core/objects/module.h"
#include "core/objects/oop.h"

using namespace meow;

[[nodiscard]] constexpr size_t operator+(OpCode op) noexcept {
    return static_cast<size_t>(std::to_underlying(op));
}

#include "handlers/load.inl"
#include "handlers/memory.inl"
#include "handlers/data.inl"
#include "handlers/oop.inl"
#include "handlers/module.inl"
#include "handlers/exception.inl"

void Machine::run() {
    // printl("Starting Machine execution loop (Computed Goto)...");

#if !defined(__GNUC__) && !defined(__clang__)
    throw_vm_error("Computed goto dispatch loop requires GCC or Clang.");
#endif

    const uint8_t* ip = context_->current_frame_->ip_;

    // --- Bảng nhảy (Dispatch Table) ---
    static const void* dispatch_table[static_cast<size_t>(OpCode::TOTAL_OPCODES)] = {
        [+OpCode::LOAD_CONST]     = &&op_LOAD_CONST,
        [+OpCode::LOAD_NULL]      = &&op_LOAD_NULL,
        [+OpCode::LOAD_TRUE]      = &&op_LOAD_TRUE,
        [+OpCode::LOAD_FALSE]     = &&op_LOAD_FALSE,
        [+OpCode::LOAD_INT]       = &&op_LOAD_INT,
        [+OpCode::LOAD_FLOAT]     = &&op_LOAD_FLOAT,
        [+OpCode::MOVE]           = &&op_MOVE,
        [+OpCode::ADD]            = &&op_ADD,
        [+OpCode::SUB]            = &&op_SUB,
        [+OpCode::MUL]            = &&op_MUL,
        [+OpCode::DIV]            = &&op_DIV,
        [+OpCode::MOD]            = &&op_MOD,
        [+OpCode::POW]            = &&op_POW,
        [+OpCode::EQ]             = &&op_EQ,
        [+OpCode::NEQ]            = &&op_NEQ,
        [+OpCode::GT]             = &&op_GT,
        [+OpCode::GE]             = &&op_GE,
        [+OpCode::LT]             = &&op_LT,
        [+OpCode::LE]             = &&op_LE,
        [+OpCode::NEG]            = &&op_NEG,
        [+OpCode::NOT]            = &&op_NOT,
        [+OpCode::GET_GLOBAL]     = &&op_GET_GLOBAL,
        [+OpCode::SET_GLOBAL]     = &&op_SET_GLOBAL,
        [+OpCode::GET_UPVALUE]    = &&op_GET_UPVALUE,
        [+OpCode::SET_UPVALUE]    = &&op_SET_UPVALUE,
        [+OpCode::CLOSURE]        = &&op_CLOSURE,
        [+OpCode::CLOSE_UPVALUES] = &&op_CLOSE_UPVALUES,
        [+OpCode::JUMP]           = &&op_JUMP,
        [+OpCode::JUMP_IF_FALSE]  = &&op_JUMP_IF_FALSE,
        [+OpCode::JUMP_IF_TRUE]   = &&op_JUMP_IF_TRUE,
        [+OpCode::CALL]           = &&op_CALL,
        [+OpCode::CALL_VOID]      = &&op_CALL_VOID,
        [+OpCode::RETURN]         = &&op_RETURN,
        [+OpCode::HALT]           = &&op_HALT,
        [+OpCode::NEW_ARRAY]      = &&op_NEW_ARRAY,
        [+OpCode::NEW_HASH]       = &&op_NEW_HASH,
        [+OpCode::GET_INDEX]      = &&op_GET_INDEX,
        [+OpCode::SET_INDEX]      = &&op_SET_INDEX,
        [+OpCode::GET_KEYS]       = &&op_GET_KEYS,
        [+OpCode::GET_VALUES]     = &&op_GET_VALUES,
        [+OpCode::NEW_CLASS]      = &&op_NEW_CLASS,
        [+OpCode::NEW_INSTANCE]   = &&op_NEW_INSTANCE,
        [+OpCode::GET_PROP]       = &&op_GET_PROP,
        [+OpCode::SET_PROP]       = &&op_SET_PROP,
        [+OpCode::SET_METHOD]     = &&op_SET_METHOD,
        [+OpCode::INHERIT]        = &&op_INHERIT,
        [+OpCode::GET_SUPER]      = &&op_GET_SUPER,
        [+OpCode::BIT_AND]        = &&op_BIT_AND,
        [+OpCode::BIT_OR]         = &&op_BIT_OR,
        [+OpCode::BIT_XOR]        = &&op_BIT_XOR,
        [+OpCode::BIT_NOT]        = &&op_BIT_NOT,
        [+OpCode::LSHIFT]         = &&op_LSHIFT,
        [+OpCode::RSHIFT]         = &&op_RSHIFT,
        [+OpCode::THROW]          = &&op_THROW,
        [+OpCode::SETUP_TRY]      = &&op_SETUP_TRY,
        [+OpCode::POP_TRY]        = &&op_POP_TRY,
        [+OpCode::IMPORT_MODULE]  = &&op_IMPORT_MODULE,
        [+OpCode::EXPORT]         = &&op_EXPORT,
        [+OpCode::GET_EXPORT]     = &&op_GET_EXPORT,
        [+OpCode::IMPORT_ALL]     = &&op_IMPORT_ALL,
    };

dispatch_start:
    try {
        if (ip >= (CURRENT_CHUNK().get_code() + CURRENT_CHUNK().get_code_size())) {
            // printl("End of chunk reached, performing implicit return.");

            Value return_value = Value(null_t{});
            CallFrame popped_frame = *context_->current_frame_;
            size_t old_base = popped_frame.start_reg_;
            close_upvalues(context_.get(), popped_frame.start_reg_);
            if (popped_frame.function_->get_proto() == popped_frame.module_->get_main_proto()) {
                if (popped_frame.module_->is_executing()) {
                    popped_frame.module_->set_executed();
                }
            }
            context_->call_stack_.pop_back();

            if (context_->call_stack_.empty()) {
                // printl("Call stack empty. Halting.");
                return; // Thoát hàm run()
            }

            context_->current_frame_ = &context_->call_stack_.back();
            ip = context_->current_frame_->ip_;
            context_->current_base_ = context_->current_frame_->start_reg_;

            if (popped_frame.ret_reg_ != static_cast<size_t>(-1)) {
                context_->registers_[context_->current_base_ + popped_frame.ret_reg_] = return_value;
            }
            context_->registers_.resize(old_base);
            
            goto dispatch_start;
        }
        
        DISPATCH(); // Nhảy đến opcode đầu tiên

        op_LOAD_CONST: {
            op_load_const(ip);
            DISPATCH();
        }
        op_LOAD_NULL: {
            op_load_null(ip);
            DISPATCH();
        }
        op_LOAD_TRUE: {
            op_load_true(ip);
            DISPATCH();
        }
        op_LOAD_FALSE: {
            op_load_false(ip);
            DISPATCH();
        }
        op_MOVE: {
            op_move(ip);
            DISPATCH();
        }
        op_LOAD_INT: {
            op_load_int(ip);
            DISPATCH();
        }
        op_LOAD_FLOAT: {
            op_load_float(ip);
            DISPATCH();
        }

        op_ADD: {
            uint16_t dst = READ_U16();
            uint16_t r1  = READ_U16();
            uint16_t r2  = READ_U16();
            
            auto& left  = REGISTER(r1);
            auto& right = REGISTER(r2);
            if (left.is_int() && right.is_int()) [[likely]] {
                REGISTER(dst) = value_t(left.as_int() + right.as_int());
            } else if (left.is_float() && right.is_float()) {
                REGISTER(dst) = value_t(left.as_float() + right.as_float());
            } else {                
                auto func = OperatorDispatcher::find(OpCode::ADD, left, right);
                REGISTER(dst) = func(heap_.get(), left, right);
            }

            // printl("Final value in R0: {}", REGISTER(0).as_int());
            // printl("add r{}, r{}, r{}", meow::to_string(left), meow::to_string(right), meow::to_string(REGISTER(dst)));
            DISPATCH();
        }

        // BINARY_OP_HANDLER(ADD,     "ADD")
        BINARY_OP_HANDLER(SUB,     "SUB")
        BINARY_OP_HANDLER(MUL,     "MUL")
        BINARY_OP_HANDLER(DIV,     "DIV")
        BINARY_OP_HANDLER(MOD,     "MOD")
        BINARY_OP_HANDLER(POW,     "POW")
        BINARY_OP_HANDLER(EQ,      "EQ")
        BINARY_OP_HANDLER(NEQ,     "NEQ")
        BINARY_OP_HANDLER(GT,      "GT")
        BINARY_OP_HANDLER(GE,      "GE")
        BINARY_OP_HANDLER(LT,      "LT")
        BINARY_OP_HANDLER(LE,      "LE")
        BINARY_OP_HANDLER(BIT_AND, "BIT_AND")
        BINARY_OP_HANDLER(BIT_OR,  "BIT_OR")
        BINARY_OP_HANDLER(BIT_XOR, "BIT_XOR")
        BINARY_OP_HANDLER(LSHIFT,  "LSHIFT")
        BINARY_OP_HANDLER(RSHIFT,  "RSHIFT")

        UNARY_OP_HANDLER(NEG,     "NEG")
        UNARY_OP_HANDLER(NOT,     "NOT")
        UNARY_OP_HANDLER(BIT_NOT, "BIT_NOT")
        
        op_GET_GLOBAL: {
            op_get_global(ip);
            DISPATCH();
        }
        op_SET_GLOBAL: {
            op_set_global(ip);
            DISPATCH();
        }
        op_GET_UPVALUE: {
            op_get_upvalue(ip);
            DISPATCH();
        }
        op_SET_UPVALUE: {
            op_set_upvalue(ip);
            DISPATCH();
        }
        op_CLOSURE: {
            op_closure(ip);
            DISPATCH();
        }
        op_CLOSE_UPVALUES: {
            op_close_upvalues(ip);
            DISPATCH();
        }

        op_JUMP: {
            uint16_t target = READ_ADDRESS();
            ip = CURRENT_CHUNK().get_code() + target;
            DISPATCH();
        }
        op_JUMP_IF_FALSE: {
            uint16_t reg = READ_U16();
            uint16_t target = READ_ADDRESS();
            bool is_truthy_val = to_bool(REGISTER(reg));
            if (!is_truthy_val) {
                ip = CURRENT_CHUNK().get_code() + target;
            }
            DISPATCH();
        }
        op_JUMP_IF_TRUE: {
            uint16_t reg = READ_U16();
            uint16_t target = READ_ADDRESS();
            bool is_truthy_val = to_bool(REGISTER(reg));
            if (is_truthy_val) {
                ip = CURRENT_CHUNK().get_code() + target;
            }
            DISPATCH();
        }
        op_CALL:
        op_CALL_VOID: {
            uint16_t dst, fn_reg, arg_start, argc;
            size_t ret_reg;
            OpCode instruction = (ip[-1] == static_cast<uint8_t>(OpCode::CALL)) ? OpCode::CALL : OpCode::CALL_VOID;

            if (instruction == OpCode::CALL) {
                dst = READ_U16();
                fn_reg = READ_U16();
                arg_start = READ_U16();
                argc = READ_U16();
                ret_reg = (dst == 0xFFFF) ? static_cast<size_t>(-1) : static_cast<size_t>(dst);
            } else {
                fn_reg = READ_U16();
                arg_start = READ_U16();
                argc = READ_U16();
                ret_reg = static_cast<size_t>(-1);
            }
            Value& callee = REGISTER(fn_reg);

            if (callee.is_native()) {
                native_t fn = callee.as_native();
                
                Value* args_ptr = &REGISTER(arg_start); 
                
                Value result = fn(this, argc, args_ptr);
                
                if (instruction == OpCode::CALL && ret_reg != static_cast<size_t>(-1)) {
                    REGISTER(dst) = result;
                }
                DISPATCH();
            }

            instance_t self = nullptr;
            function_t closure_to_call = nullptr;
            bool is_constructor_call = false;

            if (callee.is_function()) {
                closure_to_call = callee.as_function();
            } else if (callee.is_bound_method()) {
                bound_method_t bound = callee.as_bound_method();
                self = bound->get_instance();
                closure_to_call = bound->get_function();
            } else if (callee.is_class()) {
                class_t k = callee.as_class();
                self = heap_->new_instance(k);
                is_constructor_call = true;
                if (ret_reg != static_cast<size_t>(-1)) {
                    REGISTER(dst) = Value(self);
                }
                Value init_val = k->get_method(heap_->new_string("init"));
                if (init_val.is_function()) {
                    closure_to_call = init_val.as_function();
                } else {
                    DISPATCH();
                }
            } else {
                throw_vm_error("CALL: Giá trị không thể gọi được.");
            }
            
            if (closure_to_call == nullptr) {
                DISPATCH();
            }

            proto_t proto = closure_to_call->get_proto();
            size_t new_base = context_->registers_.size();
            context_->registers_.resize(new_base + proto->get_num_registers());
            size_t arg_offset = 0;
            if (self != nullptr) {
                if (proto->get_num_registers() > 0) {
                    context_->registers_[new_base + 0] = Value(self);
                    arg_offset = 1;
                }
            }
            for (size_t i = 0; i < argc; ++i) {
                if ((arg_offset + i) < proto->get_num_registers()) {
                    context_->registers_[new_base + arg_offset + i] = REGISTER(arg_start + i);
                }
            }
            context_->current_frame_->ip_ = ip;
            module_t current_module = context_->current_frame_->module_;
            size_t frame_ret_reg = is_constructor_call ? static_cast<size_t>(-1) : ret_reg;
            context_->call_stack_.emplace_back(closure_to_call, current_module, new_base, frame_ret_reg, proto->get_chunk().get_code());
            context_->current_frame_ = &context_->call_stack_.back();
            ip = context_->current_frame_->ip_;
            context_->current_base_ = context_->current_frame_->start_reg_;
            
            DISPATCH();
        }
        op_RETURN: {
            uint16_t ret_reg_idx = READ_U16();
            Value return_value = (ret_reg_idx == 0xFFFF) ? Value(null_t{}) : REGISTER(ret_reg_idx);
            CallFrame popped_frame = *context_->current_frame_;
            size_t old_base = popped_frame.start_reg_;
            close_upvalues(context_.get(), popped_frame.start_reg_);
            context_->call_stack_.pop_back();

            if (context_->call_stack_.empty()) {
                // printl("Call stack empty. Halting.");
                if (!context_->registers_.empty()) context_->registers_[0] = return_value;
                return; // Thoát hàm run()
            }

            context_->current_frame_ = &context_->call_stack_.back();
            ip = context_->current_frame_->ip_;
            context_->current_base_ = context_->current_frame_->start_reg_;
            if (popped_frame.ret_reg_ != static_cast<size_t>(-1)) {
                context_->registers_[context_->current_base_ + popped_frame.ret_reg_] = return_value;
            }
            context_->registers_.resize(old_base);
            
            DISPATCH();
        }

        op_NEW_ARRAY: {
            op_new_array(ip);
            DISPATCH();
        }
        op_NEW_HASH: {
            op_new_hash(ip);
            DISPATCH();
        }
        op_GET_INDEX: {
            op_get_index(ip);
            DISPATCH();
        }
        op_SET_INDEX: {
            op_set_index(ip);
            DISPATCH();
        }
        op_GET_KEYS: {
            op_get_keys(ip);
            DISPATCH();
        }
        op_GET_VALUES: {
            op_get_values(ip);
            DISPATCH();
        }
        op_NEW_CLASS: {
            op_new_class(ip);
            DISPATCH();
        }
        op_NEW_INSTANCE: {
            op_new_instance(ip);
            DISPATCH();
        }
        op_GET_PROP: {
            op_get_prop(ip);
            DISPATCH();
        }
        op_SET_PROP: {
            op_set_prop(ip);
            DISPATCH();
        }
        op_SET_METHOD: {
            op_set_method(ip);
            DISPATCH();
        }
        op_INHERIT: {
            op_inherit(ip);
            DISPATCH();
        }
        op_GET_SUPER: {
            op_get_super(ip);
            DISPATCH();
        }

        op_THROW: {
            op_throw(ip);
            DISPATCH();
        }
        op_SETUP_TRY: {
            op_setup_try(ip);
            DISPATCH();
        }
        op_POP_TRY: {
            op_pop_try();
            DISPATCH();
        }
        op_IMPORT_MODULE: {
            uint16_t dst = READ_U16();
            uint16_t path_idx = READ_U16();
            string_t path = CONSTANT(path_idx).as_string();
            string_t importer_path = context_->current_frame_->module_->get_file_path();
            module_t mod = mod_manager_->load_module(path, importer_path);
            REGISTER(dst) = Value(mod);
            if (mod->is_executed() || mod->is_executing()) {
                DISPATCH();
            }
            if (!mod->is_has_main()) {
                mod->set_executed();
                DISPATCH();
            }

            mod->set_execution();
            proto_t main_proto = mod->get_main_proto();
            function_t main_closure = heap_->new_function(main_proto);
            context_->current_frame_->ip_ = ip;
            size_t new_base = context_->registers_.size();
            context_->registers_.resize(new_base + main_proto->get_num_registers());
            context_->call_stack_.emplace_back(main_closure, mod, new_base, static_cast<size_t>(-1), main_proto->get_chunk().get_code());
            context_->current_frame_ = &context_->call_stack_.back();
            ip = context_->current_frame_->ip_;
            context_->current_base_ = context_->current_frame_->start_reg_;
            
            DISPATCH();
        }

        op_EXPORT: {
            op_export(ip);
            DISPATCH();
        }
        op_GET_EXPORT: {
            op_get_export(ip);
            DISPATCH();
        }
        op_IMPORT_ALL: {
            op_import_all(ip);
            DISPATCH();
        }

        op_HALT: {
            // printl("halt");
            if (!context_->registers_.empty()) {
                if (REGISTER(0).is_int()) {
                    // printl("Final value in R0: {}", REGISTER(0).as_int());
                }
            }
            return;
        }
    } catch (const VMError& e) {
        // Gọi hàm xử lý riêng
        if (recover_from_error(e, context_.get(), heap_.get())) {
            // Nếu cứu được, cập nhật lại IP cục bộ từ frame và nhảy tiếp
            ip = context_->current_frame_->ip_;
            goto dispatch_start;
        } else {
            // Nếu không cứu được, thoát
            return;
        }
    }

    std::unreachable();
}lazycat@lazy-cat:~/Projects/cpp/meow-vm$ cat include/vm/handlers/math_ops.h
#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"

namespace meow::handlers {

// Macro tối ưu: Gọi thẳng, không kiểm tra nullptr (vì OperatorDispatcher đảm bảo luôn trả về function)
#define BINARY_OP_IMPL(NAME, OP_CODE) \
    HOT_HANDLER impl_##NAME(const uint8_t* ip, VMState* state) { \
        uint16_t dst = read_u16(ip); \
        uint16_t r1  = read_u16(ip); \
        uint16_t r2  = read_u16(ip); \
        /* Truy cập trực tiếp thanh ghi, compiler sẽ tối ưu pointer arithmetic */ \
        auto& left  = state->reg(r1); \
        auto& right = state->reg(r2); \
        /* GỌI THẲNG: Không if, không check. Tin tưởng tuyệt đối vào Dispatcher */ \
        state->reg(dst) = OperatorDispatcher::find(OpCode::OP_CODE, left, right)(&state->heap, left, right); \
        return ip; \
    }

// --- ADD (Siêu tối ưu) ---
// Giữ lại fast-path cho Int/Int và Float/Float vì nó nhanh hơn function call
HOT_HANDLER impl_ADD(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t r1 = read_u16(ip);
    uint16_t r2 = read_u16(ip);
    
    // Prefetch dữ liệu nếu cần (tùy architecture, nhưng để compiler lo)
    auto& left = state->reg(r1);
    auto& right = state->reg(r2);

    // Fast Path: Int + Int (Chiếm 90% trường hợp loop)
    if (left.is_int() && right.is_int()) [[likely]] {
        state->reg(dst) = Value(left.as_int() + right.as_int());
    } 
    // Fast Path: Float + Float
    else if (left.is_float() && right.is_float()) {
        state->reg(dst) = Value(left.as_float() + right.as_float());
    } 
    else {
        // Slow Path: Dispatch trực tiếp
        // Lưu ý: Nếu kiểu không hợp lệ, hàm trap của Dispatcher sẽ chạy.
        // Cần đảm bảo hàm trap đó ném VMError nếu muốn dừng VM, hoặc trả null nếu muốn lờ đi.
        state->reg(dst) = OperatorDispatcher::find(OpCode::ADD, left, right)(&state->heap, left, right);
    }
    return ip;
}

// --- LT (So sánh nhỏ hơn) ---
HOT_HANDLER impl_LT(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t r1 = read_u16(ip);
    uint16_t r2 = read_u16(ip);
    auto& left = state->reg(r1);
    auto& right = state->reg(r2);

    if (left.is_int() && right.is_int()) [[likely]] {
        state->reg(dst) = Value(left.as_int() < right.as_int());
    } else {
        // Dispatch trực tiếp
        state->reg(dst) = OperatorDispatcher::find(OpCode::LT, left, right)(&state->heap, left, right);
    }
    return ip;
}

// Sinh code cho các toán tử còn lại
BINARY_OP_IMPL(SUB, SUB)
BINARY_OP_IMPL(MUL, MUL)
BINARY_OP_IMPL(DIV, DIV)
BINARY_OP_IMPL(MOD, MOD)
BINARY_OP_IMPL(POW, POW)

BINARY_OP_IMPL(EQ, EQ)
BINARY_OP_IMPL(NEQ, NEQ)
BINARY_OP_IMPL(GT, GT)
BINARY_OP_IMPL(GE, GE)
BINARY_OP_IMPL(LE, LE) // LT đã viết tay ở trên

BINARY_OP_IMPL(BIT_AND, BIT_AND)
BINARY_OP_IMPL(BIT_OR, BIT_OR)
BINARY_OP_IMPL(BIT_XOR, BIT_XOR)
BINARY_OP_IMPL(LSHIFT, LSHIFT)
BINARY_OP_IMPL(RSHIFT, RSHIFT)

// --- Unary Ops ---
// NEG và BIT_NOT ít khi có fastpath đơn giản hơn dispatch table, nên gọi thẳng luôn cho gọn
HOT_HANDLER impl_NEG(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src = read_u16(ip);
    auto& val = state->reg(src);
    state->reg(dst) = OperatorDispatcher::find(OpCode::NEG, val)(&state->heap, val);
    return ip;
}

HOT_HANDLER impl_BIT_NOT(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src = read_u16(ip);
    auto& val = state->reg(src);
    state->reg(dst) = OperatorDispatcher::find(OpCode::BIT_NOT, val)(&state->heap, val);
    return ip;
}

HOT_HANDLER impl_NOT(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t src = read_u16(ip);
    // NOT logic: không cần dispatcher vì mọi value đều to_bool được
    state->reg(dst) = Value(!to_bool(state->reg(src)));
    return ip;
}

#undef BINARY_OP_IMPL
#undef HOT_HANDLER // Định nghĩa trong utils hoặc flow_ops nếu cần
}lazycat@lazy-cat:~/Projects/cpp/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

[register] Đang đăng kí object: 0x6155b3076a00
[register] Đang đăng kí object: 0x6155b3076b60
[register] Đang đăng kí object: 0x6155b3076be0
[register] Đang đăng kí object: 0x6155b3076c20
[register] Đang đăng kí object: 0x6155b3076ce0
Running Machine (Computed Goto)... 306.622 ms
Running Interpreter (Musttail)... 280.364 ms

📊 RESULT:
Machine (Computed Goto): 306.622 ms
Interpreter (Musttail):  280.364 ms
Ratio: Interpreter is 0.91x time of Machine.
🏆 MUSTTAIL WINS! (Ảo thật đấy!)
[destroy] Đang xử lí các object khi hủy GC
lazycat@lazy-cat:~/Projects/cpp/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

[register] Đang đăng kí object: 0x55aad8047a00
[register] Đang đăng kí object: 0x55aad8047b60
[register] Đang đăng kí object: 0x55aad8047be0
[register] Đang đăng kí object: 0x55aad8047c20
[register] Đang đăng kí object: 0x55aad8047ce0
Running Machine (Computed Goto)... 301.372 ms
Running Interpreter (Musttail)... 270.798 ms

📊 RESULT:
Machine (Computed Goto): 301.372 ms
Interpreter (Musttail):  270.798 ms
Ratio: Interpreter is 0.90x time of Machine.
🏆 MUSTTAIL WINS! (Ảo thật đấy!)
[destroy] Đang xử lí các object khi hủy GC
lazycat@lazy-cat:~/Projects/cpp/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

[register] Đang đăng kí object: 0x560ce7aa9a00
[register] Đang đăng kí object: 0x560ce7aa9b60
[register] Đang đăng kí object: 0x560ce7aa9be0
[register] Đang đăng kí object: 0x560ce7aa9c20
[register] Đang đăng kí object: 0x560ce7aa9ce0
Running Machine (Computed Goto)... 308.011 ms
Running Interpreter (Musttail)... 279.747 ms

📊 RESULT:
Machine (Computed Goto): 308.011 ms
Interpreter (Musttail):  279.747 ms
Ratio: Interpreter is 0.91x time of Machine.
🏆 MUSTTAIL WINS! (Ảo thật đấy!)
[destroy] Đang xử lí các object khi hủy GC
lazycat@lazy-cat:~/Projects/cpp/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

[register] Đang đăng kí object: 0x5659e3f7da00
[register] Đang đăng kí object: 0x5659e3f7db60
[register] Đang đăng kí object: 0x5659e3f7dbe0
[register] Đang đăng kí object: 0x5659e3f7dc20
[register] Đang đăng kí object: 0x5659e3f7dce0
Running Machine (Computed Goto)... 299.416 ms
Running Interpreter (Musttail)... 272.679 ms

📊 RESULT:
Machine (Computed Goto): 299.416 ms
Interpreter (Musttail):  272.679 ms
Ratio: Interpreter is 0.91x time of Machine.
🏆 MUSTTAIL WINS! (Ảo thật đấy!)
[destroy] Đang xử lí các object khi hủy GC
lazycat@lazy-cat:~/Projects/cpp/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

[register] Đang đăng kí object: 0x59f5a924ea00
[register] Đang đăng kí object: 0x59f5a924eb60
[register] Đang đăng kí object: 0x59f5a924ebe0
[register] Đang đăng kí object: 0x59f5a924ec20
[register] Đang đăng kí object: 0x59f5a924ece0
Running Machine (Computed Goto)... 302.377 ms
Running Interpreter (Musttail)... 269.418 ms

📊 RESULT:
Machine (Computed Goto): 302.377 ms
Interpreter (Musttail):  269.418 ms
Ratio: Interpreter is 0.89x time of Machine.
🏆 MUSTTAIL WINS! (Ảo thật đấy!)
[destroy] Đang xử lí các object khi hủy GC
lazycat@lazy-cat:~/Projects/cpp/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

[register] Đang đăng kí object: 0x5fcf40940a00
[register] Đang đăng kí object: 0x5fcf40940b60
[register] Đang đăng kí object: 0x5fcf40940be0
[register] Đang đăng kí object: 0x5fcf40940c20
[register] Đang đăng kí object: 0x5fcf40940ce0
Running Machine (Computed Goto)... 300.609 ms
Running Interpreter (Musttail)... 270.038 ms

📊 RESULT:
Machine (Computed Goto): 300.609 ms
Interpreter (Musttail):  270.038 ms
Ratio: Interpreter is 0.90x time of Machine.
🏆 MUSTTAIL WINS! (Ảo thật đấy!)
[destroy] Đang xử lí các object khi hủy GC
lazycat@lazy-cat:~/Projects/cpp/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

[register] Đang đăng kí object: 0x587593132a00
[register] Đang đăng kí object: 0x587593132b60
[register] Đang đăng kí object: 0x587593132be0
[register] Đang đăng kí object: 0x587593132c20
[register] Đang đăng kí object: 0x587593132ce0
Running Machine (Computed Goto)... 299.636 ms
Running Interpreter (Musttail)... 268.852 ms

📊 RESULT:
Machine (Computed Goto): 299.636 ms
Interpreter (Musttail):  268.852 ms
Ratio: Interpreter is 0.90x time of Machine.
🏆 MUSTTAIL WINS! (Ảo thật đấy!)
[destroy] Đang xử lí các object khi hủy GC
lazycat@lazy-cat:~/Projects/cpp/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

[register] Đang đăng kí object: 0x5771fa2a7a00
[register] Đang đăng kí object: 0x5771fa2a7b60
[register] Đang đăng kí object: 0x5771fa2a7be0
[register] Đang đăng kí object: 0x5771fa2a7c20
[register] Đang đăng kí object: 0x5771fa2a7ce0
Running Machine (Computed Goto)... 299.816 ms
Running Interpreter (Musttail)... 274.71 ms

📊 RESULT:
Machine (Computed Goto): 299.816 ms
Interpreter (Musttail):  274.71 ms
Ratio: Interpreter is 0.92x time of Machine.
🏆 MUSTTAIL WINS! (Ảo thật đấy!)
[destroy] Đang xử lí các object khi hủy GC
lazycat@lazy-cat:~/Projects/cpp/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

[register] Đang đăng kí object: 0x5d8e51e0ba00
[register] Đang đăng kí object: 0x5d8e51e0bb60
[register] Đang đăng kí object: 0x5d8e51e0bbe0
[register] Đang đăng kí object: 0x5d8e51e0bc20
[register] Đang đăng kí object: 0x5d8e51e0bce0
Running Machine (Computed Goto)... 298.309 ms
Running Interpreter (Musttail)... 269.87 ms

📊 RESULT:
Machine (Computed Goto): 298.309 ms
Interpreter (Musttail):  269.87 ms
Ratio: Interpreter is 0.90x time of Machine.
🏆 MUSTTAIL WINS! (Ảo thật đấy!)
[destroy] Đang xử lí các object khi hủy GC
lazycat@lazy-cat:~/Projects/cpp/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

[register] Đang đăng kí object: 0x64b44e8d7a00
[register] Đang đăng kí object: 0x64b44e8d7b60
[register] Đang đăng kí object: 0x64b44e8d7be0
[register] Đang đăng kí object: 0x64b44e8d7c20
[register] Đang đăng kí object: 0x64b44e8d7ce0
Running Machine (Computed Goto)... 299.712 ms
Running Interpreter (Musttail)... 268.546 ms

📊 RESULT:
Machine (Computed Goto): 299.712 ms
Interpreter (Musttail):  268.546 ms
Ratio: Interpreter is 0.90x time of Machine.
🏆 MUSTTAIL WINS! (Ảo thật đấy!)
[destroy] Đang xử lí các object khi hủy GC
lazycat@lazy-cat:~/Projects/cpp/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

[register] Đang đăng kí object: 0x570572734a00
[register] Đang đăng kí object: 0x570572734b60
[register] Đang đăng kí object: 0x570572734be0
[register] Đang đăng kí object: 0x570572734c20
[register] Đang đăng kí object: 0x570572734ce0
Running Machine (Computed Goto)... 300.746 ms
Running Interpreter (Musttail)... 282.77 ms

📊 RESULT:
Machine (Computed Goto): 300.746 ms
Interpreter (Musttail):  282.77 ms
Ratio: Interpreter is 0.94x time of Machine.
🏆 MUSTTAIL WINS! (Ảo thật đấy!)
[destroy] Đang xử lí các object khi hủy GC
lazycat@lazy-cat:~/Projects/cpp/meow-vm$ cd ~/Projects/meow-vm
lazycat@lazy-cat:~/Projects/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

Running Machine (Computed Goto)... 161.503 ms
Running Interpreter (Musttail)... 157.713 ms

📊 RESULT:
Machine (Computed Goto): 161.503 ms
Interpreter (Musttail):  157.713 ms
Ratio: Interpreter is 0.98x time of Machine.
lazycat@lazy-cat:~/Projects/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

Running Machine (Computed Goto)... 158.711 ms
Running Interpreter (Musttail)... 157.703 ms

📊 RESULT:
Machine (Computed Goto): 158.711 ms
Interpreter (Musttail):  157.703 ms
Ratio: Interpreter is 0.99x time of Machine.
lazycat@lazy-cat:~/Projects/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

Running Machine (Computed Goto)... 159.975 ms
Running Interpreter (Musttail)... 157.935 ms

📊 RESULT:
Machine (Computed Goto): 159.975 ms
Interpreter (Musttail):  157.935 ms
Ratio: Interpreter is 0.99x time of Machine.
lazycat@lazy-cat:~/Projects/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

Running Machine (Computed Goto)... 163.271 ms
Running Interpreter (Musttail)... 159.154 ms

📊 RESULT:
Machine (Computed Goto): 163.271 ms
Interpreter (Musttail):  159.154 ms
Ratio: Interpreter is 0.97x time of Machine.
lazycat@lazy-cat:~/Projects/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

Running Machine (Computed Goto)... 161.727 ms
Running Interpreter (Musttail)... 160.758 ms

📊 RESULT:
Machine (Computed Goto): 161.727 ms
Interpreter (Musttail):  160.758 ms
Ratio: Interpreter is 0.99x time of Machine.
lazycat@lazy-cat:~/Projects/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

Running Machine (Computed Goto)... 162.24 ms
Running Interpreter (Musttail)... 157.736 ms

📊 RESULT:
Machine (Computed Goto): 162.24 ms
Interpreter (Musttail):  157.736 ms
Ratio: Interpreter is 0.97x time of Machine.
lazycat@lazy-cat:~/Projects/meow-vm$ ./build/release/bin/meow_dispatch_bench
🐱 MEOW VM BENCHMARK: DISPATCH WAR 🐱
Scenario: Loop 10000000 iterations (ADD + LT + JUMP)

Running Machine (Computed Goto)... 160.315 ms
Running Interpreter (Musttail)... 157.663 ms

📊 RESULT:
Machine (Computed Goto): 160.315 ms
Interpreter (Musttail):  157.663 ms
Ratio: Interpreter is 0.98x time of Machine.
lazycat@lazy-cat:~/Projects/meow-vm$ 


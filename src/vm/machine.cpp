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
}
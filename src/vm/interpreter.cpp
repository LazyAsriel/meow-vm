#include "vm/interpreter.h"
#include "vm/handlers/data_ops.h"
#include "vm/handlers/math_ops.h"
#include "vm/handlers/flow_ops.h"
#include "vm/handlers/memory_ops.h"
#include "vm/handlers/oop_ops.h"
#include "vm/handlers/module_ops.h"
#include "vm/handlers/exception_ops.h"

namespace meow {

namespace {
    using OpHandler = void (*)(const uint8_t*, Value*, const Value*, VMState*);
    using OpImpl    = const uint8_t* (*)(const uint8_t*, Value*, const Value*, VMState*);

    static OpHandler dispatch_table[256];

    [[gnu::always_inline, gnu::hot]]
    static void dispatch(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        uint8_t opcode = *ip++;
        [[clang::musttail]] return dispatch_table[opcode](ip, regs, constants, state);
    }

    template <OpCode Op>
    constexpr bool IsFrameChange = false;

    template <> constexpr bool IsFrameChange<OpCode::CALL>          = true;
    template <> constexpr bool IsFrameChange<OpCode::CALL_VOID>     = true;
    template <> constexpr bool IsFrameChange<OpCode::TAIL_CALL>     = true;
    template <> constexpr bool IsFrameChange<OpCode::RETURN>        = true;
    template <> constexpr bool IsFrameChange<OpCode::IMPORT_MODULE> = true;
    template <> constexpr bool IsFrameChange<OpCode::THROW>         = true; 
    
    template <OpCode Op, OpImpl ImplFn>
    static void op_wrapper(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
        const uint8_t* next_ip = ImplFn(ip, regs, constants, state);
        if (next_ip) [[likely]] {
            if constexpr (IsFrameChange<Op>) {
                regs = state->registers;
                constants = state->constants;
            }
            [[clang::musttail]] return dispatch(next_ip, regs, constants, state);
        }
    }

    struct TableInitializer {
        TableInitializer() {
            for (int i = 0; i < 256; ++i) {
                dispatch_table[i] = op_wrapper<OpCode::HALT, handlers::impl_UNIMPL>;
            }

            #define reg(NAME) dispatch_table[static_cast<size_t>(OpCode::NAME)] = op_wrapper<OpCode::NAME, handlers::impl_##NAME>
            
            // Load / Move
            reg(LOAD_CONST); reg(LOAD_NULL); reg(LOAD_TRUE); reg(LOAD_FALSE);
            reg(LOAD_INT); reg(LOAD_FLOAT); reg(MOVE);

            reg(INC); reg(DEC);

            // Math
            reg(ADD); reg(SUB); reg(MUL); reg(DIV); reg(MOD); reg(POW);
            reg(EQ); reg(NEQ); reg(GT); reg(GE); reg(LT); reg(LE);
            reg(NEG); reg(NOT);
            reg(BIT_AND); reg(BIT_OR); reg(BIT_XOR); reg(BIT_NOT);
            reg(LSHIFT); reg(RSHIFT);

            // Variables / Memory
            reg(GET_GLOBAL); reg(SET_GLOBAL);
            reg(GET_UPVALUE); reg(SET_UPVALUE);
            reg(CLOSURE); reg(CLOSE_UPVALUES);

            // Control Flow
            reg(JUMP); reg(JUMP_IF_FALSE); reg(JUMP_IF_TRUE);
            reg(CALL); reg(CALL_VOID); reg(RETURN); reg(HALT);

            // Data Structures
            reg(NEW_ARRAY); reg(NEW_HASH);
            reg(GET_INDEX); reg(SET_INDEX);
            reg(GET_KEYS); reg(GET_VALUES);

            // OOP
            reg(NEW_CLASS); reg(NEW_INSTANCE);
            reg(GET_PROP); reg(SET_PROP); reg(SET_METHOD);
            reg(INHERIT); reg(GET_SUPER);

            // Exception
            reg(THROW); reg(SETUP_TRY); reg(POP_TRY);

            // Modules
            reg(IMPORT_MODULE); reg(EXPORT); reg(GET_EXPORT); reg(IMPORT_ALL);

            reg(TAIL_CALL);

            reg(ADD_B); reg(SUB_B); reg(MUL_B); reg(DIV_B); reg(MOD_B);
            reg(EQ_B); reg(NEQ_B); reg(GT_B); reg(GE_B); reg(LT_B); reg(LE_B);
            
            reg(JUMP_IF_TRUE_B); 
            reg(JUMP_IF_FALSE_B);
            
            reg(JUMP_IF_TRUE_B); 
            reg(JUMP_IF_FALSE_B);

            #undef reg
        }
    };
    
    static TableInitializer init_trigger;

} // namespace anonymous

void Interpreter::run(VMState state) noexcept {
    if (!state.ctx.current_frame_) return;
    
    state.update_pointers();
    
    Value* regs = state.registers;
    const Value* constants = state.constants;
    const uint8_t* ip = state.ctx.current_frame_->ip_;
    
    dispatch(ip, regs, constants, &state);
}

} // namespace meow
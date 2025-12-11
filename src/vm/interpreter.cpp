#include "vm/interpreter.h"
#include <array>

// --- Include toàn bộ các bộ handler ---
// Lưu ý: Các file header này CẦN được cập nhật chữ ký hàm 
// thành (const uint8_t*, Value*, Value*, VMState*) để khớp.
#include "vm/handlers/data_ops.h"
#include "vm/handlers/math_ops.h"
#include "vm/handlers/flow_ops.h"
#include "vm/handlers/memory_ops.h"
#include "vm/handlers/oop_ops.h"
#include "vm/handlers/module_ops.h"
#include "vm/handlers/exception_ops.h"

namespace meow {
namespace {

    // --- Định nghĩa Types (Argument Threading) ---
    // Truyền trực tiếp regs và constants để tối ưu hóa Register Allocation
    using OpHandler = void (*)(const uint8_t*, Value*, Value*, VMState*);
    using OpImpl    = const uint8_t* (*)(const uint8_t*, Value*, Value*, VMState*);

    // Bảng dispatch toàn cục
    static OpHandler dispatch_table[256];

    // --- Dispatcher Core ---
    // [[gnu::always_inline]] đảm bảo logic này được nhúng thẳng vào đuôi của handler trước đó
    [[gnu::always_inline, gnu::hot]]
    static void dispatch(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
        uint8_t opcode = *ip++;
        // Tail Call: Nhảy mà không tăng stack depth, giữ nguyên thanh ghi cho regs/constants
        [[clang::musttail]] return dispatch_table[opcode](ip, regs, constants, state);
    }

    // --- Cấu hình Reload (Optimization) ---
    // Mặc định: Không reload regs/constants (giữ nguyên trong thanh ghi CPU)
    template <OpCode Op>
    constexpr bool IsFrameChange = false;

    // Các Opcode thay đổi CallFrame bắt buộc phải reload con trỏ
    template <> constexpr bool IsFrameChange<OpCode::CALL>          = true;
    template <> constexpr bool IsFrameChange<OpCode::CALL_VOID>     = true;
    template <> constexpr bool IsFrameChange<OpCode::RETURN>        = true;
    template <> constexpr bool IsFrameChange<OpCode::IMPORT_MODULE> = true;
    // THROW có thể unwind stack -> thay đổi frame -> cần reload
    template <> constexpr bool IsFrameChange<OpCode::THROW>         = true; 
    
    // --- Template Wrapper ---
    // Tự động sinh logic reload chỉ cho những opcode cần thiết
    template <OpCode Op, OpImpl ImplFn>
    static void op_wrapper(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
        // 1. Thực thi logic nghiệp vụ (đã inline)
        const uint8_t* next_ip = ImplFn(ip, regs, constants, state);

        // 2. Dispatch tiếp theo
        if (next_ip) {
            // Compile-time check: Chỉ reload nếu opcode làm thay đổi frame
            if constexpr (IsFrameChange<Op>) {
                regs = state->registers;
                constants = state->constants;
            }
            [[clang::musttail]] return dispatch(next_ip, regs, constants, state);
        }
        
        // 3. Nếu next_ip == nullptr -> Dừng VM (HALT hoặc Error không cứu được)
    }

    // --- Khởi tạo bảng Dispatch ---
    struct TableInitializer {
        TableInitializer() {
            // 1. Mặc định gán UNIMPL
            // (Ta dùng một OpCode giả định hoặc HALT cho wrapper mặc định)
            for (int i = 0; i < 256; ++i) {
                dispatch_table[i] = op_wrapper<OpCode::HALT, handlers::impl_UNIMPL>;
            }

            // Macro cục bộ giúp đăng ký gọn gàng, tránh lặp lại logic template
            #define REG(NAME) dispatch_table[static_cast<size_t>(OpCode::NAME)] = op_wrapper<OpCode::NAME, handlers::impl_##NAME>

            // 2. Đăng ký OpCode
            
            // Load / Move
            REG(LOAD_CONST); REG(LOAD_NULL); REG(LOAD_TRUE); REG(LOAD_FALSE);
            REG(LOAD_INT); REG(LOAD_FLOAT); REG(MOVE);

            // Math
            REG(ADD); REG(SUB); REG(MUL); REG(DIV); REG(MOD); REG(POW);
            REG(EQ); REG(NEQ); REG(GT); REG(GE); REG(LT); REG(LE);
            REG(NEG); REG(NOT);
            REG(BIT_AND); REG(BIT_OR); REG(BIT_XOR); REG(BIT_NOT);
            REG(LSHIFT); REG(RSHIFT);

            // Variables / Memory
            REG(GET_GLOBAL); REG(SET_GLOBAL);
            REG(GET_UPVALUE); REG(SET_UPVALUE);
            REG(CLOSURE); REG(CLOSE_UPVALUES);

            // Control Flow
            REG(JUMP); REG(JUMP_IF_FALSE); REG(JUMP_IF_TRUE);
            REG(CALL); REG(CALL_VOID); REG(RETURN); REG(HALT);

            // Data Structures
            REG(NEW_ARRAY); REG(NEW_HASH);
            REG(GET_INDEX); REG(SET_INDEX);
            REG(GET_KEYS); REG(GET_VALUES);

            // OOP
            REG(NEW_CLASS); REG(NEW_INSTANCE);
            REG(GET_PROP); REG(SET_PROP); REG(SET_METHOD);
            REG(INHERIT); REG(GET_SUPER);

            // Exception
            REG(THROW); REG(SETUP_TRY); REG(POP_TRY);

            // Modules
            REG(IMPORT_MODULE); REG(EXPORT); REG(GET_EXPORT); REG(IMPORT_ALL);

            #undef REG
        }
    };
    
    static TableInitializer init_trigger;

} // namespace anonymous

// --- Public API ---
void Interpreter::run(VMState state) noexcept {
    if (!state.ctx.current_frame_) return;
    
    // 1. Cập nhật cache pointers lần đầu tiên
    state.update_pointers();
    
    // 2. Load hot data vào biến cục bộ (để Compiler đưa vào Registers)
    Value* regs = state.registers;
    Value* constants = state.constants;
    const uint8_t* ip = state.ctx.current_frame_->ip_;
    
    // 3. Bắt đầu chuỗi dispatch
    dispatch(ip, regs, constants, &state);
}

} // namespace meow
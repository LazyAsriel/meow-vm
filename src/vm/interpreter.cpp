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

} // namespace meow
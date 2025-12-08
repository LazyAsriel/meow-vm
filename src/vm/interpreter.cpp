#include "vm/interpreter.h"
#include <array>

#include "vm/handlers/data_ops.h"
#include "vm/handlers/math_ops.h"
#include "vm/handlers/flow_ops.h"

namespace meow {
namespace {

    // --- Định nghĩa Types ---
    using OpHandler = void (*)(const uint8_t*, VMState*);
    using OpImpl = const uint8_t* (*)(const uint8_t*, VMState*);

    // Bảng dispatch toàn cục
    static OpHandler dispatch_table[256];

    // Forward declaration
    [[gnu::always_inline]]
    static void dispatch(const uint8_t* ip, VMState* state) {
        uint8_t opcode = *ip++;
        [[clang::musttail]] return dispatch_table[opcode](ip, state);
    }

    // --- Template Wrapper (The Magic) ---
    // Nhận Logic (ImplFn) -> Chạy Logic -> Lấy Next IP -> Musttail Jump
    template <OpImpl ImplFn>
    static void op_wrapper(const uint8_t* ip, VMState* state) {
        if (const uint8_t* next_ip = ImplFn(ip, state)) {
            [[clang::musttail]] return dispatch(next_ip, state);
        }
    }

    // --- Khởi tạo bảng ---
struct TableInitializer {
        TableInitializer() {
            // 1. Fill default
            for (int i = 0; i < 256; ++i) dispatch_table[i] = op_wrapper<handlers::impl_UNIMPL>;

            // Helper lambda để đăng ký cho gọn, tự động cast OpCode sang size_t
            auto reg = [](OpCode op, OpHandler handler) {
                dispatch_table[static_cast<size_t>(op)] = handler;
            };

            // 2. Đăng ký OpCode (Dùng hàm reg thay vì gán trực tiếp)
            reg(OpCode::LOAD_CONST,   op_wrapper<handlers::impl_LOAD_CONST>);
            reg(OpCode::ADD,          op_wrapper<handlers::impl_ADD>);
            reg(OpCode::LT,           op_wrapper<handlers::impl_LT>);
            reg(OpCode::JUMP_IF_TRUE, op_wrapper<handlers::impl_JUMP_IF_TRUE>);
            reg(OpCode::RETURN,       op_wrapper<handlers::impl_RETURN>);
            reg(OpCode::HALT,         op_wrapper<handlers::impl_HALT>);
            
            // ... Thêm opcode khác thì cứ reg(...) thôi ...
        }
    };
    static TableInitializer init_trigger;

} // namespace anonymous

// --- Public Entry Point ---
void Interpreter::run(VMState state) noexcept {
    if (!state.ctx.current_frame_) return;
    const uint8_t* ip = state.ctx.current_frame_->ip_;
    dispatch(ip, &state);
}

} // namespace meow
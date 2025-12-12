#include <meow/machine.h>
#include "vm/interpreter.h"
#include "vm/vm_state.h" // Include nội bộ được phép ở đây

namespace meow {

// [FIX] Chuyển thành hàm static helper, không phải member của Machine
static VMState create_vm_state(Machine& m, const uint8_t* ip) {
    // Cần friend hoặc truy cập public getters. 
    // Trong code hiện tại các member context_, heap_ đang là private.
    // Tuy nhiên, vì ta đang ở trong machine.cpp, ta có thể truy cập member của 'm' 
    // nếu ta vẫn giữ logic cũ nhưng không khai báo trong header.
    // NHƯNG: Cách đơn giản nhất là copy logic khởi tạo vào execute/run 
    // để tránh rắc rối quyền truy cập.
    return VMState {
        m,           
        *m.context_,       
        *m.heap_,          
        *m.mod_manager_,   
        m.context_->current_regs_, 
        nullptr,                 
        ip,                      
        nullptr,                 
        "", false        
    };
}

// [FIX] Vì Machine::context_ là private, hàm static bên ngoài không truy cập được 
// trừ khi dùng friend.
// GIẢI PHÁP TỐT NHẤT: Viết logic tạo state trực tiếp trong các hàm run/execute.

void Machine::execute(function_t func) {
    if (!func) return;

    context_->reset();

    proto_t proto = func->get_proto();
    size_t num_regs = proto->get_num_registers();
    
    if (!context_->check_overflow(num_regs)) {
        error("Stack Overflow on startup");
        return;
    }

    *context_->frame_ptr_ = CallFrame(
        func, 
        context_->stack_, 
        nullptr,          
        proto->get_chunk().get_code()
    );

    context_->current_regs_ = context_->stack_;
    context_->stack_top_ += num_regs; 
    context_->current_frame_ = context_->frame_ptr_;

    // [FIX] Tạo state trực tiếp
    VMState state {
        *this,           
        *context_,       
        *heap_,          
        *mod_manager_,   
        context_->current_regs_,
        nullptr,                 
        proto->get_chunk().get_code(),
        nullptr,                 
        "", false        
    };
    
    Interpreter::run(state);
    
    if (state.has_error()) {
        this->error(std::string(state.get_error_message()));
    }
}

void Machine::run() noexcept {
    const uint8_t* initial_code = context_->frame_ptr_->function_->get_proto()->get_chunk().get_code();
    
    // [FIX] Tạo state trực tiếp
    VMState state {
        *this,           
        *context_,       
        *heap_,          
        *mod_manager_,   
        context_->current_regs_,
        nullptr,                 
        initial_code,
        nullptr,                 
        "", false        
    };
    
    if (context_->frame_ptr_->function_->get_proto()->get_module()) {
        state.current_module = context_->frame_ptr_->function_->get_proto()->get_module();
    }

    Interpreter::run(state);
    
    if (state.has_error()) [[unlikely]] {
        this->error(std::string(state.get_error_message()));
    }
}

// Giữ lại các constructor/destructor cũ...
// (Bạn có thể giữ nguyên phần còn lại của file machine.cpp gốc, 
// chỉ cần xóa hàm Machine::create_state đi và thay nội dung execute/run như trên)
}
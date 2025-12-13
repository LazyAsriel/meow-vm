#include <meow/machine.h>
#include "vm/interpreter.h"
#include "vm/vm_state.h"

namespace meow {

// Đã xóa hàm static create_vm_state gây lỗi truy cập private

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

}

#include "vm/machine.h"
#include "vm/interpreter.h"

namespace meow {

void Machine::run() noexcept {
    const uint8_t* initial_code = context_->frame_ptr_->function_->get_proto()->get_chunk().get_code();

    VMState state {
        *this,           
        *context_,       
        *heap_,          
        *mod_manager_,   
        context_->current_regs_, 
        nullptr,         // [FIX] Thêm constants (khởi tạo null, update_pointers sẽ điền sau)
        initial_code,    
        "", false        
    };

    Interpreter::run(state);
    
    if (state.has_error()) [[unlikely]] {
        this->error(std::string(state.get_error_message()));
    }
}

}
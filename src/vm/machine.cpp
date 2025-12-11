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
        nullptr,         // Constants (sẽ được update)
        initial_code,
        // [FIX] Thêm nullptr cho trường 'current_module' mới thêm vào struct VMState
        nullptr,    
        "", false        
    };

    Interpreter::run(state);
    
    if (state.has_error()) [[unlikely]] {
        this->error(std::string(state.get_error_message()));
    }
}

}
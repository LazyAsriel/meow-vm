#include "vm/machine.h"
#include "vm/interpreter.h"

namespace meow {

void Machine::run() noexcept {
    VMState state {
        *this,           // Machine&
        *context_,       // ExecutionContext&
        *heap_,          // MemoryManager&
        *mod_manager_,   // ModuleManager&
        "", false        // Error state
    };

    Interpreter::run(state);
    
    if (state.has_error()) [[unlikely]] {
        this->error(std::string(state.get_error_message()));
    }
}

}
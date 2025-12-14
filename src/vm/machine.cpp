#include <meow/machine.h>
#include "vm/interpreter.h"
#include "vm/vm_state.h"
#include <meow/memory/memory_manager.h>
#include "vm/handlers/oop_ops.h"

namespace meow {

Value Machine::call_callable(Value callable, const std::vector<Value>& args) noexcept {
    if (callable.is_native()) {
        return callable.as_native()(this, static_cast<int>(args.size()), const_cast<Value*>(args.data()));
    }

    function_t closure = nullptr;
    instance_t self = nullptr;

    if (callable.is_function()) {
        closure = callable.as_function();
    } else if (callable.is_bound_method()) {
        auto bm = callable.as_bound_method();
        self = bm->get_instance();
        closure = bm->get_function();
    } else if (callable.is_class()) {
        class_t k = callable.as_class();
        self = heap_->new_instance(k, heap_->get_empty_shape());
        Value init = k->get_method(heap_->new_string("init"));
        if (init.is_function()) {
            closure = init.as_function();
        } else {
            return Value(self);
        }
    } else {
        error("Runtime Error: Attempt to call a non-callable value.");
        return Value(null_t{});
    }

    proto_t proto = closure->get_proto();
    const size_t num_params = proto->get_num_registers();
    const size_t argc = args.size();

    if (!context_->check_overflow(num_params) || !context_->check_frame_overflow()) [[unlikely]] {
        error("Stack Overflow: Cannot push arguments for native callback.");
        return Value(null_t{});
    }

    Value* base = context_->stack_top_;
    size_t arg_offset = 0;

    if (self) {
        base[0] = Value(self);
        arg_offset = 1;
    }

    const size_t copy_count = std::min(argc, num_params - arg_offset);
    for (size_t i = 0; i < copy_count; ++i) {
        base[arg_offset + i] = args[i];
    }

    for (size_t i = arg_offset + copy_count; i < num_params; ++i) {
        base[i] = Value(null_t{});
    }

    context_->frame_ptr_++;
    Value return_val = Value(null_t{});

    *context_->frame_ptr_ = CallFrame(
        closure,
        base,
        &return_val,
        proto->get_chunk().get_code()
    );

    context_->current_regs_ = base;
    context_->stack_top_ += num_params;
    context_->current_frame_ = context_->frame_ptr_;

    VMState state {
        *this, *context_, *heap_, *mod_manager_,
        context_->current_regs_, nullptr, proto->get_chunk().get_code(),
        nullptr, "", false
    };
    Interpreter::run(state);

    if (state.has_error()) [[unlikely]] {
        error(std::string(state.get_error_message()));
        return Value(null_t{});
    }

    if (self && callable.is_class()) return Value(self);

    return return_val;
}

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
    std::cout << "Size of Entry: " << sizeof(meow::handlers::InlineCacheEntry) << "\n";
    std::cout << "Size of Cache: " << sizeof(meow::handlers::InlineCache) << "\n";
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

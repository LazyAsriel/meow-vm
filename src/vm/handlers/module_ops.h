#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include "module/module_manager.h"

namespace meow::handlers {

[[gnu::always_inline]] 
static const uint8_t* impl_EXPORT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t name_idx = read_u16(ip);
    uint16_t src_reg = read_u16(ip);
    Value val = regs[src_reg];
    
    string_t name = constants[name_idx].as_string();
    state->current_module->set_export(name, val);
    
    state->heap.write_barrier(state->current_module, val);
    
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_GET_EXPORT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t mod_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    
    Value& mod_val = regs[mod_reg];
    string_t name = constants[name_idx].as_string();
    
    if (!mod_val.is_module()) [[unlikely]] {
        state->error("GET_EXPORT: Toán hạng không phải là Module.", ip);
        return impl_PANIC(ip, regs, constants, state);
    }
    
    module_t mod = mod_val.as_module();
    if (!mod->has_export(name)) [[unlikely]] {
        state->error("Module không export '" + std::string(name->c_str()) + "'.", ip);
        return impl_PANIC(ip, regs, constants, state);
    }
    
    regs[dst] = mod->get_export(name);
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_IMPORT_ALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t src_idx = read_u16(ip);
    (void)constants;
    
    const Value& mod_val = regs[src_idx];
    
    if (auto src_mod = mod_val.as_if_module()) {
        state->current_module->import_all_export(src_mod);
    } else [[unlikely]] {
        state->error("IMPORT_ALL: Register không chứa Module.", ip);
        return impl_PANIC(ip, regs, constants, state);
    }
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_IMPORT_MODULE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t path_idx = read_u16(ip);
    
    string_t path = constants[path_idx].as_string();
    string_t importer_path = state->current_module ? state->current_module->get_file_path() : nullptr;
    
    module_t mod = state->modules.load_module(path, importer_path);
    regs[dst] = Value(mod);

    if (mod->is_executed() || mod->is_executing()) {
        return ip;
    }
    
    if (!mod->is_has_main()) {
        mod->set_executed();
        return ip;
    }

    mod->set_execution();
    
    proto_t main_proto = mod->get_main_proto();
    function_t main_closure = state->heap.new_function(main_proto);
    
    size_t num_regs = main_proto->get_num_registers();

    if (!state->ctx.check_frame_overflow()) [[unlikely]] {
        state->error("Call Stack Overflow (too many imports)!", ip);
        return impl_PANIC(ip, regs, constants, state);
    }
    if (!state->ctx.check_overflow(num_regs)) [[unlikely]] {
        state->error("Register Stack Overflow at import!", ip);
        return impl_PANIC(ip, regs, constants, state);
    }
    
    Value* new_base = state->ctx.stack_top_;
    state->ctx.frame_ptr_++; 
    *state->ctx.frame_ptr_ = CallFrame(
        main_closure,
        new_base,
        nullptr,
        ip
    );
    
    state->ctx.current_regs_ = new_base;
    state->ctx.stack_top_ += num_regs;
    state->ctx.current_frame_ = state->ctx.frame_ptr_;
    
    state->update_pointers();
    
    return main_proto->get_chunk().get_code(); 
}

} // namespace meow::handlers
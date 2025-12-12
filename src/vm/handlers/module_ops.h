#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include "module/module_manager.h"

namespace meow::handlers {

[[gnu::always_inline]] static const uint8_t* impl_EXPORT(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t name_idx = read_u16(ip);
    uint16_t src_reg = read_u16(ip);
    string_t name = constants[name_idx].as_string();
    
    // [FIX] Dùng state->current_module thay vì frame_ptr_->module_
    state->current_module->set_export(name, regs[src_reg]);
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_GET_EXPORT(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t mod_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    
    Value& mod_val = regs[mod_reg];
    string_t name = constants[name_idx].as_string();
    
    if (!mod_val.is_module()) {
        state->error("GET_EXPORT: Toán hạng không phải là Module.");
        return impl_PANIC(ip, regs, constants, state);
    }
    module_t mod = mod_val.as_module();
    if (!mod->has_export(name)) {
        state->error("Module không export '" + std::string(name->c_str()) + "'.");
        return impl_PANIC(ip, regs, constants, state);
    }
    regs[dst] = mod->get_export(name);
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_IMPORT_ALL(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t src_idx = read_u16(ip);
    (void)constants;
    const Value& mod_val = regs[src_idx];
    
    if (auto src_mod = mod_val.as_if_module()) {
        // [FIX] Dùng state->current_module
        module_t curr_mod = state->current_module;
        curr_mod->import_all_export(src_mod);
    } else {
        state->error("IMPORT_ALL: Register không chứa Module.");
        return impl_PANIC(ip, regs, constants, state);
    }
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_IMPORT_MODULE(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t path_idx = read_u16(ip);
    
    string_t path = constants[path_idx].as_string();
    // [FIX] Dùng state->current_module
    string_t importer_path = state->current_module->get_file_path();
    
    module_t mod = state->modules.load_module(path, importer_path);
    regs[dst] = Value(mod);
    
    if (mod->is_executed() || mod->is_executing()) {
        return ip;
    }
    if (!mod->is_has_main()) {
        mod->set_executed();
        return ip;
    }

    // --- Thực thi module main ---
    mod->set_execution();
    proto_t main_proto = mod->get_main_proto();
    function_t main_closure = state->heap.new_function(main_proto);
    
    size_t num_regs = main_proto->get_num_registers();
    if (!state->ctx.check_frame_overflow()) {
        state->error("Stack Overflow (Call Stack) at import!");
        return impl_PANIC(ip, regs, constants, state);
    }
    if (!state->ctx.check_overflow(num_regs)) {
        state->error("Stack Overflow (Registers) at import!");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    state->ctx.frame_ptr_->ip_ = ip; 
    
    Value* new_base = state->ctx.stack_top_;
    state->ctx.frame_ptr_++; 
    
    // [FIX] Bỏ tham số 'mod' trong constructor CallFrame
    *state->ctx.frame_ptr_ = CallFrame(
        main_closure,
        new_base,
        nullptr, 
        main_proto->get_chunk().get_code()
    );
    
    state->ctx.current_regs_ = new_base;
    state->ctx.stack_top_ += num_regs;
    
    state->ctx.current_frame_ = state->ctx.frame_ptr_;
    state->update_pointers();
    
    return state->ctx.frame_ptr_->ip_; 
}

} // namespace meow::handlers
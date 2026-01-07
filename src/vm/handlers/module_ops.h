#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include "module/module_manager.h"

namespace meow::handlers {

constexpr int ERR_MODULE = 30;
constexpr int ERR_STACK_OVERFLOW = 31;

[[gnu::always_inline]] 
static const uint8_t* impl_EXPORT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 2 * u16 = 4 bytes -> Load u32
    auto [name_idx, src_reg] = decode::args<u16, u16>(ip);
    Value val = regs[src_reg];
    
    string_t name = constants[name_idx].as_string();
    state->current_module->set_export(name, val);
    
    state->heap.write_barrier(state->current_module, val);
    
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_GET_EXPORT(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 3 * u16 = 6 bytes -> "Dân chơi" load u64 (đọc lố padding)
    auto [dst, mod_reg, name_idx] = decode::args<u16, u16, u16>(ip);
    
    Value& mod_val = regs[mod_reg];
    string_t name = constants[name_idx].as_string();
    
    if (!mod_val.is_module()) [[unlikely]] {
        return ERROR<6>(ip, regs, constants, state, ERR_MODULE, "GET_EXPORT: Operand is not a Module");
    }
    
    module_t mod = mod_val.as_module();
    if (!mod->has_export(name)) [[unlikely]] {
        return ERROR<6>(ip, regs, constants, state, ERR_MODULE, "Module does not export '{}'", name->c_str());
    }
    
    regs[dst] = mod->get_export(name);
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_IMPORT_ALL(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    auto [src_idx] = decode::args<u16>(ip);
    
    const Value& mod_val = regs[src_idx];
    
    if (auto src_mod = mod_val.as_if_module()) {
        state->current_module->import_all_export(src_mod);
    } else [[unlikely]] {
        return ERROR<2>(ip, regs, constants, state, ERR_MODULE, "IMPORT_ALL: Register does not contain a Module");
    }
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_IMPORT_MODULE(const uint8_t* ip, Value* regs, const Value* constants, VMState* state) {
    // 2 * u16 = 4 bytes -> Load u32
    auto [dst, path_idx] = decode::args<u16, u16>(ip);
    
    string_t path = constants[path_idx].as_string();
    string_t importer_path = state->current_module ? state->current_module->get_file_path() : nullptr;
    
    auto load_result = state->modules.load_module(path, importer_path);

    if (load_result.failed()) {
        auto err = load_result.error();
        return ERROR<4>(ip, regs, constants, state, ERR_MODULE, 
                        "Cannot import module '{}'. Code: {}", path->c_str(), static_cast<int>(err.code()));
    }

    module_t mod = load_result.value();
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
        return ERROR<4>(ip, regs, constants, state, ERR_STACK_OVERFLOW, "Call Stack Overflow (too many imports)");
    }
    if (!state->ctx.check_overflow(num_regs)) [[unlikely]] {
        return ERROR<4>(ip, regs, constants, state, ERR_STACK_OVERFLOW, "Register Stack Overflow at import");
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
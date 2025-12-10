#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include "module/module_manager.h"

namespace meow::handlers {

[[gnu::always_inline]] static const uint8_t* impl_EXPORT(const uint8_t* ip, VMState* state) {
    uint16_t name_idx = read_u16(ip);
    uint16_t src_reg = read_u16(ip);
    string_t name = state->constant(name_idx).as_string();
    
    // [FIX] Dùng frame_ptr_ thay vì current_frame_
    state->ctx.frame_ptr_->module_->set_export(name, state->reg(src_reg));
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_GET_EXPORT(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t mod_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    
    Value& mod_val = state->reg(mod_reg);
    string_t name = state->constant(name_idx).as_string();
    
    if (!mod_val.is_module()) {
        state->error("GET_EXPORT: Toán hạng không phải là Module.");
        return impl_PANIC(ip, state);
    }
    module_t mod = mod_val.as_module();
    if (!mod->has_export(name)) {
        state->error("Module không export '" + std::string(name->c_str()) + "'.");
        return impl_PANIC(ip, state);
    }
    state->reg(dst) = mod->get_export(name);
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_IMPORT_ALL(const uint8_t* ip, VMState* state) {
    uint16_t src_idx = read_u16(ip);
    const Value& mod_val = state->reg(src_idx);
    
    if (auto src_mod = mod_val.as_if_module()) {
        // [FIX] Dùng frame_ptr_
        module_t curr_mod = state->ctx.frame_ptr_->module_;
        curr_mod->import_all_export(src_mod);
    } else {
        state->error("IMPORT_ALL: Register không chứa Module.");
        return impl_PANIC(ip, state);
    }
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_IMPORT_MODULE(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t path_idx = read_u16(ip);
    
    string_t path = state->constant(path_idx).as_string();
    // [FIX] Dùng frame_ptr_
    string_t importer_path = state->ctx.frame_ptr_->module_->get_file_path();
    
    module_t mod = state->modules.load_module(path, importer_path);
    state->reg(dst) = Value(mod);
    
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
    
    // [FIX] Logic mới: Check Stack Overflow
    size_t num_regs = main_proto->get_num_registers();
    if (!state->ctx.check_frame_overflow()) {
        state->error("Stack Overflow (Call Stack) at import!");
        return impl_PANIC(ip, state);
    }
    if (!state->ctx.check_overflow(num_regs)) {
        state->error("Stack Overflow (Registers) at import!");
        return impl_PANIC(ip, state);
    }
    
    // Lưu lại IP hiện tại để quay về sau khi chạy xong module
    state->ctx.frame_ptr_->ip_ = ip; 
    
    // [FIX] Push frame mới dùng con trỏ
    Value* new_base = state->ctx.stack_top_;
    state->ctx.frame_ptr_++; // Tăng frame pointer
    
    // Khởi tạo Frame trực tiếp trên mảng tĩnh
    *state->ctx.frame_ptr_ = CallFrame(
        main_closure,
        mod,
        new_base,
        nullptr, // Module main không trả về giá trị (hoặc ignore)
        main_proto->get_chunk().get_code()
    );
    
    // Update Context State
    state->ctx.current_regs_ = new_base;
    state->ctx.stack_top_ += num_regs;
    
    // Sync legacy pointers & Cache
    state->ctx.current_frame_ = state->ctx.frame_ptr_;
    state->update_pointers(); // Cực kỳ quan trọng để VMState nhận diện registers mới
    
    // Trả về IP của module main để Musttail nhảy tới
    return state->ctx.frame_ptr_->ip_;
}

} // namespace meow::handlers
#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include "module/module_manager.h"

namespace meow::handlers {

[[always_inline]] static const uint8_t* impl_EXPORT(const uint8_t* ip, VMState* state) {
    uint16_t name_idx = read_u16(ip);
    uint16_t src_reg = read_u16(ip);
    string_t name = state->constant(name_idx).as_string();
    
    state->ctx.current_frame_->module_->set_export(name, state->reg(src_reg));
    return ip;
}

[[always_inline]] static const uint8_t* impl_GET_EXPORT(const uint8_t* ip, VMState* state) {
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

[[always_inline]] static const uint8_t* impl_IMPORT_ALL(const uint8_t* ip, VMState* state) {
    uint16_t src_idx = read_u16(ip);
    const Value& mod_val = state->reg(src_idx);
    
    if (auto src_mod = mod_val.as_if_module()) {
        module_t curr_mod = state->ctx.current_frame_->module_;
        curr_mod->import_all_export(src_mod);
    } else {
        state->error("IMPORT_ALL: Register không chứa Module.");
        return impl_PANIC(ip, state);
    }
    return ip;
}

[[always_inline]] static const uint8_t* impl_IMPORT_MODULE(const uint8_t* ip, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t path_idx = read_u16(ip);
    
    string_t path = state->constant(path_idx).as_string();
    string_t importer_path = state->ctx.current_frame_->module_->get_file_path();
    
    // Load module (có thể ném exception C++ nếu IO lỗi, nhưng ở đây ta giả định ModuleManager an toàn hoặc handle ở ngoài)
    module_t mod = state->modules.load_module(path, importer_path);
    state->reg(dst) = Value(mod);
    
    // Nếu module đã chạy xong hoặc đang chạy (circular dependency), không cần chạy lại main
    if (mod->is_executed() || mod->is_executing()) {
        return ip;
    }
    // Nếu module native (không có main proto), đánh dấu đã xong
    if (!mod->is_has_main()) {
        mod->set_executed();
        return ip;
    }

    // --- Thực thi module main ---
    mod->set_execution();
    proto_t main_proto = mod->get_main_proto();
    function_t main_closure = state->heap.new_function(main_proto);
    
    // Lưu lại IP hiện tại để quay về sau khi chạy xong module
    state->ctx.current_frame_->ip_ = ip; 
    
    // Push frame mới cho module main
    size_t new_base = state->ctx.registers_.size();
    state->ctx.registers_.resize(new_base + main_proto->get_num_registers());
    
    state->ctx.call_stack_.emplace_back(
        main_closure,
        mod,
        new_base,
        static_cast<size_t>(-1), // Không cần giá trị trả về
        main_proto->get_chunk().get_code()
    );
    
    state->ctx.current_frame_ = &state->ctx.call_stack_.back();
    state->ctx.current_base_ = state->ctx.current_frame_->start_reg_;
    
    // Trả về IP của module main để Musttail nhảy tới
    return state->ctx.current_frame_->ip_;
}

} // namespace meow::handlers
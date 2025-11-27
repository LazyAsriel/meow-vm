#pragma once
// Chứa các handler cho Module, Import, Export

inline void Machine::op_export(const uint8_t*& ip) {
    uint16_t name_idx = READ_U16();
    uint16_t src_reg = READ_U16();
    string_t name = CONSTANT(name_idx).as_string();
    context_->current_frame_->module_->set_export(name, REGISTER(src_reg));
}

inline void Machine::op_get_export(const uint8_t*& ip) {
    uint16_t dst = READ_U16();
    uint16_t mod_reg = READ_U16();
    uint16_t name_idx = READ_U16();
    Value& mod_val = REGISTER(mod_reg);
    string_t name = CONSTANT(name_idx).as_string();
    if (!mod_val.is_module()) throw_vm_error("GET_EXPORT: operand is not a module.");
    module_t mod = mod_val.as_module();
    if (!mod->has_export(name)) throw_vm_error("Module does not export name.");
    REGISTER(dst) = mod->get_export(name);
}

inline void Machine::op_import_all(const uint8_t*& ip) {
    uint16_t src_idx = READ_U16();
    const Value& mod_val = REGISTER(src_idx);
    if (auto src_mod = mod_val.as_if_module()) {
        module_t curr_mod = context_->current_frame_->module_;
        curr_mod->import_all_export(src_mod);
    } else {
        throw_vm_error("IMPORT_ALL: Source register does not contain a Module object.");
    }
}
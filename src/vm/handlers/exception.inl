#pragma once

inline void Machine::op_setup_try(const uint8_t*& ip) {
    uint16_t target = READ_ADDRESS(); // Địa chỉ nhảy tới catch
    uint16_t err_reg = READ_U16();    //  Register lưu lỗi

    size_t catch_ip = target;
    // Lưu trạng thái hiện tại
    size_t frame_depth = context_->call_stack_.size() - 1;
    size_t stack_depth = context_->registers_.size();

    // Push vào danh sách handler
    context_->exception_handlers_.emplace_back(catch_ip, frame_depth, stack_depth, err_reg);
    
    // Debug chơi cho vui
    printl("SETUP_TRY -> Catch: {}, Reg: {}", catch_ip, err_reg);
}

inline void Machine::op_pop_try() {
    if (!context_->exception_handlers_.empty()) {
        context_->exception_handlers_.pop_back();
    }
}

inline void Machine::op_throw(const uint8_t*& ip) {
    uint16_t reg = READ_U16();
    Value& val = REGISTER(reg);
    // Ném lỗi với message từ register
    throw_vm_error(to_string(val)); 
}
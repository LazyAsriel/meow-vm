#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include "module/module_manager.h"

namespace meow::handlers {

[[gnu::always_inline]] 
static const uint8_t* impl_EXPORT(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t name_idx = read_u16(ip);
    uint16_t src_reg = read_u16(ip);
    
    string_t name = constants[name_idx].as_string();
    
    // Debug log (Optional)
    // string_t mod_path = state->current_module ? state->current_module->get_file_path() : state->heap.new_string("NULL");
    // std::println(">>> OP_EXPORT: Key='{}' | Into Module={}", name->c_str(), mod_path->c_str());

    state->current_module->set_export(name, regs[src_reg]);
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_GET_EXPORT(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t mod_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    
    Value& mod_val = regs[mod_reg];
    string_t name = constants[name_idx].as_string();
    
    if (!mod_val.is_module()) [[unlikely]] {
        state->error("GET_EXPORT: Toán hạng không phải là Module.");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    module_t mod = mod_val.as_module();
    if (!mod->has_export(name)) [[unlikely]] {
        state->error("Module không export '" + std::string(name->c_str()) + "'.");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    regs[dst] = mod->get_export(name);
    return ip;
}

[[gnu::always_inline]] 
static const uint8_t* impl_IMPORT_ALL(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t src_idx = read_u16(ip);
    (void)constants;
    
    const Value& mod_val = regs[src_idx];
    
    if (auto src_mod = mod_val.as_if_module()) {
        state->current_module->import_all_export(src_mod);
    } else [[unlikely]] {
        state->error("IMPORT_ALL: Register không chứa Module.");
        return impl_PANIC(ip, regs, constants, state);
    }
    return ip;
}

// ============================================================================
// CRITICAL FIX: IMPORT_MODULE
// ============================================================================
[[gnu::always_inline]] 
static const uint8_t* impl_IMPORT_MODULE(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    // 1. Đọc tham số
    uint16_t dst = read_u16(ip);
    uint16_t path_idx = read_u16(ip);
    
    // 2. Load Module
    string_t path = constants[path_idx].as_string();
    string_t importer_path = state->current_module ? state->current_module->get_file_path() : nullptr;
    
    // Module được cache tại đây, nếu load rồi thì trả về instance cũ
    module_t mod = state->modules.load_module(path, importer_path);
    
    // 3. Gán object module vào thanh ghi đích NGAY LẬP TỨC
    // (Để code phía sau có thể dùng ngay module này kể cả khi code top-level của nó chưa chạy xong)
    regs[dst] = Value(mod);

    // 4. Kiểm tra vòng lặp hoặc đã thực thi
    if (mod->is_executed() || mod->is_executing()) {
        // Nếu đang chạy (circular dependency) hoặc đã chạy xong -> Skip execution
        return ip;
    }
    
    // Module chỉ chứa dữ liệu, không có code main -> Đánh dấu xong luôn
    if (!mod->is_has_main()) {
        mod->set_executed();
        return ip;
    }

    // 5. Chuẩn bị thực thi code top-level của Module (như một hàm void)
    mod->set_execution(); // Đánh dấu đang chạy (để tránh infinite recursion)
    
    proto_t main_proto = mod->get_main_proto();
    function_t main_closure = state->heap.new_function(main_proto);
    
    size_t num_regs = main_proto->get_num_registers();

    // 6. Kiểm tra Stack Overflow
    if (!state->ctx.check_frame_overflow()) [[unlikely]] {
        state->error("Call Stack Overflow (too many imports)!");
        return impl_PANIC(ip, regs, constants, state);
    }
    if (!state->ctx.check_overflow(num_regs)) [[unlikely]] {
        state->error("Register Stack Overflow at import!");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    // 7. Tạo Frame mới
    Value* new_base = state->ctx.stack_top_;
    state->ctx.frame_ptr_++; 
    
    // [CRITICAL FIX] ---------------------------------------------------------
    // CallFrame(function, regs_base, return_destination, return_address)
    // 
    // - return_destination = nullptr: Code module chạy xong không trả về giá trị gì vào thanh ghi caller.
    // - return_address = ip: ĐÂY LÀ ĐIỂM QUAN TRỌNG NHẤT.
    //   Ta phải lưu 'ip' hiện tại (đang trỏ tới lệnh ngay sau IMPORT_MODULE của file cha)
    //   để khi file con chạy xong lệnh RETURN, nó biết đường nhảy về file cha.
    // ------------------------------------------------------------------------
    *state->ctx.frame_ptr_ = CallFrame(
        main_closure,
        new_base,
        nullptr, // Không cần hứng giá trị trả về từ main module
        ip       // <--- Return Address: Quay về caller (importer)
    );
    
    // 8. Cập nhật State để chuyển ngữ cảnh sang Module mới
    state->ctx.current_regs_ = new_base;
    state->ctx.stack_top_ += num_regs;
    state->ctx.current_frame_ = state->ctx.frame_ptr_;
    
    // Cập nhật pointers cache (constants, registers, instruction_base...)
    state->update_pointers();
    
    // 9. Jump: Trả về địa chỉ bắt đầu code của Module mới
    return main_proto->get_chunk().get_code(); 
}

} // namespace meow::handlers
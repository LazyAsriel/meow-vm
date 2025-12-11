#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include "core/objects/shape.h" // Đảm bảo đã include Shape

namespace meow::handlers {

// Cấu trúc Cache nằm ngay trong dòng lệnh Bytecode (Alignment 1 byte)
#pragma pack(push, 1)
struct InlineCache {
    const Shape* shape; // 8 bytes (trên 64-bit)
    uint32_t offset;    // 4 bytes
};
#pragma pack(pop)

// Helper: Đọc/Ghi Cache từ Instruction Pointer
// Lưu ý: Con trỏ ip đang trỏ tới byte tiếp theo sau các tham số u16
[[gnu::always_inline]]
inline static InlineCache* get_inline_cache(const uint8_t*& ip) {
    auto* ic = reinterpret_cast<InlineCache*>(const_cast<uint8_t*>(ip));
    ip += sizeof(InlineCache); // Nhảy qua vùng cache để trỏ tới lệnh kế tiếp
    return ic;
}

// --- NEW_CLASS & NEW_INSTANCE (Giữ nguyên, chỉ fix format nếu cần) ---

[[gnu::always_inline]] static const uint8_t* impl_NEW_CLASS(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    string_t name = constants[name_idx].as_string();
    regs[dst] = Value(state->heap.new_class(name));
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_NEW_INSTANCE(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t class_reg = read_u16(ip);
    
    Value& class_val = regs[class_reg];
    if (!class_val.is_class()) {
        state->error("NEW_INSTANCE: Toán hạng không phải là Class.");
        return impl_PANIC(ip, regs, constants, state);
    }
    // Khởi tạo với Shape rỗng
    regs[dst] = Value(state->heap.new_instance(class_val.as_class(), state->heap.get_empty_shape()));
    return ip;
}

// --- GET_PROP (Có Inline Caching) ---

[[gnu::always_inline]] static const uint8_t* impl_GET_PROP(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    
    // Lấy con trỏ tới vùng cache (nằm ngay sau lệnh)
    InlineCache* ic = get_inline_cache(ip);

    Value& obj = regs[obj_reg];
    
    if (obj.is_instance()) [[likely]] {
        instance_t inst = obj.as_instance();
        Shape* current_shape = inst->get_shape();

        // 1. FAST PATH: Cache Hit?
        // So sánh Shape hiện tại với Shape đã lưu trong cache
        if (current_shape == ic->shape) [[likely]] {
            // BINGO! Không cần hash lookup
            regs[dst] = inst->get_field_at(ic->offset);
            return ip;
        }

        // 2. SLOW PATH: Cache Miss -> Phải tra cứu
        string_t name = constants[name_idx].as_string();
        int offset = current_shape->get_offset(name);

        if (offset != -1) {
            // Tìm thấy! Update Cache ngay lập tức
            ic->shape = current_shape;
            ic->offset = static_cast<uint32_t>(offset);
            
            regs[dst] = inst->get_field_at(offset);
            return ip;
        }

        // Fallback: Tìm method (Method không cache vào Shape IC vì nó nằm ở Class)
        class_t k = inst->get_class();
        while (k) {
            if (k->has_method(name)) {
                regs[dst] = Value(state->heap.new_bound_method(inst, k->get_method(name).as_function()));
                return ip;
            }
            k = k->get_super();
        }
    }
    else if (obj.is_module()) {
        string_t name = constants[name_idx].as_string();
        module_t mod = obj.as_module();
        if (mod->has_export(name)) {
            regs[dst] = mod->get_export(name);
            return ip;
        }
    }
    
    regs[dst] = Value(null_t{});
    return ip;
}

// --- SET_PROP (Có Inline Caching) ---

[[gnu::always_inline]] static const uint8_t* impl_SET_PROP(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    uint16_t val_reg = read_u16(ip);
    
    // Lấy cache
    InlineCache* ic = get_inline_cache(ip);

    Value& obj = regs[obj_reg];
    Value& val = regs[val_reg];
    
    if (obj.is_instance()) [[likely]] {
        instance_t inst = obj.as_instance();
        Shape* current_shape = inst->get_shape();

        // 1. FAST PATH: Cache Hit (Chỉ áp dụng cho Update, không áp dụng cho Transition)
        if (current_shape == ic->shape) [[likely]] {
            inst->set_field_at(ic->offset, val);
            return ip;
        }

        // 2. SLOW PATH
        string_t name = constants[name_idx].as_string();
        int offset = current_shape->get_offset(name);

        if (offset != -1) {
            // [UPDATE] Property đã tồn tại -> Update Cache
            ic->shape = current_shape;
            ic->offset = static_cast<uint32_t>(offset);
            
            inst->set_field_at(offset, val);
        } else {
            // [TRANSITION] Thêm mới -> Logic phức tạp hơn
            // Hiện tại ta KHÔNG cache transition (Polymorphic IC phức tạp hơn nhiều)
            // Chỉ thực hiện logic chuyển shape bình thường.
            
            Shape* next_shape = current_shape->get_transition(name);
            if (next_shape == nullptr) {
                next_shape = current_shape->add_transition(name, &state->heap);
            }

            inst->set_shape(next_shape);
            inst->get_fields_raw().push_back(val);
            
            // Lưu ý: Sau lệnh này, Shape của instance đã đổi thành next_shape.
            // Lần tới chạy lệnh này, nếu instance đã có shape mới, nó sẽ lại cache miss
            // cho đến khi nó ổn định.
        }
    } else {
        state->error("SET_PROP: Chỉ có thể gán thuộc tính cho Instance.");
        return impl_PANIC(ip, regs, constants, state);
    }
    return ip;
}

// --- SET_METHOD, INHERIT, GET_SUPER (Giữ nguyên) ---

[[gnu::always_inline]] static const uint8_t* impl_SET_METHOD(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t class_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    uint16_t method_reg = read_u16(ip);
    
    Value& class_val = regs[class_reg];
    string_t name = constants[name_idx].as_string();
    Value& method_val = regs[method_reg];
    
    if (!class_val.is_class()) {
        state->error("SET_METHOD: Đích không phải là Class.");
        return impl_PANIC(ip, regs, constants, state);
    }
    if (!method_val.is_function()) {
        state->error("SET_METHOD: Giá trị không phải là Function.");
        return impl_PANIC(ip, regs, constants, state);
    }
    class_val.as_class()->set_method(name, method_val);
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_INHERIT(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t sub_reg = read_u16(ip);
    uint16_t super_reg = read_u16(ip);
    (void)constants;
    
    Value& sub_val = regs[sub_reg];
    Value& super_val = regs[super_reg];
    
    if (!sub_val.is_class() || !super_val.is_class()) {
        state->error("INHERIT: Cả 2 toán hạng phải là Class.");
        return impl_PANIC(ip, regs, constants, state);
    }
    sub_val.as_class()->set_super(super_val.as_class());
    return ip;
}

[[gnu::always_inline]] static const uint8_t* impl_GET_SUPER(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    string_t name = constants[name_idx].as_string();
    
    Value& receiver_val = regs[0]; 
    if (!receiver_val.is_instance()) {
        state->error("GET_SUPER: 'super' chỉ dùng được trong method của instance.");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    instance_t receiver = receiver_val.as_instance();
    class_t klass = receiver->get_class();
    class_t super = klass->get_super();
    
    if (!super) {
        state->error("GET_SUPER: Class không có superclass.");
        return impl_PANIC(ip, regs, constants, state);
    }
    
    class_t k = super;
    while (k) {
        if (k->has_method(name)) {
            Value method_val = k->get_method(name);
            if (!method_val.is_function()) break; 
            regs[dst] = Value(state->heap.new_bound_method(receiver, method_val.as_function()));
            return ip;
        }
        k = k->get_super();
    }
    
    state->error("GET_SUPER: Không tìm thấy method '" + std::string(name->c_str()) + "' ở superclass.");
    return impl_PANIC(ip, regs, constants, state);
}

} // namespace meow::handlers
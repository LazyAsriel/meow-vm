#pragma once
#include "vm/handlers/utils.h"
#include "vm/handlers/flow_ops.h"
#include <meow/core/shape.h>

namespace meow::handlers {

// --- Cấu hình Polymorphic Cache ---
static constexpr int IC_CAPACITY = 4;

struct InlineCacheEntry {
    const Shape* shape; // 8 bytes
    uint32_t offset;    // 4 bytes
};

// Kích thước: 12 * 4 = 48 bytes
#pragma pack(push, 1)
struct InlineCache {
    InlineCacheEntry entries[IC_CAPACITY];
};
#pragma pack(pop)

// Helper: Đọc Cache từ dòng lệnh
[[gnu::always_inline]]
inline static InlineCache* get_inline_cache(const uint8_t*& ip) {
    auto* ic = reinterpret_cast<InlineCache*>(const_cast<uint8_t*>(ip));
    ip += sizeof(InlineCache); // Nhảy qua 48 bytes cache
    return ic;
}

// Helper: Cập nhật Cache (Chiến thuật: Move-To-Front)
// Đưa entry mới nhất lên đầu để lần truy cập sau nhanh nhất.
inline static void update_inline_cache(InlineCache* ic, const Shape* shape, uint32_t offset) {
    // 1. Nếu shape đã có trong cache, đưa nó lên đầu (LRU-ish)
    for (int i = 0; i < IC_CAPACITY; ++i) {
        if (ic->entries[i].shape == shape) {
            if (i > 0) {
                InlineCacheEntry temp = ic->entries[i];
                // Dịch chuyển các phần tử phía trước xuống 1 nấc
                for (int j = i; j > 0; --j) {
                    ic->entries[j] = ic->entries[j - 1];
                }
                ic->entries[0] = temp;
                // Cập nhật lại offset phòng khi shape bị rebuild (ít gặp nhưng an toàn)
                ic->entries[0].offset = offset; 
            }
            return;
        }
    }

    // 2. Nếu chưa có, đẩy tất cả xuống và chèn vào đầu (loại bỏ phần tử cuối cùng)
    for (int i = IC_CAPACITY - 1; i > 0; --i) {
        ic->entries[i] = ic->entries[i - 1];
    }
    ic->entries[0].shape = shape;
    ic->entries[0].offset = offset;
}

// --- NEW_CLASS & NEW_INSTANCE (Giữ nguyên) ---

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
    regs[dst] = Value(state->heap.new_instance(class_val.as_class(), state->heap.get_empty_shape()));
    return ip;
}

// --- GET_PROP (Polymorphic IC) ---

[[gnu::always_inline]] static const uint8_t* impl_GET_PROP(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t dst = read_u16(ip);
    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    
    InlineCache* ic = get_inline_cache(ip);
    Value& obj = regs[obj_reg];
    
    if (obj.is_instance()) [[likely]] {
        instance_t inst = obj.as_instance();
        Shape* current_shape = inst->get_shape();

        // 1. FAST PATH: Duyệt tuyến tính qua 4 slots (Rất nhanh vì nằm gọn trong Cache Line CPU)
        // Dùng loop unrolling thủ công hoặc để compiler tự lo.
        for (int i = 0; i < IC_CAPACITY; ++i) {
            if (ic->entries[i].shape == current_shape) {
                // BINGO!
                regs[dst] = inst->get_field_at(ic->entries[i].offset);
                return ip;
            }
        }

        // 2. SLOW PATH: Cache Miss
        string_t name = constants[name_idx].as_string();
        int offset = current_shape->get_offset(name);

        if (offset != -1) {
            // Tìm thấy -> Update Cache (Move-To-Front)
            update_inline_cache(ic, current_shape, static_cast<uint32_t>(offset));
            
            regs[dst] = inst->get_field_at(offset);
            return ip;
        }

        // Fallback: Tìm method
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

// --- SET_PROP (Polymorphic IC) ---

[[gnu::always_inline]] static const uint8_t* impl_SET_PROP(const uint8_t* ip, Value* regs, Value* constants, VMState* state) {
    uint16_t obj_reg = read_u16(ip);
    uint16_t name_idx = read_u16(ip);
    uint16_t val_reg = read_u16(ip);
    
    InlineCache* ic = get_inline_cache(ip);
    Value& obj = regs[obj_reg];
    Value& val = regs[val_reg];
    
    if (obj.is_instance()) [[likely]] {
        instance_t inst = obj.as_instance();
        Shape* current_shape = inst->get_shape();

        // 1. FAST PATH: Inline Cache Hit
        for (int i = 0; i < IC_CAPACITY; ++i) {
            if (ic->entries[i].shape == current_shape) {
                // Gán giá trị siêu tốc
                inst->set_field_at(ic->entries[i].offset, val);
                
                // 🔥 [NEW] Write Barrier: Bảo vệ Old Gen
                // (Nếu inst là Già, val là Trẻ -> Ghi nhớ lại)
                state->heap.write_barrier(inst, val);
                
                return ip;
            }
        }

        // 2. SLOW PATH: Cache Miss
        string_t name = constants[name_idx].as_string();
        int offset = current_shape->get_offset(name);

        if (offset != -1) {
            // Case A: Field đã tồn tại -> Update Cache & Value
            update_inline_cache(ic, current_shape, static_cast<uint32_t>(offset));
            inst->set_field_at(offset, val);
            
            // 🔥 [NEW] Write Barrier
            state->heap.write_barrier(inst, val);
        } else {
            // Case B: Field mới -> Transition (Đổi Shape)
            Shape* next_shape = current_shape->get_transition(name);
            if (next_shape == nullptr) {
                next_shape = current_shape->add_transition(name, &state->heap);
            }

            inst->set_shape(next_shape);
            inst->get_fields_raw().push_back(val);
            
            // 🔥 [NEW] Write Barrier
            state->heap.write_barrier(inst, val);
        }
    } else {
        state->error("SET_PROP: Chỉ có thể gán thuộc tính cho Instance.");
        return impl_PANIC(ip, regs, constants, state);
    }
    return ip;
}

// --- Các hàm khác giữ nguyên ... ---
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
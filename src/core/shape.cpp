#include "core/objects/shape.h"
#include "memory/memory_manager.h"

namespace meow {

Shape* Shape::add_transition(string_t name, MemoryManager* heap) {
    // 1. Tạo Shape mới
    Shape* new_shape = heap->new_shape();
    
    // 2. Kế thừa toàn bộ offset của shape hiện tại
    new_shape->copy_from(this);
    
    // 3. Đăng ký offset cho thuộc tính mới
    new_shape->add_property(name);

    // 4. Lưu vào bảng chuyển đổi của shape hiện tại (Memoization)
    transitions_[name] = new_shape;
    
    return new_shape;
}

void Shape::trace(GCVisitor& visitor) const noexcept {
    // Trace các key trong property map để tránh GC thu hồi string
    for (auto& [key, _] : property_offsets_) {
        visitor.visit_object(key);
    }
    // Trace các transitions để giữ các shape con
    for (auto& [key, shape] : transitions_) {
        visitor.visit_object(key);
        visitor.visit_object(shape);
    }
}

}
// #include "core/objects/shape.h"
// #include "memory/memory_manager.h"

// namespace meow {

// Shape* Shape::add_transition(string_t name, MemoryManager* heap) {
//     // 1. Tạo Shape mới với dung lượng + 1
//     Shape* new_shape = heap->new_shape(num_fields_ + 1);
    
//     // 2. Kế thừa toàn bộ offset của shape hiện tại
//     new_shape->copy_offsets_from(this);
    
//     // 3. Đăng ký offset cho thuộc tính mới
//     new_shape->set_offset(name, num_fields_);
//     new_shape->parent_ = this;

//     // 4. Lưu vào bảng chuyển đổi của shape hiện tại (Memoization)
//     transitions_[name] = new_shape;
    
//     return new_shape;
// }

// void Shape::trace(GCVisitor& visitor) const noexcept {
//     // Trace các transitions để GC không thu hồi các shape con
//     for (auto& [key, shape] : transitions_) {
//         visitor.visit_object(key);
//         visitor.visit_object(shape);
//     }
// }

// }
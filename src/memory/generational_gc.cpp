#include "pch.h"
#include "memory/generational_gc.h"
#include <meow/value.h>
#include "runtime/execution_context.h"
#include <meow/core/meow_object.h>

namespace meow {

GenerationalGC::~GenerationalGC() noexcept {
    for (auto obj : young_gen_) delete obj;
    for (auto obj : old_gen_) delete obj;
}

void GenerationalGC::register_object(const MeowObject* object) {
    auto* obj = const_cast<MeowObject*>(object);
    // Mặc định là UNMARKED (Young)
    obj->gc_state = GCState::UNMARKED;
    young_gen_.push_back(obj);
}

// 🔥 [NEW] Rào chắn ghi: Cực nhanh nhờ so sánh Enum
void GenerationalGC::write_barrier(MeowObject* owner, Value value) noexcept {
    // 1. Chỉ quan tâm nếu owner là Già (OLD)
    if (owner->gc_state != GCState::OLD) return;

    // 2. Chỉ quan tâm nếu value là Object và nó Trẻ (Khác OLD)
    if (value.is_object()) {
        MeowObject* target = value.as_object();
        if (target && target->gc_state != GCState::OLD) {
            // Old trỏ Young -> Ghi nhớ để quét
            remembered_set_.push_back(owner);
        }
    }
}

size_t GenerationalGC::collect() noexcept {
    // 1. Mark Roots (Stack, Globals)
    context_->trace(*this);

    // [NEW] Nếu chỉ quét Young Gen, cần thêm Remembered Set làm Root
    if (old_gen_.size() <= old_gen_threshold_) {
        for (auto* old_obj : remembered_set_) {
            // Mark object già này để nó trace xuống con (Young) của nó
            if (old_obj) old_obj->trace(*this); 
        }
    }

    // 2. Sweep
    if (old_gen_.size() > old_gen_threshold_) {
        sweep_full();
        old_gen_threshold_ = std::max((size_t)100, old_gen_.size() * 2);
    } else {
        sweep_young();
    }

    // Clear remembered set sau khi GC xong (vì Young survivors đã lên Old)
    remembered_set_.clear();

    return young_gen_.size() + old_gen_.size();
}

void GenerationalGC::sweep_young() {
    std::vector<MeowObject*> survivors;
    survivors.reserve(young_gen_.size() / 2);

    for (auto obj : young_gen_) {
        if (obj->gc_state == GCState::MARKED) {
            // Sống sót -> Promote lên Old ngay lập tức (đơn giản hoá)
            obj->gc_state = GCState::OLD;
            old_gen_.push_back(obj);
        } else {
            // Chết -> Xoá
            delete obj;
        }
    }
    // Xoá danh sách Young cũ
    young_gen_.clear(); 
    // Reset survivors nếu cần (ở đây ta đã move hết lên Old)
}

void GenerationalGC::sweep_full() {
    // Dọn Old Gen
    std::vector<MeowObject*> old_survivors;
    old_survivors.reserve(old_gen_.size());
    
    for (auto obj : old_gen_) {
        // Với Old Gen, ta phải check xem có được mark không
        // Lưu ý: MarkSweepGC dùng gc_state == MARKED.
        // Ở đây, Old Gen mặc định gc_state == OLD.
        // Khi trace(), ta sẽ đổi nó thành MARKED?
        // -> Cần chỉnh lại logic mark_root một chút.
        
        // Logic thực tế: 
        // Trước khi Mark: Old Gen đang là OLD. Young là UNMARKED.
        // Khi Mark: Đổi thành MARKED (bất kể Old hay Young).
        
        if (obj->gc_state == GCState::MARKED) {
            obj->gc_state = GCState::OLD; // Reset về trạng thái Old
            old_survivors.push_back(obj);
        } else {
            // Không được mark -> Chết
            delete obj;
        }
    }
    old_gen_ = std::move(old_survivors);

    // Dọn Young Gen (giống logic trên)
    for (auto obj : young_gen_) {
        if (obj->gc_state == GCState::MARKED) {
            obj->gc_state = GCState::OLD; // Promote
            old_gen_.push_back(obj);
        } else {
            delete obj;
        }
    }
    young_gen_.clear();
}

void GenerationalGC::visit_value(param_t value) noexcept {
    if (value.is_object()) mark_root(value.as_object());
}

void GenerationalGC::visit_object(const MeowObject* object) noexcept {
    mark_root(const_cast<MeowObject*>(object));
}

void GenerationalGC::mark_root(MeowObject* object) {
    if (object == nullptr) return;
    
    // Nếu đã được Mark rồi thì thôi
    if (object->gc_state == GCState::MARKED) return;
    
    // Đánh dấu là MARKED (Dù trước đó là OLD hay UNMARKED)
    object->gc_state = GCState::MARKED;
    
    // Đệ quy
    object->trace(*this);
}

}

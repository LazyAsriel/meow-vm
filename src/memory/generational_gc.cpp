#include "pch.h"
#include "memory/generational_gc.h"
#include <meow/value.h>
#include "runtime/execution_context.h"
#include <meow/core/meow_object.h>

namespace meow {

GenerationalGC::~GenerationalGC() noexcept {
    // Giải phóng toàn bộ khi tắt VM
    for (auto obj : young_gen_) delete obj;
    for (auto obj : old_gen_) delete obj;
}

void GenerationalGC::register_object(const MeowObject* object) {
    // Object mới sinh ra luôn vào Young Gen (const_cast vì ta sở hữu nó)
    young_gen_.push_back(const_cast<MeowObject*>(object));
}

size_t GenerationalGC::collect() noexcept {
    // 1. MARKING PHASE
    // Duyệt từ rễ (Stack, Globals...)
    context_->trace(*this);

    // 2. SWEEPING PHASE
    // Nếu Old Gen đầy -> Dọn sạch sẽ (Major GC)
    // Nếu chưa -> Chỉ dọn lính mới (Minor GC - Rất nhanh)
    if (old_gen_.size() > old_gen_threshold_) {
        sweep_full();
        // Tăng ngưỡng linh động để tránh Major GC chạy liên tục
        old_gen_threshold_ = std::max((size_t)100, old_gen_.size() * 2);
    } else {
        sweep_young();
    }

    return young_gen_.size() + old_gen_.size();
}

// --- Logic Dọn Dẹp Young Gen (Minor GC) ---
void GenerationalGC::sweep_young() {
    std::vector<MeowObject*> survivors;
    survivors.reserve(young_gen_.size() / 2);

    for (auto obj : young_gen_) {
        if (obj->is_marked()) {
            obj->unmark();
            old_gen_.push_back(obj);
        } else {
            delete obj;
        }
    }
    
    young_gen_.clear();
    
    // 👇 [THÊM ĐOẠN NÀY] Rất quan trọng! 
    // Phải bỏ đánh dấu Old Gen, nếu không lần GC sau nó sẽ bị bỏ qua -> Crash
    for (auto obj : old_gen_) {
        obj->unmark();
    }
}

// --- Logic Dọn Dẹp Toàn Bộ (Major GC) ---
void GenerationalGC::sweep_full() {
    // 1. Lọc Old Gen
    std::vector<MeowObject*> old_survivors;
    old_survivors.reserve(old_gen_.size());
    
    for (auto obj : old_gen_) {
        if (obj->is_marked()) {
            obj->unmark(); // Quan trọng: Reset cờ
            old_survivors.push_back(obj);
        } else {
            delete obj;
        }
    }
    old_gen_ = std::move(old_survivors);

    // 2. Lọc Young Gen (Giống hệt sweep_young nhưng logic gộp vào đây)
    for (auto obj : young_gen_) {
        if (obj->is_marked()) {
            obj->unmark();
            old_gen_.push_back(obj); // Promote lên Old
        } else {
            delete obj;
        }
    }
    young_gen_.clear();
}

// --- Visitor Interface ---

void GenerationalGC::visit_value(param_t value) noexcept {
    // Nếu value là object thì đánh dấu nó
    if (value.is_object()) mark_root(value.as_object());
}

void GenerationalGC::visit_object(const MeowObject* object) noexcept {
    mark_root(const_cast<MeowObject*>(object));
}

void GenerationalGC::mark_root(MeowObject* object) {
    if (object == nullptr) return;
    
    // Nếu đã mark rồi thì dừng (tránh lặp vô tận trong cycle)
    if (object->is_marked()) return;
    
    // Đánh dấu (Set bit trong padding) - Siêu nhanh
    object->mark();
    
    // Đệ quy mark các con của nó
    object->trace(*this);
}

}
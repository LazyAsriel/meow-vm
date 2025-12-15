#include "pch.h"
#include "memory/mark_sweep_gc.h"
#include <meow/value.h>
#include <module/module_manager.h>
#include "runtime/execution_context.h"
#include "meow_heap.h"

namespace meow {
    
using namespace gc_flags;

MarkSweepGC::~MarkSweepGC() noexcept {
    while (head_) {
        ObjectMeta* next = head_->next_gc;
        MeowObject* obj = static_cast<MeowObject*>(heap::get_data(head_));
        std::destroy_at(obj);
        if (heap_) heap_->deallocate_raw(head_, sizeof(ObjectMeta) + head_->size);
        head_ = next;
    }
}

void MarkSweepGC::register_object(const MeowObject* object) {
    auto* meta = heap::get_meta(const_cast<MeowObject*>(object));
    meta->next_gc = head_;
    head_ = meta;
    meta->flags = 0;
    object_count_++;
}

void MarkSweepGC::register_permanent(const MeowObject* object) {
    auto* meta = heap::get_meta(const_cast<MeowObject*>(object));
    meta->flags = MARKED | PERMANENT; 
}

size_t MarkSweepGC::collect() noexcept {
    context_->trace(*this);
    module_manager_->trace(*this);
    ObjectMeta** curr = &head_;
    size_t survived = 0;

    while (*curr) {
        ObjectMeta* meta = *curr;
        
        if (meta->flags & PERMANENT) {
            curr = &meta->next_gc;
        }
        else if (meta->flags & MARKED) {
            meta->flags &= ~MARKED;
            curr = &meta->next_gc;
            survived++;
        } else {
            ObjectMeta* dead = meta;
            *curr = dead->next_gc;
            
            MeowObject* obj = static_cast<MeowObject*>(heap::get_data(dead));
            std::destroy_at(obj);
            heap_->deallocate_raw(dead, sizeof(ObjectMeta) + dead->size);
            
            object_count_--;
        }
    }

    return survived;
}

void MarkSweepGC::visit_value(param_t value) noexcept {
    if (value.is_object()) mark(value.as_object());
}

void MarkSweepGC::visit_object(const MeowObject* object) noexcept {
    mark(const_cast<MeowObject*>(object));
}

void MarkSweepGC::mark(MeowObject* object) {
    if (object == nullptr) return;
    auto* meta = heap::get_meta(object);
    
    if (meta->flags & MARKED) return;
    
    meta->flags |= MARKED;
    object->trace(*this);
}

}
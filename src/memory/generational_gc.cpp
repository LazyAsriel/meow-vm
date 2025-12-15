#include "pch.h"
#include "memory/generational_gc.h"
#include <meow/value.h>
#include "runtime/execution_context.h"
#include <meow/core/meow_object.h>
#include <module/module_manager.h>
#include "meow_heap.h"

namespace meow {

using namespace gc_flags;

static void clear_list(heap* h, ObjectMeta* head) {
    while (head) {
        ObjectMeta* next = head->next_gc;
        MeowObject* obj = static_cast<MeowObject*>(heap::get_data(head));
        std::destroy_at(obj);
        h->deallocate_raw(head, sizeof(ObjectMeta) + head->size);
        head = next;
    }
}

GenerationalGC::~GenerationalGC() noexcept {
    if (heap_) {
        clear_list(heap_, young_head_);
        clear_list(heap_, old_head_);
        clear_list(heap_, perm_head_);
    }
}

void GenerationalGC::register_object(const MeowObject* object) {
    auto* meta = heap::get_meta(const_cast<MeowObject*>(object));
    
    meta->next_gc = young_head_;
    young_head_ = meta;
    
    meta->flags = GEN_YOUNG;
    
    young_count_++;
}

void GenerationalGC::register_permanent(const MeowObject* object) {
    auto* meta = heap::get_meta(const_cast<MeowObject*>(object));
    
    meta->next_gc = perm_head_;
    perm_head_ = meta;
    
    meta->flags = GEN_OLD | PERMANENT | MARKED;
}

void GenerationalGC::write_barrier(MeowObject* owner, Value value) noexcept {
    auto* owner_meta = heap::get_meta(owner);
    
    if (!(owner_meta->flags & GEN_OLD)) return;

    if (value.is_object()) {
        MeowObject* target = value.as_object();
        if (target) {
            auto* target_meta = heap::get_meta(target);
            if (!(target_meta->flags & GEN_OLD)) {
                remembered_set_.push_back(owner);
            }
        }
    }
}

size_t GenerationalGC::collect() noexcept {
    context_->trace(*this);
    module_manager_->trace(*this);
    
    ObjectMeta* perm = perm_head_;
    while (perm) {
        MeowObject* obj = static_cast<MeowObject*>(heap::get_data(perm));
        if (obj->get_type() != ObjectType::STRING) {
            mark_object(obj);
        }
        perm = perm->next_gc;
    }

    if (old_count_ > old_gen_threshold_) {
        sweep_full();
        old_gen_threshold_ = std::max((size_t)100, old_count_ * 2);
    } else {
        sweep_young();
    }

    remembered_set_.clear();
    return young_count_ + old_count_;
}

void GenerationalGC::destroy_object(ObjectMeta* meta) {
    MeowObject* obj = static_cast<MeowObject*>(heap::get_data(meta));
    std::destroy_at(obj);
    heap_->deallocate_raw(meta, sizeof(ObjectMeta) + meta->size);
}

void GenerationalGC::sweep_young() {
    ObjectMeta** curr = &young_head_;
    size_t survived = 0;

    while (*curr) {
        ObjectMeta* meta = *curr;
        
        if (meta->flags & MARKED) {
            *curr = meta->next_gc; 
            meta->next_gc = old_head_;
            old_head_ = meta;
            meta->flags = GEN_OLD; 
            
            old_count_++;
            young_count_--;
        } else {
            ObjectMeta* dead = meta;
            *curr = dead->next_gc;
            
            destroy_object(dead);
            young_count_--;
        }
    }
}

void GenerationalGC::sweep_full() {
    ObjectMeta** curr_old = &old_head_;
    size_t old_survived = 0;
    while (*curr_old) {
        ObjectMeta* meta = *curr_old;
        if (meta->flags & MARKED) {
            meta->flags &= ~MARKED;
            curr_old = &meta->next_gc;
            old_survived++;
        } else {
            ObjectMeta* dead = meta;
            *curr_old = dead->next_gc;
            destroy_object(dead);
        }
    }
    old_count_ = old_survived;

    sweep_young(); 
}

void GenerationalGC::visit_value(param_t value) noexcept {
    if (value.is_object()) mark_object(value.as_object());
}

void GenerationalGC::visit_object(const MeowObject* object) noexcept {
    mark_object(const_cast<MeowObject*>(object));
}

void GenerationalGC::mark_object(MeowObject* object) {
    if (object == nullptr) return;
    
    auto* meta = heap::get_meta(object);
    
    if (meta->flags & MARKED) return;
    
    meta->flags |= MARKED;
    
    object->trace(*this);
}

}
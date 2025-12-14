#include "pch.h"
#include "memory/generational_gc.h"
#include <meow/value.h>
#include "runtime/execution_context.h"
#include <meow/core/meow_object.h>
#include <module/module_manager.h>
#include "meow_heap.h"

namespace meow {

GenerationalGC::~GenerationalGC() noexcept {
    if (heap_) {
        for (auto obj : young_gen_) heap_->destroy_dynamic(obj, obj->obj_size());
        for (auto obj : old_gen_) heap_->destroy_dynamic(obj, obj->obj_size());
        for (auto obj : permanent_gen_) {
            if (obj && obj->type != ObjectType::STRING) {
                heap_->destroy_dynamic(obj, obj->obj_size());
            }
        }
    }
}

void GenerationalGC::register_object(const MeowObject* object) {
    auto* obj = const_cast<MeowObject*>(object);
    obj->gc_state = GCState::UNMARKED;
    young_gen_.push_back(obj);
}

void GenerationalGC::register_permanent(const MeowObject* object) {
    auto* obj = const_cast<MeowObject*>(object);
    obj->gc_state = GCState::OLD; 
    permanent_gen_.push_back(obj);
}

void GenerationalGC::write_barrier(MeowObject* owner, Value value) noexcept {
    if (owner->gc_state != GCState::OLD) return;

    if (value.is_object()) {
        MeowObject* target = value.as_object();
        if (target && target->gc_state != GCState::OLD) {
            remembered_set_.push_back(owner);
        }
    }
}

size_t GenerationalGC::collect() noexcept {
    context_->trace(*this);
    module_manager_->trace(*this);

    if (old_gen_.size() <= old_gen_threshold_) {
        for (auto* old_obj : remembered_set_) {
            if (old_obj) old_obj->trace(*this); 
        }
    }

    if (old_gen_.size() > old_gen_threshold_) {
        sweep_full();
        old_gen_threshold_ = std::max((size_t)100, old_gen_.size() * 2);
    } else {
        sweep_young();
    }

    remembered_set_.clear();
    return young_gen_.size() + old_gen_.size();
}

void GenerationalGC::sweep_young() {
    std::vector<MeowObject*> survivors;
    survivors.reserve(young_gen_.size() / 2);

    for (auto obj : young_gen_) {
        if (obj->gc_state == GCState::MARKED) {
            obj->gc_state = GCState::OLD;
            old_gen_.push_back(obj);
        } else {
            if (heap_) heap_->destroy_dynamic(obj, obj->obj_size());
        }
    }
    young_gen_.clear(); 
}

void GenerationalGC::sweep_full() {
    std::vector<MeowObject*> old_survivors;
    old_survivors.reserve(old_gen_.size());
    
    for (auto obj : old_gen_) {
        if (obj->gc_state == GCState::MARKED) {
            obj->gc_state = GCState::OLD; 
            old_survivors.push_back(obj);
        } else {
            if (heap_) heap_->destroy_dynamic(obj, obj->obj_size());
        }
    }
    old_gen_ = std::move(old_survivors);

    for (auto obj : young_gen_) {
        if (obj->gc_state == GCState::MARKED) {
            obj->gc_state = GCState::OLD;
            old_gen_.push_back(obj);
        } else {
            if (heap_) heap_->destroy_dynamic(obj, obj->obj_size());
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
    
    if (object->gc_state == GCState::MARKED) return;
    
    object->gc_state = GCState::MARKED;
    
    object->trace(*this);
}

}

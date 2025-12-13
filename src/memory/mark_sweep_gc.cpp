#include "pch.h"
#include "memory/mark_sweep_gc.h"
#include <meow/value.h>
#include "runtime/execution_context.h"
#include "meow_heap.h"

namespace meow {

MarkSweepGC::~MarkSweepGC() noexcept {
    for (auto const& [obj, data] : metadata_) {
        if (heap_) heap_->destroy_dynamic(const_cast<MeowObject*>(obj), obj->obj_size());
    }
}

void MarkSweepGC::register_object(const MeowObject* object) {
    metadata_.emplace(object, GCMetadata{});
}

size_t MarkSweepGC::collect() noexcept {
    context_->trace(*this);

    for (auto it = metadata_.begin(); it != metadata_.end();) {
        const MeowObject* object = it->first;
        GCMetadata& data = it->second;

        if (data.is_marked_) {
            data.is_marked_ = false;
            ++it;
        } else {
            if (heap_) heap_->destroy_dynamic(const_cast<MeowObject*>(object), object->obj_size());
            
            it = metadata_.erase(it);
        }
    }

    return metadata_.size();
}

void MarkSweepGC::visit_value(param_t value) noexcept {
    if (value.is_object()) mark(value.as_object());
}

void MarkSweepGC::visit_object(const MeowObject* object) noexcept {
    mark(object);
}

void MarkSweepGC::mark(const MeowObject* object) {
    if (object == nullptr) return;
    auto it = metadata_.find(object);
    if (it == metadata_.end() || it->second.is_marked_) return;
    it->second.is_marked_ = true;
    object->trace(*this);
}

}
#include "memory/mark_sweep_gc.h"
#include "core/value.h"
#include "runtime/builtin_registry.h"
#include "runtime/execution_context.h"
#include <print>

namespace meow {

MarkSweepGC::~MarkSweepGC() noexcept {
    std::cout << "[destroy] Đang xử lí các object khi hủy GC" << std::endl;

    // speed doesn't matter for debug here
    // std::println("[destroy] Đang xử lí các object khi hủy GC");
    for (auto const& [obj, data] : metadata_) {
        delete obj;
    }
}

void MarkSweepGC::register_object(const MeowObject* object) {
    std::println("[register] Đang đăng kí object: {:p}", static_cast<const void*>(object));
    metadata_.emplace(object, GCMetadata{});
}

size_t MarkSweepGC::collect() noexcept {
    std::cout << "[collect] Đang collect các object" << std::endl;
    // std::println("[collect] Đang collect các object");

    context_->trace(*this);

    for (auto it = metadata_.begin(); it != metadata_.end();) {
        const MeowObject* object = it->first;
        GCMetadata& data = it->second;

        if (data.is_marked_) {
            data.is_marked_ = false;
            ++it;
        } else {
            delete object;
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
#pragma once

#include "pch.h"
#include <meow/definitions.h>
#include <meow/memory/garbage_collector.h>
#include <meow/memory/gc_visitor.h>

namespace meow {
struct ExecutionContext;

class GenerationalGC : public GarbageCollector, public GCVisitor {
public:
    explicit GenerationalGC(ExecutionContext* context) noexcept : context_(context) {}
    ~GenerationalGC() noexcept override;

    // --- Collector ---
    void register_object(const MeowObject* object) override;
    size_t collect() noexcept override;

    // --- Visitor ---
    void visit_value(param_t value) noexcept override;
    void visit_object(const MeowObject* object) noexcept override;

private:
    ExecutionContext* context_ = nullptr;

    // Quản lý 2 thế hệ bằng Vector (Duyệt tuần tự cực nhanh)
    std::vector<MeowObject*> young_gen_;
    std::vector<MeowObject*> old_gen_;

    // Ngưỡng để kích hoạt Major GC (Dọn toàn bộ)
    size_t old_gen_threshold_ = 100; 

    // Không cần std::unordered_set nữa!
    // Trạng thái mark nằm ngay trong MeowObject.

    void mark_root(MeowObject* object);
    void sweep_young();
    void sweep_full();
};
}
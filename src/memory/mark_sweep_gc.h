#pragma once

#include "pch.h"
#include <meow/definitions.h>
#include <meow/memory/garbage_collector.h>
#include <meow/memory/gc_visitor.h>
#include "meow_heap.h"

namespace meow {
struct ExecutionContext;
class ModuleManager;

class MarkSweepGC : public GarbageCollector, public GCVisitor {
public:
    explicit MarkSweepGC(ExecutionContext* context) noexcept : context_(context) {}
    ~MarkSweepGC() noexcept override;

    void register_object(const MeowObject* object) override;
    void register_permanent(const MeowObject* object) override;
    size_t collect() noexcept override;

    void visit_value(param_t value) noexcept override;
    void visit_object(const MeowObject* object) noexcept override;

    void set_module_manager(ModuleManager* mm) { module_manager_ = mm; }
private:
    ExecutionContext* context_ = nullptr;
    ModuleManager* module_manager_ = nullptr;
    
    ObjectMeta* head_ = nullptr;
    size_t object_count_ = 0;

    void mark(MeowObject* object);
};
}
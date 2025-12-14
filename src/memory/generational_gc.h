#pragma once

#include "pch.h"
#include <meow/definitions.h>
#include <meow/memory/garbage_collector.h>
#include <meow/memory/gc_visitor.h>

namespace meow {
struct ExecutionContext;
class ModuleManager;

class GenerationalGC : public GarbageCollector, public GCVisitor {
public:
    explicit GenerationalGC(ExecutionContext* context) noexcept : context_(context) {}
    ~GenerationalGC() noexcept override;

    void register_object(const MeowObject* object) override;
    void register_permanent(const MeowObject* object) override;
    size_t collect() noexcept override;

    void write_barrier(MeowObject* owner, Value value) noexcept override;

    void visit_value(param_t value) noexcept override;
    void visit_object(const MeowObject* object) noexcept override;

    void set_module_manager(ModuleManager* mm) { module_manager_ = mm; }

private:
    ExecutionContext* context_ = nullptr;
    ModuleManager* module_manager_ = nullptr;

    std::vector<MeowObject*> young_gen_;
    std::vector<MeowObject*> old_gen_;
    std::vector<MeowObject*> permanent_gen_;
    
    std::vector<MeowObject*> remembered_set_;

    size_t old_gen_threshold_ = 100;

    void mark_root(MeowObject* object);
    void sweep_young();
    void sweep_full();
};
}
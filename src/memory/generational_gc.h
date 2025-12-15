#pragma once

#include "pch.h"
#include <meow/definitions.h>
#include <meow/memory/garbage_collector.h>
#include <meow/memory/gc_visitor.h>
#include <vector> 
#include "meow_heap.h"

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

    ObjectMeta* young_head_ = nullptr;
    ObjectMeta* old_head_   = nullptr;
    ObjectMeta* perm_head_  = nullptr;
    
    std::vector<MeowObject*> remembered_set_;

    size_t young_count_ = 0;
    size_t old_count_ = 0;
    size_t old_gen_threshold_ = 100;

    void mark_object(MeowObject* object);
    
    void sweep_young(); 
    void sweep_full();
    
    void destroy_object(ObjectMeta* meta);
};
}
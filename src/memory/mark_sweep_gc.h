#pragma once

#include "pch.h"
#include <meow/definitions.h>
#include <meow/memory/garbage_collector.h>
#include <meow/memory/gc_visitor.h>

namespace meow {
struct ExecutionContext;
struct GCMetadata {
    bool is_marked_ = false;
};

class MarkSweepGC : public GarbageCollector, public GCVisitor {
public:
    explicit MarkSweepGC(ExecutionContext* context) noexcept : context_(context) {}
    ~MarkSweepGC() noexcept override;

    // -- Collector ---
    void register_object(const MeowObject* object) override;
    size_t collect() noexcept override;

    // --- Visitor ---
    void visit_value(param_t value) noexcept override;
    void visit_object(const MeowObject* object) noexcept override;
private:
    std::unordered_map<const MeowObject*, GCMetadata> metadata_;
    ExecutionContext* context_ = nullptr;

    void mark(const MeowObject* object);
};
}
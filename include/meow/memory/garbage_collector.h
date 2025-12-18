#pragma once

#include <cstddef>
#include <meow/value.h>
#include "meow_heap.h"

namespace meow {
struct MeowObject;

namespace gc_flags {
    static constexpr uint32_t GEN_YOUNG = 0;       // Bit 0 = 0
    static constexpr uint32_t GEN_OLD   = 1 << 0;  // Bit 0 = 1
    static constexpr uint32_t MARKED    = 1 << 1;  // Bit 1 = 1
    static constexpr uint32_t PERMANENT = 1 << 2;  // Bit 2 = 1
}

class GarbageCollector {
protected:
    meow::heap* heap_ = nullptr;
public:
    virtual ~GarbageCollector() noexcept = default;
    void set_heap(meow::heap* h) noexcept { heap_ = h; }

    virtual void register_object(const MeowObject* object) = 0;
    virtual void register_permanent(const MeowObject* object) = 0;
    virtual size_t collect() noexcept = 0;
    virtual void write_barrier(MeowObject*, Value) noexcept {}
};
}
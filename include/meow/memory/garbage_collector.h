#pragma once

#include <cstddef>
#include <meow/value.h>

namespace meow {
struct MeowObject;
class heap;

class GarbageCollector {
protected:
    meow::heap* heap_ = nullptr;
public:
    virtual ~GarbageCollector() noexcept = default;
    void set_heap(meow::heap* h) noexcept { heap_ = h; }

    virtual void register_object(const MeowObject* object) = 0;
    virtual size_t collect() noexcept = 0;
    virtual void write_barrier(MeowObject*, Value) noexcept {}
};
}
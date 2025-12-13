#pragma once

#include <cstddef>
#include <meow/value.h>

namespace meow {
struct MeowObject;

class GarbageCollector {
   public:
    virtual ~GarbageCollector() noexcept = default;

    virtual void register_object(const MeowObject* object) = 0;
    virtual size_t collect() noexcept = 0;
    virtual void write_barrier(MeowObject* owner, Value value) noexcept {}
};
}
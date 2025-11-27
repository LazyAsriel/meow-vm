#pragma once

#include "common/definitions.h"

namespace meow {
class Value;
struct MeowObject;
}

namespace meow {
struct GCVisitor {
    virtual ~GCVisitor() = default;
    virtual void visit_value(param_t value) noexcept = 0;
    virtual void visit_object(const MeowObject* object) noexcept = 0;
};
}
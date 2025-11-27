#pragma once

#include "common/pch.h"
#include "core/objects/string.h"
#include "common/definitions.h"
#include "core/value.h"
#include "memory/gc_visitor.h"

namespace meow {
struct BuiltinRegistry {
    std::unordered_map<string_t, std::unordered_map<string_t, Value>> methods;
    std::unordered_map<string_t, std::unordered_map<string_t, Value>> getters;

    inline void trace(GCVisitor& visitor) const noexcept {
        for (const auto& [name, method] : methods) {
            visitor.visit_object(name);
            for (const auto& [key, value] : method) {
                visitor.visit_object(key);
                visitor.visit_value(value);
            }
        }

        for (const auto& [name, getter] : getters) {
            visitor.visit_object(name);
            for (const auto& [key, value] : getter) {
                visitor.visit_object(key);
                visitor.visit_value(value);
            }
        }
    }
};
}
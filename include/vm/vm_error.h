#pragma once

#include "common/pch.h"

namespace meow {
struct VMError : public std::runtime_error {
    explicit VMError(const std::string& message) : std::runtime_error(message) {}
};
}
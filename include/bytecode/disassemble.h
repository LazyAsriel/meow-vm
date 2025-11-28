#pragma once

#include "common/pch.h"

namespace meow { class Chunk; }

namespace meow {
    std::string disassemble_chunk(const Chunk& chunk) noexcept;
}
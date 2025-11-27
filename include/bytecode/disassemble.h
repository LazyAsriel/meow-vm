#pragma once

#include "common/pch.h"

namespace meow {
class Chunk;
std::string disassemble_chunk(const Chunk& chunk) noexcept;
}
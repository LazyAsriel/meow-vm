#pragma once

#include "pch.h"
#include <string>
#include <utility>

namespace meow { 
    class Chunk; 
}

namespace meow {

    /**
     * @brief Disassemble toàn bộ chunk thành chuỗi (phiên bản tối ưu).
     */
    std::string disassemble_chunk(const Chunk& chunk, const char* name = nullptr) noexcept;

    /**
     * @brief Disassemble một instruction.
     * @return Pair: {Chuỗi kết quả, Offset của lệnh tiếp theo}
     */
    std::pair<std::string, size_t> disassemble_instruction(const Chunk& chunk, size_t offset) noexcept;

    /**
     * @brief Context Disassembly: In ra vùng code xung quanh IP để debug lỗi crash.
     * Tự động căn chỉnh (align) instruction để không in rác.
     */
    std::string disassemble_around(const Chunk& chunk, size_t ip, int context_lines = 10) noexcept;
}
#include <vector>
#include <cstddef>
#include <cstdint>
#include "meow/bytecode/op_codes.h"

namespace meow::jit::analysis {

    struct LoopInfo {
        size_t start_ip;
        size_t end_ip;
    };

    // Hàm quét bytecode để tìm loops
    std::vector<LoopInfo> find_loops(const uint8_t* bytecode, size_t len) {
        std::vector<LoopInfo> loops;
        size_t ip = 0;
        
        while (ip < len) {
            OpCode op = static_cast<OpCode>(bytecode[ip]);
            ip++; 

            // Logic skip operands (tương tự disassembler)
            // ... (Cần implement logic đọc độ dài từng opcode)
            // Tạm thời bỏ qua vì cần bảng OpCodeLength.
            
            // Nếu gặp JUMP ngược -> Phát hiện loop
        }
        return loops;
    }

} // namespace meow::jit::analysis
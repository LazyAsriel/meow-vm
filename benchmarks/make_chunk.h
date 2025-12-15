#pragma once

#include "pch.h"
#include <meow/bytecode/op_codes.h>
#include <meow/bytecode/chunk.h>

namespace meow {
// Chunk create_vm_chunk(int64_t LIMIT) {
//     Chunk chunk;
    
//     chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT)); chunk.write_u16(0); chunk.write_u64(0);
//     chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT)); chunk.write_u16(1); chunk.write_u64(0);
//     chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT)); chunk.write_u16(2); chunk.write_u64(1);
//     chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT)); chunk.write_u16(3); chunk.write_u64(static_cast<uint64_t>(LIMIT)); 

//     size_t loop_start = chunk.get_code_size();
//     chunk.write_byte(static_cast<uint8_t>(OpCode::ADD)); chunk.write_u16(0); chunk.write_u16(0); chunk.write_u16(2);
//     chunk.write_byte(static_cast<uint8_t>(OpCode::ADD)); chunk.write_u16(1); chunk.write_u16(1); chunk.write_u16(2);
//     chunk.write_byte(static_cast<uint8_t>(OpCode::LT));  chunk.write_u16(4); chunk.write_u16(1); chunk.write_u16(3);
//     chunk.write_byte(static_cast<uint8_t>(OpCode::JUMP_IF_TRUE)); chunk.write_u16(4); chunk.write_u16(static_cast<uint16_t>(loop_start));
//     chunk.write_byte(static_cast<uint8_t>(OpCode::HALT));

//     return chunk;
// }

Chunk create_vm_chunk(int64_t LIMIT) {
    Chunk chunk;
    
    // --- SETUP (Chạy 1 lần, giữ nguyên u16 để an toàn) ---
    // R0 = sum = 0
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT)); chunk.write_u16(0); chunk.write_u64(0);
    // R1 = counter = 0
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT)); chunk.write_u16(1); chunk.write_u64(0);
    // R2 = step = 1
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT)); chunk.write_u16(2); chunk.write_u64(1);
    // R3 = limit = 10,000,000
    chunk.write_byte(static_cast<uint8_t>(OpCode::LOAD_INT)); chunk.write_u16(3); chunk.write_u64(static_cast<uint64_t>(LIMIT)); 

    // --- LOOP START (Hot Path - Tối ưu nén lệnh) ---
    size_t loop_start = chunk.get_code_size();

    // 1. ADD_B R0, R0, R2 (sum += step)
    // Cấu trúc: [OP_ADD_B] [dst:1] [r1:1] [r2:1] -> Tổng 4 bytes
    chunk.write_byte(static_cast<uint8_t>(OpCode::ADD_B)); 
    chunk.write_byte(0); chunk.write_byte(0); chunk.write_byte(2);

    // 2. ADD_B R1, R1, R2 (counter += step)
    chunk.write_byte(static_cast<uint8_t>(OpCode::ADD_B)); 
    chunk.write_byte(1); chunk.write_byte(1); chunk.write_byte(2);

    // 3. LT_B R4, R1, R3 (counter < limit ?)
    chunk.write_byte(static_cast<uint8_t>(OpCode::LT_B));  
    chunk.write_byte(4); chunk.write_byte(1); chunk.write_byte(3);

    // 4. JUMP_IF_TRUE_B R4, loop_start
    // Cấu trúc: [OP_JUMP_B] [cond:1] [offset:2] -> Tổng 4 bytes
    chunk.write_byte(static_cast<uint8_t>(OpCode::JUMP_IF_TRUE_B)); 
    chunk.write_byte(4); 
    chunk.write_u16(static_cast<uint16_t>(loop_start));

    // HALT
    chunk.write_byte(static_cast<uint8_t>(OpCode::HALT));
    return chunk;
}
}
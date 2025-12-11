#pragma once

#include <vector>
#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <stdexcept>
#include <unordered_map>
#include <array>
#include <bit> 

#include "core/value.h"
#include "bytecode/op_codes.h"
// 👇 Include chuẩn, không trick
#include "meow_nanbox_layout.h" 

namespace meow {

class JitCompiler {
private:
    uint8_t* code_buffer_ = nullptr;
    size_t capacity_ = 1024 * 64; 
    size_t size_ = 0;
    
    struct JumpFixup {
        size_t jump_instruction_pos;
        size_t target_bytecode_offset;
        bool is_short; 
    };
    std::vector<JumpFixup> fixups_;
    std::unordered_map<size_t, size_t> bytecode_to_native_map_;

    static constexpr int CPU_RBX = 3;
    static constexpr int CPU_R12 = 12;
    static constexpr int CPU_R13 = 13;
    static constexpr int CPU_R14 = 14;
    static constexpr int CPU_R15 = 15;

    // Helper đơn giản, constexpr để inline tốt
    [[nodiscard]] constexpr int map_vm_reg_to_cpu(int vm_reg) const noexcept {
        switch (vm_reg) {
            case 0: return CPU_RBX;
            case 1: return CPU_R12;
            case 2: return CPU_R13;
            case 3: return CPU_R14;
            case 4: return CPU_R15;
            default: return -1;
        }
    }

public:
    JitCompiler() {
        code_buffer_ = (uint8_t*)mmap(nullptr, capacity_, 
                                      PROT_READ | PROT_WRITE | PROT_EXEC, 
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (code_buffer_ == MAP_FAILED) throw std::runtime_error("JIT mmap failed");
    }

    ~JitCompiler() {
        if (code_buffer_) munmap(code_buffer_, capacity_);
    }

    // --- Emit Functions ---

    void emit(uint8_t b) { code_buffer_[size_++] = b; }
    
    void emit_u32(uint32_t v) { 
        std::memcpy(code_buffer_ + size_, &v, 4); 
        size_ += 4; 
    }
    
    void emit_u64(uint64_t v) { 
        std::memcpy(code_buffer_ + size_, &v, 8); 
        size_ += 8; 
    }

    void emit_rex(bool w, bool r, bool x, bool b) {
        uint8_t rex = 0x40;
        if (w) rex |= 0x08;
        if (r) rex |= 0x04;
        if (x) rex |= 0x02;
        if (b) rex |= 0x01;
        emit(rex);
    }

    void emit_mov_reg_reg(int dst, int src) {
        bool r12_15_dst = dst >= 8;
        bool r12_15_src = src >= 8;
        emit_rex(true, r12_15_dst, false, r12_15_src);
        emit(0x8B); 
        emit(0xC0 | ((dst & 7) << 3) | (src & 7));
    }

    void emit_add_reg_reg(int dst, int src) {
        bool r12_15_dst = dst >= 8;
        bool r12_15_src = src >= 8;
        emit_rex(true, r12_15_dst, false, r12_15_src);
        emit(0x01); 
        emit(0xC0 | ((src & 7) << 3) | (dst & 7));
    }

    void emit_cmp_reg_reg(int lhs, int rhs) {
        bool r12_15_lhs = lhs >= 8;
        bool r12_15_rhs = rhs >= 8;
        emit_rex(true, r12_15_lhs, false, r12_15_rhs);
        emit(0x39);
        emit(0xC0 | ((rhs & 7) << 3) | (lhs & 7));
    }

    // Hàm này rất quan trọng: 
    // Nếu 'imm' truyền vào là hằng số compile-time, trình biên dịch sẽ tối ưu nhánh if/else ngay lập tức.
    void emit_mov_reg_imm(int dst, uint64_t imm) {
        bool r12_15 = dst >= 8;
        if (imm <= 0xFFFFFFFF) {
            // Tối ưu: Dùng mov r32, imm32 (nhanh hơn và code ngắn hơn)
            if (r12_15) emit(0x41); 
            emit(0xB8 | (dst & 7));
            emit_u32((uint32_t)imm);
        } else {
            // Full 64-bit move
            emit_rex(true, false, false, r12_15);
            emit(0xB8 | (dst & 7));
            emit_u64(imm);
        }
    }

    void emit_load_mem_to_reg(int cpu_dst, int vm_src_idx) {
        bool r12_15 = cpu_dst >= 8;
        emit_rex(true, r12_15, false, false);
        emit(0x8B); 
        emit(0x80 | ((cpu_dst & 7) << 3) | 7); 
        emit_u32(vm_src_idx * 8);
    }

    void emit_store_reg_to_mem(int vm_dst_idx, int cpu_src) {
        bool r12_15 = cpu_src >= 8;
        emit_rex(true, r12_15, false, false);
        emit(0x89);
        emit(0x80 | ((cpu_src & 7) << 3) | 7);
        emit_u32(vm_dst_idx * 8);
    }

    void emit_sar(int reg, uint8_t bits) {
        bool r12_15 = reg >= 8;
        emit_rex(true, false, false, r12_15);
        emit(0xC1); 
        emit(0xF8 | (reg & 7));
        emit(bits);
    }

    void emit_shl(int reg, uint8_t bits) {
        bool r12_15 = reg >= 8;
        emit_rex(true, false, false, r12_15);
        emit(0xC1);
        emit(0xE0 | (reg & 7));
        emit(bits);
    }

    using JitFunc = void (*)(Value* regs);

    JitFunc compile(const uint8_t* bytecode, size_t len) {
        size_t pc = 0;

        // --- PROLOGUE ---
        emit(0x53);             // push rbx
        emit(0x41); emit(0x54); // push r12
        emit(0x41); emit(0x55); // push r13
        emit(0x41); emit(0x56); // push r14
        emit(0x41); emit(0x57); // push r15

        // Load & Unbox
        for (int i = 0; i <= 4; ++i) {
            int cpu_reg = map_vm_reg_to_cpu(i);
            emit_load_mem_to_reg(cpu_reg, i);
            emit_shl(cpu_reg, 16);
            emit_sar(cpu_reg, 16);
        }

        while (pc < len) {
            bytecode_to_native_map_[pc] = size_;
            OpCode op = static_cast<OpCode>(bytecode[pc]);
            pc++; 

            switch (op) {
                case OpCode::LOAD_INT: {
                    uint16_t dst = *reinterpret_cast<const uint16_t*>(bytecode + pc); pc += 2;
                    int64_t val = *reinterpret_cast<const int64_t*>(bytecode + pc); pc += 8;
                    int reg = map_vm_reg_to_cpu(dst);
                    if (reg != -1) emit_mov_reg_imm(reg, val);
                    break;
                }

                case OpCode::ADD: 
                case OpCode::ADD_B: {
                    uint16_t dst, r1, r2;
                    if (op == OpCode::ADD) {
                        dst = *reinterpret_cast<const uint16_t*>(bytecode + pc); pc += 2;
                        r1 = *reinterpret_cast<const uint16_t*>(bytecode + pc); pc += 2;
                        r2 = *reinterpret_cast<const uint16_t*>(bytecode + pc); pc += 2;
                    } else {
                        dst = bytecode[pc++]; r1 = bytecode[pc++]; r2 = bytecode[pc++];
                    }

                    int reg_dst = map_vm_reg_to_cpu(dst);
                    int reg_r1  = map_vm_reg_to_cpu(r1);
                    int reg_r2  = map_vm_reg_to_cpu(r2);

                    if (reg_dst != -1 && reg_r1 != -1 && reg_r2 != -1) {
                        if (reg_dst != reg_r1) emit_mov_reg_reg(reg_dst, reg_r1);
                        emit_add_reg_reg(reg_dst, reg_r2);
                    }
                    break;
                }

                case OpCode::LT:
                case OpCode::LT_B: {
                    uint16_t dst, r1, r2;
                    if (op == OpCode::LT) {
                        dst = *reinterpret_cast<const uint16_t*>(bytecode + pc); pc += 2;
                        r1 = *reinterpret_cast<const uint16_t*>(bytecode + pc); pc += 2;
                        r2 = *reinterpret_cast<const uint16_t*>(bytecode + pc); pc += 2;
                    } else {
                        dst = bytecode[pc++]; r1 = bytecode[pc++]; r2 = bytecode[pc++];
                    }

                    int reg_dst = map_vm_reg_to_cpu(dst);
                    int reg_r1  = map_vm_reg_to_cpu(r1);
                    int reg_r2  = map_vm_reg_to_cpu(r2);

                    bool fused = false;
                    if (pc < len) {
                        OpCode next_op = static_cast<OpCode>(bytecode[pc]);
                        if (next_op == OpCode::JUMP_IF_TRUE_B) {
                            uint8_t jump_reg = bytecode[pc + 1];
                            if (jump_reg == dst) {
                                pc += 2; 
                                uint16_t target = *reinterpret_cast<const uint16_t*>(bytecode + pc); pc += 2;

                                if (reg_r1 != -1 && reg_r2 != -1) {
                                    emit_cmp_reg_reg(reg_r1, reg_r2);
                                    
                                    if (bytecode_to_native_map_.count(target)) {
                                        size_t target_addr = bytecode_to_native_map_[target];
                                        int64_t diff = (int64_t)target_addr - (int64_t)(size_ + 2);
                                        if (diff >= -128 && diff <= 127) {
                                            emit(0x7C); emit((uint8_t)diff);
                                        } else {
                                            emit(0x0F); emit(0x8C);
                                            int32_t rel32 = (int32_t)(target_addr - (size_ + 4));
                                            emit_u32(rel32);
                                        }
                                    } else {
                                        emit(0x0F); emit(0x8C); 
                                        fixups_.push_back({size_, (size_t)target, false});
                                        emit_u32(0);
                                    }
                                    fused = true;
                                }
                            }
                        }
                    }

                    if (!fused && reg_dst != -1 && reg_r1 != -1 && reg_r2 != -1) {
                        emit_cmp_reg_reg(reg_r1, reg_r2);
                        emit(0x0F); emit(0x9C); emit(0xC0); 
                        bool r12_15_dst = reg_dst >= 8;
                        emit_rex(true, r12_15_dst, false, false);
                        emit(0x0F); emit(0xB6); 
                        emit(0xC0 | ((reg_dst & 7) << 3)); 
                    }
                    break;
                }

                case OpCode::JUMP_IF_TRUE:
                case OpCode::JUMP_IF_TRUE_B: {
                    uint16_t reg, target;
                    if (op == OpCode::JUMP_IF_TRUE) {
                        reg = *reinterpret_cast<const uint16_t*>(bytecode + pc); pc += 2;
                        target = *reinterpret_cast<const uint16_t*>(bytecode + pc); pc += 2;
                    } else {
                        reg = bytecode[pc++];
                        target = *reinterpret_cast<const uint16_t*>(bytecode + pc); pc += 2;
                    }

                    int cpu_reg = map_vm_reg_to_cpu(reg);
                    emit_rex(true, cpu_reg >= 8, false, cpu_reg >= 8);
                    emit(0x85); 
                    emit(0xC0 | ((cpu_reg & 7) << 3) | (cpu_reg & 7)); 

                    if (bytecode_to_native_map_.count(target)) {
                        size_t target_addr = bytecode_to_native_map_[target];
                        int64_t diff = (int64_t)target_addr - (int64_t)(size_ + 2);
                        if (diff >= -128 && diff <= 127) {
                            emit(0x75); emit((uint8_t)diff);
                        } else {
                            emit(0x0F); emit(0x85); 
                            int32_t rel32 = (int32_t)(target_addr - (size_ + 4));
                            emit_u32(rel32);
                        }
                    } else {
                        emit(0x0F); emit(0x85); 
                        fixups_.push_back({size_, (size_t)target, false});
                        emit_u32(0);
                    }
                    break;
                }

                case OpCode::HALT: {
                    for (int i = 0; i <= 4; ++i) {
                        int cpu_reg = map_vm_reg_to_cpu(i);
                        
                        // 🔥 ĐIỂM SỬA CHÍNH: Gọi trực tiếp consteval function
                        // Không qua biến trung gian, không enum, không static variable.
                        // Trình biên dịch sẽ inline giá trị này thành constant literal.
                        // NanboxLayout::make_tag(2) trả về 0x7FFA... ngay lúc compile.
                        emit_mov_reg_imm(0, NanboxLayout::make_tag(2)); 
                        
                        bool r12_15 = cpu_reg >= 8;
                        emit_rex(true, r12_15, false, false);
                        emit(0x09); 
                        emit(0xC0 | (0 << 3) | (cpu_reg & 7)); 
                        emit_store_reg_to_mem(i, cpu_reg);
                    }
                    emit(0x41); emit(0x5F); 
                    emit(0x41); emit(0x5E); 
                    emit(0x41); emit(0x5D); 
                    emit(0x41); emit(0x5C); 
                    emit(0x5B); 
                    emit(0xC3); 
                    break;
                }

                default:
                    emit(0xC3); break;
            }
        }

        for (auto& fix : fixups_) {
            size_t target_native = bytecode_to_native_map_[fix.target_bytecode_offset];
            size_t src_native = fix.jump_instruction_pos + 4;
            int32_t rel = (int32_t)(target_native - src_native);
            std::memcpy(code_buffer_ + fix.jump_instruction_pos, &rel, 4);
        }

        return (JitFunc)code_buffer_;
    }
};

} // namespace meow
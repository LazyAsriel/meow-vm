#!/bin/bash

# Tạo cấu trúc thư mục
mkdir -p src/jit/x64

echo "Đang tạo src/jit/x64/common.h..."
cat << 'EOF' > src/jit/x64/common.h
#pragma once
#include <cstdint>

namespace meow::jit::x64 {

// Các thanh ghi x64 (theo thứ tự mã hóa phần cứng)
enum Reg : uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8 = 8,  R9 = 9,  R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    INVALID_REG = 0xFF
};

// Điều kiện nhảy (Jcc) và so sánh (SETcc)
enum Condition : uint8_t {
    O = 0x0, NO = 0x1, B = 0x2, AE = 0x3,
    E = 0x4, NE = 0x5, BE = 0x6, A = 0x7,
    S = 0x8, NS = 0x9, P = 0xA, NP = 0xB,
    L = 0xC, GE = 0xD, LE = 0xE, G = 0xF
};

} // namespace meow::jit::x64
EOF

echo "Đang tạo src/jit/x64/emitter.h..."
cat << 'EOF' > src/jit/x64/emitter.h
#pragma once
#include "common.h"
#include <vector>
#include <cstddef>
#include <cstdint>

namespace meow::jit::x64 {

class Emitter {
private:
    uint8_t* buffer_;
    size_t capacity_;
    size_t size_;

public:
    explicit Emitter(uint8_t* buffer, size_t capacity);

    // --- Core ---
    void emit_byte(uint8_t b);
    void emit_u32(uint32_t v);
    void emit_u64(uint64_t v);
    
    // --- x64 Encoding ---
    void emit_rex(bool w, bool r, bool x, bool b);
    void emit_modrm(int mode, int reg, int rm);

    // --- Moves ---
    void mov_reg_imm(Reg dst, uint64_t imm);       // MOV r64, imm64
    void mov_reg_reg(Reg dst, Reg src);            // MOV r64, r64
    void mov_reg_mem(Reg dst, Reg base, int32_t disp); // Load: MOV r64, [base + disp]
    void mov_mem_reg(Reg base, int32_t disp, Reg src); // Store: MOV [base + disp], r64

    // --- ALU & Logic ---
    // Opcode mapping: ADD(0x01), SUB(0x29), AND(0x21), OR(0x09), XOR(0x31), CMP(0x39)
    void alu_reg_reg(uint8_t opcode, Reg dst, Reg src); 
    void imul_reg_reg(Reg dst, Reg src);           // IMUL r64, r64
    void idiv_reg(Reg src);                        // IDIV r64 (chia RDX:RAX)
    void cqo();                                    // CDQ/CQO: Sign extend RAX -> RDX

    // --- Shifts ---
    // Shift dst theo CL. arithmetic=true (SAR), false (SHL/SHR)
    void shift_cl(Reg dst, bool right, bool arithmetic);
    // Shift dst theo hằng số
    void shift_imm(Reg dst, uint8_t imm, bool right, bool arithmetic);

    // --- Unary ---
    void neg(Reg dst);
    void not_op(Reg dst);

    // --- Control Flow ---
    void jcc(Condition cond, int32_t rel_offset);
    void jmp(int32_t rel_offset);
    void setcc(Condition cond, Reg dst);           // SETcc r8

    // --- Stack ---
    void push(Reg r);
    void pop(Reg r);
    void ret();

    // --- Utils ---
    [[nodiscard]] size_t cursor() const { return size_; }
    void patch_u32(size_t offset, uint32_t value);
};

} // namespace meow::jit::x64
EOF

echo "Đang tạo src/jit/x64/emitter.cpp..."
cat << 'EOF' > src/jit/x64/emitter.cpp
#include "emitter.h"
#include <cstring>
#include <stdexcept>

namespace meow::jit::x64 {

Emitter::Emitter(uint8_t* buffer, size_t capacity) 
    : buffer_(buffer), capacity_(capacity), size_(0) {}

void Emitter::emit_byte(uint8_t b) {
    if (size_ >= capacity_) throw std::runtime_error("JIT buffer overflow");
    buffer_[size_++] = b;
}

void Emitter::emit_u32(uint32_t v) {
    if (size_ + 4 > capacity_) throw std::runtime_error("JIT buffer overflow");
    std::memcpy(buffer_ + size_, &v, 4);
    size_ += 4;
}

void Emitter::emit_u64(uint64_t v) {
    if (size_ + 8 > capacity_) throw std::runtime_error("JIT buffer overflow");
    std::memcpy(buffer_ + size_, &v, 8);
    size_ += 8;
}

void Emitter::patch_u32(size_t offset, uint32_t value) {
    if (offset + 4 > size_) throw std::runtime_error("Patch out of bounds");
    std::memcpy(buffer_ + offset, &value, 4);
}

// --- Encoding Helpers ---

void Emitter::emit_rex(bool w, bool r, bool x, bool b) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;
    if (r) rex |= 0x04;
    if (x) rex |= 0x02;
    if (b) rex |= 0x01;
    emit_byte(rex);
}

void Emitter::emit_modrm(int mode, int reg, int rm) {
    emit_byte((mode << 6) | ((reg & 7) << 3) | (rm & 7));
}

// --- Instructions ---

void Emitter::mov_reg_imm(Reg dst, uint64_t imm) {
    bool r12_15 = dst >= 8;
    // Tối ưu: Nếu imm 32-bit dương -> MOV r32, imm32 (tự động zero-extend)
    if (imm <= 0xFFFFFFFF && (imm & 0x80000000) == 0) {
        if (r12_15) emit_byte(0x41);
        emit_byte(0xB8 | (dst & 7));
        emit_u32((uint32_t)imm);
    } 
    // Tối ưu: Sign-extended 32-bit (MOV r64, imm32)
    else if ((int64_t)imm >= -2147483648LL && (int64_t)imm <= 2147483647LL) {
        emit_rex(true, false, false, r12_15);
        emit_byte(0xC7);
        emit_modrm(3, 0, dst);
        emit_u32((uint32_t)imm);
    } 
    // Full 64-bit
    else {
        emit_rex(true, false, false, r12_15);
        emit_byte(0xB8 | (dst & 7));
        emit_u64(imm);
    }
}

void Emitter::mov_reg_reg(Reg dst, Reg src) {
    if (dst == src) return;
    emit_rex(true, dst >= 8, false, src >= 8);
    emit_byte(0x8B);
    emit_modrm(3, dst, src);
}

void Emitter::mov_reg_mem(Reg dst, Reg base, int32_t disp) {
    emit_rex(true, dst >= 8, false, base >= 8);
    emit_byte(0x8B);
    // Disp 0
    if (disp == 0 && (base & 7) != 5) {
        emit_modrm(0, dst, base);
    } 
    // Disp 8-bit
    else if (disp >= -128 && disp <= 127) {
        emit_modrm(1, dst, base);
        emit_byte((uint8_t)disp);
    } 
    // Disp 32-bit
    else {
        emit_modrm(2, dst, base);
        emit_u32(disp);
    }
}

void Emitter::mov_mem_reg(Reg base, int32_t disp, Reg src) {
    emit_rex(true, src >= 8, false, base >= 8);
    emit_byte(0x89);
    if (disp == 0 && (base & 7) != 5) {
        emit_modrm(0, src, base);
    } else if (disp >= -128 && disp <= 127) {
        emit_modrm(1, src, base);
        emit_byte((uint8_t)disp);
    } else {
        emit_modrm(2, src, base);
        emit_u32(disp);
    }
}

void Emitter::alu_reg_reg(uint8_t opcode, Reg dst, Reg src) {
    emit_rex(true, src >= 8, false, dst >= 8);
    emit_byte(opcode);
    emit_modrm(3, src, dst);
}

void Emitter::imul_reg_reg(Reg dst, Reg src) {
    emit_rex(true, dst >= 8, false, src >= 8);
    emit_byte(0x0F); emit_byte(0xAF);
    emit_modrm(3, dst, src);
}

void Emitter::idiv_reg(Reg src) {
    emit_rex(true, false, false, src >= 8);
    emit_byte(0xF7);
    emit_modrm(3, 7, src);
}

void Emitter::cqo() {
    emit_rex(true, false, false, false);
    emit_byte(0x99);
}

void Emitter::shift_cl(Reg dst, bool right, bool arithmetic) {
    emit_rex(true, false, false, dst >= 8);
    emit_byte(0xD3);
    int ext = right ? (arithmetic ? 7 : 5) : 4;
    emit_modrm(3, ext, dst);
}

void Emitter::shift_imm(Reg dst, uint8_t imm, bool right, bool arithmetic) {
    emit_rex(true, false, false, dst >= 8);
    if (imm == 1) emit_byte(0xD1); else emit_byte(0xC1);
    int ext = right ? (arithmetic ? 7 : 5) : 4;
    emit_modrm(3, ext, dst);
    if (imm != 1) emit_byte(imm);
}

void Emitter::neg(Reg dst) {
    emit_rex(true, false, false, dst >= 8);
    emit_byte(0xF7); emit_modrm(3, 3, dst);
}

void Emitter::not_op(Reg dst) {
    emit_rex(true, false, false, dst >= 8);
    emit_byte(0xF7); emit_modrm(3, 2, dst);
}

void Emitter::jcc(Condition cond, int32_t rel_offset) {
    emit_byte(0x0F);
    emit_byte(0x80 | cond);
    emit_u32(rel_offset);
}

void Emitter::jmp(int32_t rel_offset) {
    emit_byte(0xE9);
    emit_u32(rel_offset);
}

void Emitter::setcc(Condition cond, Reg dst) {
    emit_byte(0x0F);
    emit_byte(0x90 | cond);
    emit_byte(0xC0 | (dst & 7)); // REX not strictly needed for AL-DL if no upper regs used
}

void Emitter::push(Reg r) {
    if (r >= 8) emit_byte(0x41);
    emit_byte(0x50 | (r & 7));
}

void Emitter::pop(Reg r) {
    if (r >= 8) emit_byte(0x41);
    emit_byte(0x58 | (r & 7));
}

void Emitter::ret() {
    emit_byte(0xC3);
}

} // namespace meow::jit::x64
EOF

echo "Đang tạo src/jit/compiler.h..."
cat << 'EOF' > src/jit/compiler.h
#pragma once

#include "x64/emitter.h"
#include <meow/value.h>
#include <meow/bytecode/op_codes.h>
#include <unordered_map>
#include <vector>

namespace meow::jit {

class Compiler {
public:
    using JitFunc = void (*)(Value* regs);

    Compiler();
    ~Compiler();

    JitFunc compile(const uint8_t* bytecode, size_t len);

private:
    uint8_t* code_mem_ = nullptr;
    size_t capacity_ = 1024 * 256;
    x64::Emitter emit_;
    
    struct Fixup {
        size_t jump_op_pos; // Vị trí bắt đầu lệnh jump
        size_t target_bc;   // Bytecode target
        bool is_cond;       // True nếu là Jcc (để tính offset lệnh)
    };
    std::vector<Fixup> fixups_;
    std::unordered_map<size_t, size_t> bc_to_native_;

    // Helpers
    x64::Reg map_vm_reg(int vm_reg) const;
    void load_vm_reg(x64::Reg cpu_dst, int vm_src);
    void store_vm_reg(int vm_dst, x64::Reg cpu_src);
    
    void emit_prologue();
    void emit_epilogue();
};

} // namespace meow::jit
EOF

echo "Đang tạo src/jit/compiler.cpp..."
cat << 'EOF' > src/jit/compiler.cpp
#include "compiler.h"
#include "meow_nanbox_layout.h" 
#include <sys/mman.h>
#include <cstring>
#include <iostream>

namespace meow::jit {

using namespace x64;

Compiler::Compiler() : emit_(nullptr, 0) {
    code_mem_ = (uint8_t*)mmap(nullptr, capacity_, 
                               PROT_READ | PROT_WRITE | PROT_EXEC, 
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code_mem_ == MAP_FAILED) throw std::runtime_error("JIT mmap failed");
    
    // Khởi tạo lại emitter với vùng nhớ thật
    emit_ = Emitter(code_mem_, capacity_);
}

Compiler::~Compiler() {
    if (code_mem_) munmap(code_mem_, capacity_);
}

// Map 5 thanh ghi đầu tiên của VM (0-4) vào các thanh ghi callee-saved của CPU
Reg Compiler::map_vm_reg(int vm_reg) const {
    switch (vm_reg) {
        case 0: return RBX;
        case 1: return R12;
        case 2: return R13;
        case 3: return R14;
        case 4: return R15;
        default: return INVALID_REG;
    }
}

// Load: Từ VM Memory (hoặc Cache Reg) -> CPU Reg (UNBOXED)
void Compiler::load_vm_reg(Reg cpu_dst, int vm_src) {
    Reg direct = map_vm_reg(vm_src);
    if (direct != INVALID_REG) {
        emit_.mov_reg_reg(cpu_dst, direct);
    } else {
        // Load từ [RDI + index*8]
        emit_.mov_reg_mem(cpu_dst, RDI, vm_src * 8);
        // Unbox (bỏ Tag ở 16 bit cao)
        emit_.shift_imm(cpu_dst, 16, false, false); // SHL 16
        emit_.shift_imm(cpu_dst, 16, true, true);   // SAR 16 (giữ dấu)
    }
}

// Store: Từ CPU Reg (UNBOXED) -> VM Memory (hoặc Cache Reg)
void Compiler::store_vm_reg(int vm_dst, Reg cpu_src) {
    Reg direct = map_vm_reg(vm_dst);
    if (direct != INVALID_REG) {
        emit_.mov_reg_reg(direct, cpu_src);
    } else {
        // Box: OR với Tag
        emit_.mov_reg_imm(RDX, NanboxLayout::make_tag(2)); // RDX làm temp chứa TAG INT
        // Nếu cpu_src == RAX, ta cần save RAX hoặc dùng register khác?
        // Ở đây giả sử cpu_src là kết quả tính toán tạm thời, có thể modify.
        // Tuy nhiên tốt nhất là copy ra RAX để box rồi ghi.
        
        emit_.mov_reg_reg(RAX, cpu_src); // RAX = val
        emit_.alu_reg_reg(0x09, RAX, RDX); // OR RAX, RDX (Box)
        emit_.mov_mem_reg(RDI, vm_dst * 8, RAX); // Store [RDI...]
    }
}

void Compiler::emit_prologue() {
    emit_.push(RBX);
    emit_.push(R12); emit_.push(R13);
    emit_.push(R14); emit_.push(R15);

    // Pre-load các thanh ghi hot (0-4) và Unbox sẵn
    for (int i = 0; i <= 4; ++i) {
        Reg r = map_vm_reg(i);
        emit_.mov_reg_mem(r, RDI, i * 8);
        emit_.shift_imm(r, 16, false, false); // SHL
        emit_.shift_imm(r, 16, true, true);   // SAR
    }
}

void Compiler::emit_epilogue() {
    // Save ngược các thanh ghi hot về RAM (Box lại)
    for (int i = 0; i <= 4; ++i) {
        Reg r = map_vm_reg(i);
        emit_.mov_reg_reg(RAX, r);
        emit_.mov_reg_imm(RDX, NanboxLayout::make_tag(2));
        emit_.alu_reg_reg(0x09, RAX, RDX); // Box
        emit_.mov_mem_reg(RDI, i * 8, RAX);
    }
    
    emit_.pop(R15); emit_.pop(R14);
    emit_.pop(R13); emit_.pop(R12);
    emit_.pop(RBX);
    emit_.ret();
}

Compiler::JitFunc Compiler::compile(const uint8_t* bytecode, size_t len) {
    bc_to_native_.clear();
    fixups_.clear();
    
    // TODO: Reset emitter cursor hoặc tạo buffer mới mỗi lần compile
    // Ở đây giả định compile 1 lần rồi thôi hoặc quản lý buffer bên ngoài.
    
    emit_prologue();

    size_t pc = 0;
    while (pc < len) {
        bc_to_native_[pc] = emit_.cursor();
        OpCode op = static_cast<OpCode>(bytecode[pc++]);

        auto read_u16 = [&]() { uint16_t v; memcpy(&v, bytecode + pc, 2); pc += 2; return v; };
        auto read_u8 = [&]() { return bytecode[pc++]; };

        switch (op) {
            case OpCode::LOAD_INT: {
                uint16_t dst = read_u16();
                int64_t val; memcpy(&val, bytecode + pc, 8); pc += 8;
                Reg r = map_vm_reg(dst);
                if (r != INVALID_REG) {
                    emit_.mov_reg_imm(r, val);
                } else {
                    // Spill to RAM
                    emit_.mov_reg_imm(RAX, val | NanboxLayout::make_tag(2));
                    emit_.mov_mem_reg(RDI, dst * 8, RAX);
                }
                break;
            }

            case OpCode::MOVE: {
                uint16_t dst = read_u16();
                uint16_t src = read_u16();
                load_vm_reg(RAX, src);
                store_vm_reg(dst, RAX);
                break;
            }

            case OpCode::ADD: case OpCode::ADD_B: {
                bool b = (op == OpCode::ADD_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                load_vm_reg(RAX, r1);
                load_vm_reg(RCX, r2);
                emit_.alu_reg_reg(0x01, RAX, RCX); // ADD
                store_vm_reg(dst, RAX);
                break;
            }
            
            case OpCode::SUB: case OpCode::SUB_B: {
                bool b = (op == OpCode::SUB_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                load_vm_reg(RAX, r1);
                load_vm_reg(RCX, r2);
                emit_.alu_reg_reg(0x29, RAX, RCX); // SUB
                store_vm_reg(dst, RAX);
                break;
            }

            case OpCode::MUL: case OpCode::MUL_B: {
                bool b = (op == OpCode::MUL_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                load_vm_reg(RAX, r1);
                load_vm_reg(RCX, r2);
                emit_.imul_reg_reg(RAX, RCX); 
                store_vm_reg(dst, RAX);
                break;
            }

            case OpCode::DIV: case OpCode::DIV_B: {
                bool b = (op == OpCode::DIV_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                load_vm_reg(RAX, r1);
                load_vm_reg(RCX, r2);
                emit_.cqo();          // Sign extend RAX -> RDX:RAX
                emit_.idiv_reg(RCX);  // Divide by RCX
                store_vm_reg(dst, RAX); // Quote
                break;
            }

            // Comparisons: EQ, NEQ, LT, GT...
            case OpCode::EQ: case OpCode::EQ_B:
            case OpCode::NEQ: case OpCode::NEQ_B:
            case OpCode::LT: case OpCode::LT_B: {
                bool b = (op >= OpCode::ADD_B); // check range thô
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                
                load_vm_reg(RAX, r1);
                load_vm_reg(RCX, r2);
                emit_.alu_reg_reg(0x39, RCX, RAX); // CMP RAX, RCX (Lưu ý thứ tự dst/src của CMP)

                Condition cond;
                switch(op) {
                    case OpCode::EQ: case OpCode::EQ_B: cond = Condition::E; break;
                    case OpCode::NEQ: case OpCode::NEQ_B: cond = Condition::NE; break;
                    case OpCode::LT: case OpCode::LT_B: cond = Condition::L; break;
                    default: cond = Condition::E; break;
                }
                
                emit_.setcc(cond, RAX); // Kết quả 1 byte vào AL
                // Zero extend AL -> RAX (MOVZX)
                // Đơn giản nhất: AND RAX, 0xFF
                emit_.alu_reg_reg(0x21, RAX, RAX); // Không đúng, AND không có dạng movzx
                // Workaround: MOVZX bằng cách shift nếu lười implement movzx:
                // SHL 56, SHR 56? Không, setcc chỉ set byte thấp. 
                // Đúng chuẩn: MOVZX. Vì chưa có movzx trong emitter, ta dùng:
                // AND RAX, 0xFF (nhưng phải load 0xFF vào reg khác).
                emit_.mov_reg_imm(RCX, 0xFF);
                emit_.alu_reg_reg(0x21, RAX, RCX); // AND RAX, 0xFF

                store_vm_reg(dst, RAX);
                break;
            }

            case OpCode::JUMP: {
                uint16_t target = read_u16();
                fixups_.push_back({emit_.cursor(), (size_t)target, false});
                emit_.jmp(0); 
                break;
            }
            
            case OpCode::JUMP_IF_TRUE: case OpCode::JUMP_IF_TRUE_B: {
                bool b = (op == OpCode::JUMP_IF_TRUE_B);
                uint16_t reg = b ? read_u8() : read_u16();
                uint16_t target = read_u16();
                
                load_vm_reg(RAX, reg);
                emit_.alu_reg_reg(0x21, RAX, RAX); // Test zero? No, AND. Use TEST logic implies CMP.
                // Emitter chưa có TEST, dùng CMP RAX, 0
                emit_.mov_reg_imm(RCX, 0);
                emit_.alu_reg_reg(0x39, RCX, RAX); // CMP RAX, 0

                // JNE (Not Equal 0) -> Jump if True
                fixups_.push_back({emit_.cursor(), (size_t)target, true});
                emit_.jcc(Condition::NE, 0);
                break;
            }

            case OpCode::HALT:
                emit_epilogue();
                break;

            default:
                emit_.ret(); // Fallback
                break;
        }
    }

    // Patch Jumps
    for (const auto& fix : fixups_) {
        size_t target_native = bc_to_native_[fix.target_bc];
        // JMP (E9 xx xx xx xx) -> 5 bytes
        // Jcc (0F 8x xx xx xx xx) -> 6 bytes
        size_t instr_len = fix.is_cond ? 6 : 5;
        size_t src_native = fix.jump_op_pos + instr_len; 
        
        int32_t rel = (int32_t)(target_native - src_native);
        // Patch vào offset +1 (cho JMP) hoặc +2 (cho Jcc)
        emit_.patch_u32(fix.jump_op_pos + (fix.is_cond ? 2 : 1), rel);
    }

    return (JitFunc)code_mem_;
}

} // namespace meow::jit
EOF

echo "Hoàn tất! Cấu trúc src/jit/ đã được tạo lại."

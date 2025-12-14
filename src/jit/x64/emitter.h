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

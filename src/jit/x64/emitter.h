#pragma once
#include <cstdint>
#include <cstddef>

namespace meow::jit::x64 {

enum Reg {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3, RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8 = 8,  R9 = 9,  R10 = 10, R11 = 11, R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    INVALID_REG = -1
};

enum Condition {
    O = 0, NO = 1, B = 2, AE = 3, E = 4, NE = 5, BE = 6, A = 7,
    S = 8, NS = 9, P = 10, NP = 11, L = 12, GE = 13, LE = 14, G = 15
};

class Emitter {
public:
    Emitter(uint8_t* buffer, size_t capacity);

    size_t cursor() const { return size_; }
    void patch_u32(size_t offset, uint32_t value);

    // --- High-Level Instructions ---
    void mov(Reg dst, Reg src);
    void mov(Reg dst, int64_t imm); 
    void mov(Reg dst, Reg base, int32_t disp); // Load
    void mov(Reg base, int32_t disp, Reg src); // Store

    void add(Reg dst, Reg src);
    void sub(Reg dst, Reg src);
    void imul(Reg dst, Reg src);
    void idiv(Reg src); 
    
    void and_(Reg dst, Reg src);
    void or_(Reg dst, Reg src);
    void xor_(Reg dst, Reg src);
    
    void shl(Reg dst, uint8_t imm);
    void sar(Reg dst, uint8_t imm);
    void shl_cl(Reg dst);
    void sar_cl(Reg dst);

    void cmp(Reg dst, Reg src);
    void test(Reg dst, Reg src);
    void jmp(int32_t rel_offset);
    void jcc(Condition cond, int32_t rel_offset);
    void setcc(Condition cond, Reg dst);
    
    void push(Reg r);
    void pop(Reg r);
    void ret();
    void cqo();

private:
    uint8_t* buffer_;
    size_t capacity_;
    size_t size_;

    void emit(uint8_t b);
    void emit_u32(uint32_t v);
    void emit_u64(uint64_t v);
    void emit_rex(bool w, bool r, bool x, bool b);
    void emit_modrm(int mode, int reg, int rm);
    
    void alu(uint8_t opcode, Reg dst, Reg src);
    void shift(uint8_t opcode, uint8_t subcode, Reg dst);
    void shift(uint8_t opcode, uint8_t subcode, Reg dst, uint8_t imm);
};

} // namespace meow::jit::x64
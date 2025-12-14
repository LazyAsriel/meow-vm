#include "jit/x64/emitter.h"
#include <cstring>
#include <iostream>

namespace meow::jit::x64 {

Emitter::Emitter(uint8_t* buffer, size_t capacity) 
    : buffer_(buffer), capacity_(capacity), size_(0) {}

void Emitter::emit(uint8_t b) {
    if (size_ < capacity_) buffer_[size_++] = b;
}

void Emitter::emit_u32(uint32_t v) {
    if (size_ + 4 <= capacity_) {
        std::memcpy(buffer_ + size_, &v, 4);
        size_ += 4;
    }
}

void Emitter::emit_u64(uint64_t v) {
    if (size_ + 8 <= capacity_) {
        std::memcpy(buffer_ + size_, &v, 8);
        size_ += 8;
    }
}

void Emitter::patch_u32(size_t offset, uint32_t value) {
    if (offset + 4 <= capacity_) {
        std::memcpy(buffer_ + offset, &value, 4);
    }
}

// REX Prefix: 0100 W R X B
void Emitter::emit_rex(bool w, bool r, bool x, bool b) {
    if (w || r || x || b) {
        uint8_t rex = 0x40 | (w ? 8 : 0) | (r ? 4 : 0) | (x ? 2 : 0) | (b ? 1 : 0);
        emit(rex);
    }
}

void Emitter::emit_modrm(int mode, int reg, int rm) {
    uint8_t val = ((mode & 3) << 6) | ((reg & 7) << 3) | (rm & 7);
    emit(val);
}

// --- Instructions ---

void Emitter::mov(Reg dst, Reg src) {
    // MOV r64, r64: 89 /r
    emit_rex(true, src >= R8, false, dst >= R8);
    emit(0x89);
    emit_modrm(3, src, dst);
}

void Emitter::mov(Reg dst, int64_t imm) {
    // MOV r64, imm64: B8+rd io
    emit_rex(true, false, false, dst >= R8);
    emit(0xB8 + (dst & 7));
    emit_u64(imm);
}

void Emitter::mov(Reg dst, Reg base, int32_t disp) {
    // MOV r64, [r64 + disp]: 8B /r
    emit_rex(true, dst >= R8, false, base >= R8);
    emit(0x8B);
    if (disp == 0 && (base & 7) != RBP) {
        emit_modrm(0, dst, base); // Mode 0: [reg]
    } else {
        emit_modrm(2, dst, base); // Mode 2: [reg + disp32]
        emit_u32(disp);
    }
}

void Emitter::mov(Reg base, int32_t disp, Reg src) {
    // MOV [r64 + disp], r64: 89 /r
    emit_rex(true, src >= R8, false, base >= R8);
    emit(0x89);
    if (disp == 0 && (base & 7) != RBP) {
        emit_modrm(0, src, base);
    } else {
        emit_modrm(2, src, base);
        emit_u32(disp);
    }
}

void Emitter::alu(uint8_t opcode, Reg dst, Reg src) {
    // 01 /r (ADD), 29 /r (SUB), 21 /r (AND), 09 /r (OR), 31 /r (XOR)
    emit_rex(true, src >= R8, false, dst >= R8);
    emit(opcode);
    emit_modrm(3, src, dst);
}

void Emitter::add(Reg dst, Reg src) { alu(0x01, dst, src); }
void Emitter::sub(Reg dst, Reg src) { alu(0x29, dst, src); }
void Emitter::and_(Reg dst, Reg src) { alu(0x21, dst, src); }
void Emitter::or_(Reg dst, Reg src)  { alu(0x09, dst, src); }
void Emitter::xor_(Reg dst, Reg src) { alu(0x31, dst, src); }

void Emitter::imul(Reg dst, Reg src) {
    // IMUL r64, r64: 0F AF /r
    emit_rex(true, dst >= R8, false, src >= R8);
    emit(0x0F); emit(0xAF);
    emit_modrm(3, dst, src);
}

void Emitter::idiv(Reg src) {
    // IDIV r64: F7 /7
    emit_rex(true, false, false, src >= R8);
    emit(0xF7);
    emit_modrm(3, 7, src);
}

void Emitter::shift(uint8_t opcode, uint8_t subcode, Reg dst) {
    // Shift by 1: D1 /sub
    emit_rex(true, false, false, dst >= R8);
    emit(opcode);
    emit_modrm(3, subcode, dst);
}

void Emitter::shift(uint8_t opcode, uint8_t subcode, Reg dst, uint8_t imm) {
    // Shift by imm: C1 /sub imm
    emit_rex(true, false, false, dst >= R8);
    emit(opcode);
    emit_modrm(3, subcode, dst);
    emit(imm);
}

void Emitter::shl(Reg dst, uint8_t imm) { shift(0xC1, 4, dst, imm); }
void Emitter::sar(Reg dst, uint8_t imm) { shift(0xC1, 7, dst, imm); }
void Emitter::shl_cl(Reg dst) { shift(0xD3, 4, dst); }
void Emitter::sar_cl(Reg dst) { shift(0xD3, 7, dst); }

void Emitter::cmp(Reg dst, Reg src) {
    // CMP r64, r64: 39 /r
    emit_rex(true, src >= R8, false, dst >= R8);
    emit(0x39);
    emit_modrm(3, src, dst);
}

void Emitter::test(Reg dst, Reg src) {
    // TEST r64, r64: 85 /r
    emit_rex(true, src >= R8, false, dst >= R8);
    emit(0x85);
    emit_modrm(3, src, dst);
}

void Emitter::jmp(int32_t rel_offset) {
    // JMP rel32: E9 cd
    emit(0xE9);
    emit_u32(rel_offset);
}

void Emitter::jcc(Condition cond, int32_t rel_offset) {
    // JCC rel32: 0F 8x cd
    emit(0x0F);
    emit(0x80 + cond);
    emit_u32(rel_offset);
}

void Emitter::setcc(Condition cond, Reg dst) {
    // SETcc r8: 0F 9x /0 (Reg is r8, need REX for SIL/DIL/etc if needed, but here simple)
    // Note: SETcc writes to byte register.
    bool r_high = (dst >= 4 && dst < 8); // SPL, BPL, SIL, DIL require REX
    bool r_ex = (dst >= 8);
    if (r_high || r_ex) emit_rex(false, false, false, r_ex);
    
    emit(0x0F);
    emit(0x90 + cond);
    emit_modrm(3, 0, dst);
}

// --- MỚI THÊM: Call & Movzx ---

void Emitter::call(Reg r) {
    // CALL r64: FF /2
    // REX.B needed if r >= R8.
    emit_rex(false, false, false, r >= R8);
    emit(0xFF);
    emit_modrm(3, 2, r); 
}

void Emitter::movzx_b(Reg dst, Reg src) {
    // MOVZX r64, r8: 0F B6 /r
    // REX.W = 1 (promotes to 64-bit dst)
    // REX.R = dst extension
    // REX.B = src extension
    emit_rex(true, dst >= R8, false, src >= R8);
    emit(0x0F);
    emit(0xB6);
    emit_modrm(3, dst, src);
}

void Emitter::push(Reg r) {
    // PUSH r64: 50+rd
    if (r >= R8) emit(0x41);
    emit(0x50 + (r & 7));
}

void Emitter::pop(Reg r) {
    // POP r64: 58+rd
    if (r >= R8) emit(0x41);
    emit(0x58 + (r & 7));
}

void Emitter::ret() {
    emit(0xC3);
}

void Emitter::cqo() {
    emit_rex(true, false, false, false);
    emit(0x99);
}

} // namespace meow::jit::x64
#include "jit/x64/emitter.h"
#include <cstring>
#include <stdexcept>

namespace meow::jit::x64 {

Emitter::Emitter(uint8_t* buffer, size_t capacity) 
    : buffer_(buffer), capacity_(capacity), size_(0) {}

void Emitter::emit(uint8_t b) {
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
    std::memcpy(buffer_ + offset, &value, 4);
}

// REX Prefix
void Emitter::emit_rex(bool w, bool r, bool x, bool b) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;
    if (r) rex |= 0x04;
    if (x) rex |= 0x02;
    if (b) rex |= 0x01;
    if (rex != 0x40) emit(rex);
}

// ModR/M
void Emitter::emit_modrm(int mode, int reg, int rm) {
    emit((mode << 6) | ((reg & 7) << 3) | (rm & 7));
}

// MOV reg, reg
void Emitter::mov(Reg dst, Reg src) {
    if (dst == src) return;
    emit_rex(true, dst >= 8, false, src >= 8);
    emit(0x8B);
    emit_modrm(3, dst, src);
}

// MOV reg, imm64
void Emitter::mov(Reg dst, int64_t imm) {
    // Optim: MOV r32, imm32 (zero extends) if positive and fits 32 bit
    if (imm >= 0 && imm <= 0xFFFFFFFF) {
        if (dst >= 8) emit(0x41);
        emit(0xB8 | (dst & 7));
        emit_u32((uint32_t)imm);
    } else if (imm >= -2147483648LL && imm <= 2147483647LL) {
        // Sign extended move: MOV r64, imm32 (C7 /0)
        emit_rex(true, false, false, dst >= 8);
        emit(0xC7);
        emit_modrm(3, 0, dst);
        emit_u32((uint32_t)imm);
    } else {
        // Full 64-bit load
        emit_rex(true, false, false, dst >= 8);
        emit(0xB8 | (dst & 7));
        emit_u64(imm);
    }
}

// MOV dst, [base + disp]
void Emitter::mov(Reg dst, Reg base, int32_t disp) {
    emit_rex(true, dst >= 8, false, base >= 8);
    emit(0x8B);
    if (disp == 0 && (base & 7) != 5) {
        emit_modrm(0, dst, base);
    } else if (disp >= -128 && disp <= 127) {
        emit_modrm(1, dst, base);
        emit((uint8_t)disp);
    } else {
        emit_modrm(2, dst, base);
        emit_u32(disp);
    }
    if ((base & 7) == 4) emit(0x24); // SIB for RSP
}

// MOV [base + disp], src
void Emitter::mov(Reg base, int32_t disp, Reg src) {
    emit_rex(true, src >= 8, false, base >= 8);
    emit(0x89);
    if (disp == 0 && (base & 7) != 5) {
        emit_modrm(0, src, base);
    } else if (disp >= -128 && disp <= 127) {
        emit_modrm(1, src, base);
        emit((uint8_t)disp);
    } else {
        emit_modrm(2, src, base);
        emit_u32(disp);
    }
    if ((base & 7) == 4) emit(0x24); // SIB for RSP
}

void Emitter::alu(uint8_t opcode, Reg dst, Reg src) {
    emit_rex(true, src >= 8, false, dst >= 8);
    emit(opcode);
    emit_modrm(3, src, dst);
}

void Emitter::add(Reg dst, Reg src) { alu(0x01, dst, src); }
void Emitter::sub(Reg dst, Reg src) { alu(0x29, dst, src); }
void Emitter::and_(Reg dst, Reg src) { alu(0x21, dst, src); }
void Emitter::or_(Reg dst, Reg src)  { alu(0x09, dst, src); }
void Emitter::xor_(Reg dst, Reg src) { alu(0x31, dst, src); }
void Emitter::cmp(Reg dst, Reg src) { alu(0x39, dst, src); } // CMP uses opcode 39 (MR) or 3B (RM)? 39 is MR.

void Emitter::imul(Reg dst, Reg src) {
    emit_rex(true, dst >= 8, false, src >= 8);
    emit(0x0F); emit(0xAF);
    emit_modrm(3, dst, src);
}

void Emitter::test(Reg dst, Reg src) {
    emit_rex(true, src >= 8, false, dst >= 8);
    emit(0x85);
    emit_modrm(3, src, dst);
}

// --- Jumps ---

void Emitter::jmp(int32_t rel_offset) {
    emit(0xE9);
    emit_u32(rel_offset);
}

void Emitter::jmp_short(int8_t rel_offset) {
    emit(0xEB);
    emit((uint8_t)rel_offset);
}

void Emitter::jcc(Condition cond, int32_t rel_offset) {
    emit(0x0F);
    emit(0x80 | (cond & 0xF));
    emit_u32(rel_offset);
}

void Emitter::jcc_short(Condition cond, int8_t rel_offset) {
    emit(0x70 | (cond & 0xF));
    emit((uint8_t)rel_offset);
}

void Emitter::setcc(Condition cond, Reg dst) {
    // SETcc only writes 8-bit, need rex if dst > 8? No, SETcc works on r/m8.
    // However, typical usage expects cleaning high bits if used as int.
    // For x64, if we want to target DL/CL etc, or R8B..R15B we need REX.
    if (dst >= 4) emit_rex(false, false, false, dst >= 8); // Enable byte regs SPL/BPL/SIL/DIL or R8B-R15B
    emit(0x0F);
    emit(0x90 | (cond & 0xF));
    emit_modrm(3, 0, dst);
}

void Emitter::movzx_b(Reg dst, Reg src) {
    emit_rex(true, dst >= 8, false, src >= 8);
    emit(0x0F); emit(0xB6);
    emit_modrm(3, dst, src);
}

void Emitter::push(Reg r) {
    if (r >= 8) emit(0x41);
    emit(0x50 | (r & 7));
}

void Emitter::pop(Reg r) {
    if (r >= 8) emit(0x41);
    emit(0x58 | (r & 7));
}

void Emitter::ret() { emit(0xC3); }

void Emitter::shl(Reg dst, uint8_t imm) {
    emit_rex(true, false, false, dst >= 8);
    if (imm == 1) { emit(0xD1); emit_modrm(3, 4, dst); }
    else { emit(0xC1); emit_modrm(3, 4, dst); emit(imm); }
}

void Emitter::sar(Reg dst, uint8_t imm) {
    emit_rex(true, false, false, dst >= 8);
    if (imm == 1) { emit(0xD1); emit_modrm(3, 7, dst); }
    else { emit(0xC1); emit_modrm(3, 7, dst); emit(imm); }
}

void Emitter::align(size_t boundary) {
    while ((size_ % boundary) != 0) {
        emit(0x90); // NOP
    }
}

} // namespace meow::jit::x64
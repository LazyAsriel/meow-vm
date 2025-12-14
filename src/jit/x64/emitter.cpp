#include "emitter.h"
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
    if (offset + 4 > size_) throw std::runtime_error("Patch out of bounds");
    std::memcpy(buffer_ + offset, &value, 4);
}

void Emitter::emit_rex(bool w, bool r, bool x, bool b) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;
    if (r) rex |= 0x04;
    if (x) rex |= 0x02;
    if (b) rex |= 0x01;
    emit(rex);
}

void Emitter::emit_modrm(int mode, int reg, int rm) {
    emit((mode << 6) | ((reg & 7) << 3) | (rm & 7));
}

// --- Instructions ---

void Emitter::mov(Reg dst, Reg src) {
    if (dst == src) return;
    emit_rex(true, dst >= 8, false, src >= 8);
    emit(0x8B); emit_modrm(3, dst, src);
}

void Emitter::mov(Reg dst, int64_t imm) {
    bool r12_15 = dst >= 8;
    if (imm >= 0 && imm <= 0xFFFFFFFF) {
        if (r12_15) emit(0x41);
        emit(0xB8 | (dst & 7)); emit_u32((uint32_t)imm);
    } else if (imm >= -2147483648LL && imm <= 2147483647LL) {
        emit_rex(true, false, false, r12_15);
        emit(0xC7); emit_modrm(3, 0, dst); emit_u32((uint32_t)imm);
    } else {
        emit_rex(true, false, false, r12_15);
        emit(0xB8 | (dst & 7)); emit_u64(imm);
    }
}

void Emitter::mov(Reg dst, Reg base, int32_t disp) {
    emit_rex(true, dst >= 8, false, base >= 8);
    emit(0x8B);
    if (disp == 0 && (base & 7) != 5) {
        emit_modrm(0, dst, base);
    } else if (disp >= -128 && disp <= 127) {
        emit_modrm(1, dst, base); emit((uint8_t)disp);
    } else {
        emit_modrm(2, dst, base); emit_u32(disp);
    }
}

void Emitter::mov(Reg base, int32_t disp, Reg src) {
    emit_rex(true, src >= 8, false, base >= 8);
    emit(0x89);
    if (disp == 0 && (base & 7) != 5) {
        emit_modrm(0, src, base);
    } else if (disp >= -128 && disp <= 127) {
        emit_modrm(1, src, base); emit((uint8_t)disp);
    } else {
        emit_modrm(2, src, base); emit_u32(disp);
    }
}

void Emitter::alu(uint8_t opcode, Reg dst, Reg src) {
    emit_rex(true, src >= 8, false, dst >= 8);
    emit(opcode); emit_modrm(3, src, dst);
}

void Emitter::add(Reg dst, Reg src) { alu(0x01, dst, src); }
void Emitter::sub(Reg dst, Reg src) { alu(0x29, dst, src); }
void Emitter::and_(Reg dst, Reg src) { alu(0x21, dst, src); }
void Emitter::or_(Reg dst, Reg src)  { alu(0x09, dst, src); }
void Emitter::xor_(Reg dst, Reg src) { alu(0x31, dst, src); }
void Emitter::cmp(Reg dst, Reg src)  { alu(0x39, dst, src); }

void Emitter::imul(Reg dst, Reg src) {
    emit_rex(true, dst >= 8, false, src >= 8);
    emit(0x0F); emit(0xAF); emit_modrm(3, dst, src);
}

void Emitter::idiv(Reg src) {
    emit_rex(true, false, false, src >= 8);
    emit(0xF7); emit_modrm(3, 7, src);
}

void Emitter::test(Reg dst, Reg src) {
    emit_rex(true, src >= 8, false, dst >= 8);
    emit(0x85); emit_modrm(3, src, dst);
}

void Emitter::shift(uint8_t opcode, uint8_t subcode, Reg dst) {
    emit_rex(true, false, false, dst >= 8);
    emit(opcode); emit_modrm(3, subcode, dst);
}
void Emitter::shift(uint8_t opcode, uint8_t subcode, Reg dst, uint8_t imm) {
    emit_rex(true, false, false, dst >= 8);
    emit(opcode); emit_modrm(3, subcode, dst); emit(imm);
}

void Emitter::shl(Reg dst, uint8_t imm) { if(imm==1) shift(0xD1, 4, dst); else shift(0xC1, 4, dst, imm); }
void Emitter::sar(Reg dst, uint8_t imm) { if(imm==1) shift(0xD1, 7, dst); else shift(0xC1, 7, dst, imm); }
void Emitter::shl_cl(Reg dst) { shift(0xD3, 4, dst); }
void Emitter::sar_cl(Reg dst) { shift(0xD3, 7, dst); }

void Emitter::jmp(int32_t rel_offset) { emit(0xE9); emit_u32(rel_offset); }

void Emitter::jcc(Condition cond, int32_t rel_offset) {
    emit(0x0F); emit(0x80 | cond); emit_u32(rel_offset);
}

void Emitter::setcc(Condition cond, Reg dst) {
    emit(0x0F); emit(0x90 | cond);
    emit_rex(false, false, false, dst >= 8);
    emit(0xC0 | (dst & 7)); 
}

void Emitter::push(Reg r) { if (r >= 8) emit(0x41); emit(0x50 | (r & 7)); }
void Emitter::pop(Reg r)  { if (r >= 8) emit(0x41); emit(0x58 | (r & 7)); }
void Emitter::ret() { emit(0xC3); }
void Emitter::cqo() { emit_rex(true, false, false, false); emit(0x99); }

} // namespace meow::jit::x64
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

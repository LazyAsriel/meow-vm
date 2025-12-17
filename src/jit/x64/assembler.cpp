#include "jit/x64/assembler.h"
#include <cstring>
#include <stdexcept>

namespace meow::jit::x64 {

Assembler::Assembler(uint8_t* buffer, size_t capacity) 
    : buffer_(buffer), capacity_(capacity), size_(0) {}

void Assembler::emit(uint8_t b) {
    if (size_ >= capacity_) throw std::runtime_error("JIT Buffer Overflow");
    buffer_[size_++] = b;
}

void Assembler::emit_u32(uint32_t v) {
    if (size_ + 4 > capacity_) throw std::runtime_error("JIT Buffer Overflow");
    std::memcpy(buffer_ + size_, &v, 4);
    size_ += 4;
}

void Assembler::emit_u64(uint64_t v) {
    if (size_ + 8 > capacity_) throw std::runtime_error("JIT Buffer Overflow");
    std::memcpy(buffer_ + size_, &v, 8);
    size_ += 8;
}

void Assembler::patch_u32(size_t offset, uint32_t value) {
    std::memcpy(buffer_ + offset, &value, 4);
}

// REX Prefix: 0100 WRXB
void Assembler::emit_rex(bool w, bool r, bool x, bool b) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08; // 64-bit operand size
    if (r) rex |= 0x04; // Extension of ModR/M reg field
    if (x) rex |= 0x02; // Extension of SIB index field
    if (b) rex |= 0x01; // Extension of ModR/M r/m field
    
    // Luôn emit nếu có bất kỳ bit nào hoặc operand là register cao (SPL/BPL/SIL/DIL)
    // Nhưng đơn giản hóa: Nếu operand >= R8 thì R/B sẽ true.
    if (rex != 0x40 || w) emit(rex); // MeowVM chủ yếu dùng 64-bit (W=1)
}

void Assembler::emit_modrm(int mode, int reg, int rm) {
    emit((mode << 6) | ((reg & 7) << 3) | (rm & 7));
}

// MOV dst, src
void Assembler::mov(Reg dst, Reg src) {
    if (dst == src) return; // NOP
    emit_rex(true, dst >= 8, false, src >= 8);
    emit(0x8B);
    emit_modrm(3, dst, src);
}

// MOV dst, imm64
void Assembler::mov(Reg dst, int64_t imm) {
    // Optim: Nếu số dương và vừa 32-bit, dùng MOV r32, imm32 (tự zero-extend lên 64)
    if (imm >= 0 && imm <= 0xFFFFFFFF) {
        if (dst >= 8) emit(0x41); // REX.B nếu dst >= R8
        emit(0xB8 | (dst & 7));
        emit_u32((uint32_t)imm);
    } 
    // Optim: Nếu số âm nhưng vừa 32-bit sign-extended (ví dụ -1, -100) -> MOV r64, imm32 (C7 /0)
    else if (imm >= -2147483648LL && imm <= 2147483647LL) {
        emit_rex(true, false, false, dst >= 8);
        emit(0xC7);
        emit_modrm(3, 0, dst);
        emit_u32((uint32_t)imm);
    } 
    // Full 64-bit load
    else {
        emit_rex(true, false, false, dst >= 8);
        emit(0xB8 | (dst & 7));
        emit_u64((uint64_t)imm);
    }
}

// MOV dst, [base + disp]
void Assembler::mov(Reg dst, Reg base, int32_t disp) {
    emit_rex(true, dst >= 8, false, base >= 8);
    emit(0x8B);
    
    if (disp == 0 && (base & 7) != 5) { // Mode 0: [reg]
        emit_modrm(0, dst, base);
    } else if (disp >= -128 && disp <= 127) { // Mode 1: [reg + disp8]
        emit_modrm(1, dst, base);
        emit((uint8_t)disp);
    } else { // Mode 2: [reg + disp32]
        emit_modrm(2, dst, base);
        emit_u32(disp);
    }
    if ((base & 7) == 4) emit(0x24); // SIB byte cho RSP (0x24 = [RSP])
}

// MOV [base + disp], src
void Assembler::mov(Reg base, int32_t disp, Reg src) {
    emit_rex(true, src >= 8, false, base >= 8);
    emit(0x89); // Store
    
    if (disp == 0 && (base & 7) != 5) {
        emit_modrm(0, src, base);
    } else if (disp >= -128 && disp <= 127) {
        emit_modrm(1, src, base);
        emit((uint8_t)disp);
    } else {
        emit_modrm(2, src, base);
        emit_u32(disp);
    }
    if ((base & 7) == 4) emit(0x24); // SIB RSP
}

void Assembler::emit_alu(uint8_t opcode, Reg dst, Reg src) {
    emit_rex(true, src >= 8, false, dst >= 8);
    emit(opcode);
    emit_modrm(3, src, dst);
}

void Assembler::add(Reg dst, Reg src) { emit_alu(0x01, dst, src); }
void Assembler::sub(Reg dst, Reg src) { emit_alu(0x29, dst, src); }
void Assembler::and_(Reg dst, Reg src) { emit_alu(0x21, dst, src); }
void Assembler::or_(Reg dst, Reg src) { emit_alu(0x09, dst, src); }
void Assembler::xor_(Reg dst, Reg src) { emit_alu(0x31, dst, src); }
void Assembler::cmp(Reg dst, Reg src) { emit_alu(0x39, dst, src); }
void Assembler::test(Reg dst, Reg src) { emit_rex(true, src >= 8, false, dst >= 8); emit(0x85); emit_modrm(3, src, dst); }

void Assembler::imul(Reg dst, Reg src) {
    emit_rex(true, dst >= 8, false, src >= 8);
    emit(0x0F); emit(0xAF);
    emit_modrm(3, dst, src);
}

void Assembler::shl(Reg dst, uint8_t imm) {
    emit_rex(true, false, false, dst >= 8);
    if (imm == 1) { emit(0xD1); emit_modrm(3, 4, dst); }
    else { emit(0xC1); emit_modrm(3, 4, dst); emit(imm); }
}

void Assembler::sar(Reg dst, uint8_t imm) {
    emit_rex(true, false, false, dst >= 8);
    if (imm == 1) { emit(0xD1); emit_modrm(3, 7, dst); }
    else { emit(0xC1); emit_modrm(3, 7, dst); emit(imm); }
}

void Assembler::jmp(int32_t rel_offset) { emit(0xE9); emit_u32(rel_offset); }
void Assembler::jmp_short(int8_t rel_offset) { emit(0xEB); emit((uint8_t)rel_offset); }

void Assembler::jcc(Condition cond, int32_t rel_offset) {
    emit(0x0F); emit(0x80 | (cond & 0xF)); emit_u32(rel_offset);
}

void Assembler::jcc_short(Condition cond, int8_t rel_offset) {
    emit(0x70 | (cond & 0xF)); emit((uint8_t)rel_offset);
}

void Assembler::call(Reg target) {
    // Indirect call: FF /2
    if (target >= 8) emit(0x41); 
    emit(0xFF);
    emit_modrm(3, 2, target);
}

void Assembler::ret() { emit(0xC3); }

void Assembler::push(Reg r) {
    if (r >= 8) emit(0x41);
    emit(0x50 | (r & 7));
}

void Assembler::pop(Reg r) {
    if (r >= 8) emit(0x41);
    emit(0x58 | (r & 7));
}

void Assembler::setcc(Condition cond, Reg dst) {
    if (dst >= 4) emit_rex(false, false, false, dst >= 8);
    emit(0x0F); emit(0x90 | (cond & 0xF));
    emit_modrm(3, 0, dst);
}

void Assembler::movzx_b(Reg dst, Reg src) {
    emit_rex(true, dst >= 8, false, src >= 8);
    emit(0x0F); emit(0xB6);
    emit_modrm(3, dst, src);
}

void Assembler::align(size_t boundary) {
    while ((size_ % boundary) != 0) emit(0x90);
}

} // namespace meow::jit::x64
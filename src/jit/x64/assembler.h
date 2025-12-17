/**
 * @file assembler.h
 * @brief Low-level x64 Machine Code Emitter
 */

#pragma once

#include "x64/common.h"
#include <vector>
#include <cstddef>
#include <cstdint>

namespace meow::jit::x64 {

    class Assembler {
    public:
        Assembler(uint8_t* buffer, size_t capacity);

        // --- Buffer Management ---
        size_t cursor() const { return size_; }
        uint8_t* start_ptr() const { return buffer_; }
        void patch_u32(size_t offset, uint32_t value);
        void align(size_t boundary);

        // --- Data Movement ---
        void mov(Reg dst, Reg src);
        void mov(Reg dst, int64_t imm64);        // MOV r64, imm64
        void mov(Reg dst, Reg base, int32_t disp); // Load: MOV dst, [base + disp]
        void mov(Reg base, int32_t disp, Reg src); // Store: MOV [base + disp], src
        
        // --- Arithmetic (ALU) ---
        void add(Reg dst, Reg src);
        void sub(Reg dst, Reg src);
        void imul(Reg dst, Reg src);
        void and_(Reg dst, Reg src);
        void or_(Reg dst, Reg src);
        void xor_(Reg dst, Reg src);
        void shl(Reg dst, uint8_t imm);
        void sar(Reg dst, uint8_t imm);

        // --- Control Flow & Comparison ---
        void cmp(Reg r1, Reg r2);
        void test(Reg r1, Reg r2);
        
        void jmp(int32_t rel_offset);      // JMP rel32
        void jmp_short(int8_t rel_offset); // JMP rel8
        
        void jcc(Condition cond, int32_t rel_offset);
        void jcc_short(Condition cond, int8_t rel_offset);
        
        void call(Reg target); // Indirect call: CALL reg
        void ret();

        // --- Stack ---
        void push(Reg r);
        void pop(Reg r);

        // --- Helper Instructions ---
        void setcc(Condition cond, Reg dst); // SETcc r8
        void movzx_b(Reg dst, Reg src);      // MOVZX r64, r8

    private:
        void emit(uint8_t b);
        void emit_u32(uint32_t v);
        void emit_u64(uint64_t v);
        
        // Encoding Helpers
        void emit_rex(bool w, bool r, bool x, bool b);
        void emit_modrm(int mode, int reg, int rm);
        void emit_alu(uint8_t opcode, Reg dst, Reg src);

        uint8_t* buffer_;
        size_t capacity_;
        size_t size_;
    };

} // namespace meow::jit::x64
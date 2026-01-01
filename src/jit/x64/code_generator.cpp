#include "x64/code_generator.h"
#include "jit_config.h"
#include "x64/common.h"
#include "meow/value.h"
#include "meow/bytecode/op_codes.h"
#include <cstring>
#include <iostream>

// --- Runtime Helper Definitions ---
namespace meow::jit::runtime {
    extern "C" void binary_op_generic(int op, uint64_t v1, uint64_t v2, uint64_t* dst);
    extern "C" void compare_generic(int op, uint64_t v1, uint64_t v2, uint64_t* dst);
}

namespace meow::jit::x64 {

using Layout = meow::Value::layout_traits;
using Variant = meow::base_t;

// Register Mapping
static constexpr Reg REG_VM_REGS_BASE = R14;
static constexpr Reg REG_CONSTS_BASE  = R15;

#define MEM_REG(idx)   REG_VM_REGS_BASE, (idx) * 8
#define MEM_CONST(idx) REG_CONSTS_BASE,  (idx) * 8

// --- Nanbox Constants ---
static constexpr uint64_t INT_INDEX = Variant::index_of<meow::int_t>();
static constexpr uint64_t BOOL_INDEX = Variant::index_of<meow::bool_t>();
static constexpr uint64_t NULL_INDEX = Variant::index_of<meow::null_t>();

static constexpr uint64_t TAG_INT    = Layout::make_tag(INT_INDEX);
static constexpr uint64_t TAG_BOOL   = Layout::make_tag(BOOL_INDEX);
static constexpr uint64_t TAG_NULL   = Layout::make_tag(NULL_INDEX);

static constexpr uint64_t TAG_SHIFT     = Layout::TAG_SHIFT;
static constexpr uint64_t TAG_CHECK_VAL = TAG_INT >> TAG_SHIFT; 
static constexpr uint64_t VALUE_FALSE   = TAG_BOOL | 0;

CodeGenerator::CodeGenerator(uint8_t* buffer, size_t capacity) 
    : asm_(buffer, capacity) {}

Reg CodeGenerator::map_vm_reg(int vm_reg) const {
    switch (vm_reg) {
        case 0: return RBX;
        case 1: return R12;
        case 2: return R13;
        default: return INVALID_REG;
    }
}

void CodeGenerator::load_vm_reg(Reg cpu_dst, int vm_src) {
    Reg mapped = map_vm_reg(vm_src);
    if (mapped != INVALID_REG) {
        if (mapped != cpu_dst) asm_.mov(cpu_dst, mapped);
    } else {
        asm_.mov(cpu_dst, MEM_REG(vm_src));
    }
}

void CodeGenerator::store_vm_reg(int vm_dst, Reg cpu_src) {
    Reg mapped = map_vm_reg(vm_dst);
    if (mapped != INVALID_REG) {
        if (mapped != cpu_src) asm_.mov(mapped, cpu_src);
    } else {
        asm_.mov(MEM_REG(vm_dst), cpu_src);
    }
}

void CodeGenerator::emit_prologue() {
    asm_.push(RBP); 
    asm_.mov(RBP, RSP);
    asm_.push(RBX); asm_.push(R12); asm_.push(R13); asm_.push(R14); asm_.push(R15);

    asm_.mov(REG_VM_REGS_BASE, RDI, 32); 
    asm_.mov(REG_CONSTS_BASE,  RDI, 40); 

    for (int i = 0; i <= 2; ++i) {
        Reg r = map_vm_reg(i);
        asm_.mov(r, MEM_REG(i));
    }
}

void CodeGenerator::emit_epilogue() {
    for (int i = 0; i <= 2; ++i) {
        Reg r = map_vm_reg(i);
        asm_.mov(MEM_REG(i), r);
    }
    asm_.pop(R15); asm_.pop(R14); asm_.pop(R13); asm_.pop(R12); asm_.pop(RBX);
    asm_.pop(RBP);
    asm_.ret();
}

void CodeGenerator::flush_cached_regs() {
    for (int i = 0; i <= 2; ++i) {
        Reg r = map_vm_reg(i);
        asm_.mov(MEM_REG(i), r);
    }
}

void CodeGenerator::reload_cached_regs() {
    for (int i = 0; i <= 2; ++i) {
        Reg r = map_vm_reg(i);
        asm_.mov(r, MEM_REG(i));
    }
}

JitFunc CodeGenerator::compile(const uint8_t* bytecode, size_t len) {
    bc_to_native_.clear();
    fixups_.clear();
    slow_paths_.clear();

    emit_prologue();

    size_t ip = 0;
    while (ip < len) {
        bc_to_native_[ip] = asm_.cursor();
        
        OpCode op = static_cast<OpCode>(bytecode[ip++]);

        auto read_u8  = [&]() { return bytecode[ip++]; };
        auto read_u16 = [&]() { 
            uint16_t v; std::memcpy(&v, bytecode + ip, 2); ip += 2; return v; 
        };

        // --- Helper: Arithmetic ---
        auto emit_binary_op = [&](uint8_t opcode_alu, bool is_byte_op) {
            uint16_t dst = is_byte_op ? read_u8() : read_u16();
            uint16_t r1  = is_byte_op ? read_u8() : read_u16();
            uint16_t r2  = is_byte_op ? read_u8() : read_u16();

            Reg r1_reg = map_vm_reg(r1);
            if (r1_reg == INVALID_REG) { load_vm_reg(RAX, r1); r1_reg = RAX; }
            Reg r2_reg = map_vm_reg(r2);
            if (r2_reg == INVALID_REG) { load_vm_reg(RCX, r2); r2_reg = RCX; }

            // Tag Checks
            asm_.mov(R8, r1_reg); asm_.sar(R8, TAG_SHIFT);
            asm_.mov(R9, TAG_CHECK_VAL); asm_.cmp(R8, R9);
            size_t j1 = asm_.cursor(); asm_.jcc(NE, 0);

            asm_.mov(R8, r2_reg); asm_.sar(R8, TAG_SHIFT); asm_.cmp(R8, R9);
            size_t j2 = asm_.cursor(); asm_.jcc(NE, 0);

            // Fast Path (Int)
            asm_.mov(R8, r1_reg); asm_.shl(R8, 16); asm_.sar(R8, 16);
            asm_.mov(R9, r2_reg); asm_.shl(R9, 16); asm_.sar(R9, 16);

            switch(opcode_alu) {
                case 0: asm_.add(R8, R9); break;
                case 1: asm_.sub(R8, R9); break;
                case 2: asm_.imul(R8, R9); break;
            }

            asm_.mov(R9, Layout::PAYLOAD_MASK); asm_.and_(R8, R9);
            asm_.mov(R9, TAG_INT); asm_.or_(R8, R9);
            store_vm_reg(dst, R8);

            size_t j_over = asm_.cursor(); asm_.jmp(0);

            SlowPath sp;
            sp.jumps_to_here = {j1, j2};
            sp.op = static_cast<int>(op);
            sp.dst_reg_idx = dst;
            sp.src1_reg_idx = r1;
            sp.src2_reg_idx = r2;
            sp.patch_jump_over = j_over;
            slow_paths_.push_back(sp);
        };

        // --- Helper: Standard Comparison ---
        auto emit_cmp_op = [&](Condition cond_code, bool is_byte_op) {
            uint16_t dst = is_byte_op ? read_u8() : read_u16();
            uint16_t r1  = is_byte_op ? read_u8() : read_u16();
            uint16_t r2  = is_byte_op ? read_u8() : read_u16();

            Reg r1_reg = map_vm_reg(r1);
            if (r1_reg == INVALID_REG) { load_vm_reg(RAX, r1); r1_reg = RAX; }
            Reg r2_reg = map_vm_reg(r2);
            if (r2_reg == INVALID_REG) { load_vm_reg(RCX, r2); r2_reg = RCX; }

            asm_.mov(R8, r1_reg); asm_.sar(R8, TAG_SHIFT);
            asm_.mov(R9, TAG_CHECK_VAL); asm_.cmp(R8, R9);
            size_t j1 = asm_.cursor(); asm_.jcc(NE, 0);

            asm_.mov(R8, r2_reg); asm_.sar(R8, TAG_SHIFT); asm_.cmp(R8, R9);
            size_t j2 = asm_.cursor(); asm_.jcc(NE, 0);

            asm_.cmp(r1_reg, r2_reg);
            asm_.setcc(cond_code, RAX);
            asm_.movzx_b(RAX, RAX);
            asm_.mov(R9, TAG_BOOL); asm_.or_(RAX, R9);
            store_vm_reg(dst, RAX);

            size_t j_over = asm_.cursor(); asm_.jmp(0);

            SlowPath sp;
            sp.jumps_to_here = {j1, j2};
            sp.op = static_cast<int>(op);
            sp.dst_reg_idx = dst;
            sp.src1_reg_idx = r1;
            sp.src2_reg_idx = r2;
            sp.patch_jump_over = j_over;
            slow_paths_.push_back(sp);
        };

        // --- Helper: Fused Compare & Jump [FIXED] ---
        auto emit_fused_cmp_jump = [&](Condition cond) {
            uint16_t r1_idx = read_u16();
            uint16_t r2_idx = read_u16();
            uint16_t target_off = read_u16();

            Reg r1 = map_vm_reg(r1_idx);
            if (r1 == INVALID_REG) { load_vm_reg(RAX, r1_idx); r1 = RAX; }
            Reg r2 = map_vm_reg(r2_idx);
            if (r2 == INVALID_REG) { load_vm_reg(RCX, r2_idx); r2 = RCX; }

            // 1. Tag Check (Int)
            asm_.mov(R8, r1); asm_.sar(R8, TAG_SHIFT);
            asm_.mov(R9, TAG_CHECK_VAL); asm_.cmp(R8, R9);
            size_t j1 = asm_.cursor(); asm_.jcc(NE, 0);

            asm_.mov(R8, r2); asm_.sar(R8, TAG_SHIFT); asm_.cmp(R8, R9);
            size_t j2 = asm_.cursor(); asm_.jcc(NE, 0);

            // 2. Fast Compare & Jump
            asm_.cmp(r1, r2);
            fixups_.push_back({asm_.cursor(), (size_t)target_off, true});
            asm_.jcc(cond, 0); 

            size_t j_over = asm_.cursor();

            // 3. Register Slow Path
            SlowPath sp;
            sp.jumps_to_here = {j1, j2};
            sp.op = static_cast<int>(op);
            sp.src1_reg_idx = r1_idx;
            sp.src2_reg_idx = r2_idx;
            sp.dst_reg_idx = target_off; // Hack: Lưu target offset vào dst
            sp.patch_jump_over = j_over;
            slow_paths_.push_back(sp);
        };

        switch (op) {
            case OpCode::LOAD_CONST: {
                uint16_t dst = read_u16();
                uint16_t idx = read_u16();
                asm_.mov(RAX, MEM_CONST(idx));
                store_vm_reg(dst, RAX);
                break;
            }
            case OpCode::LOAD_INT: {
                uint16_t dst = read_u16();
                int64_t val; std::memcpy(&val, bytecode + ip, 8); ip += 8;
                asm_.mov(RAX, val);
                asm_.mov(RCX, Layout::PAYLOAD_MASK); asm_.and_(RAX, RCX);
                asm_.mov(RCX, TAG_INT); asm_.or_(RAX, RCX);
                store_vm_reg(dst, RAX);
                break;
            }
            case OpCode::LOAD_TRUE: {
                 uint16_t dst = read_u16();
                 asm_.mov(RAX, TAG_BOOL | 1); store_vm_reg(dst, RAX);
                 break;
            }
            case OpCode::LOAD_FALSE: {
                 uint16_t dst = read_u16();
                 asm_.mov(RAX, TAG_BOOL); store_vm_reg(dst, RAX);
                 break;
            }
            case OpCode::MOVE: {
                uint16_t dst = read_u16();
                uint16_t src = read_u16();
                Reg src_reg = map_vm_reg(src);
                if (src_reg != INVALID_REG) store_vm_reg(dst, src_reg);
                else { asm_.mov(RAX, MEM_REG(src)); store_vm_reg(dst, RAX); }
                break;
            }

            case OpCode::ADD: case OpCode::ADD_B: emit_binary_op(0, op == OpCode::ADD_B); break;
            case OpCode::SUB: case OpCode::SUB_B: emit_binary_op(1, op == OpCode::SUB_B); break;
            case OpCode::MUL: case OpCode::MUL_B: emit_binary_op(2, op == OpCode::MUL_B); break;

            case OpCode::EQ:  case OpCode::EQ_B:  emit_cmp_op(E,  op == OpCode::EQ_B); break;
            case OpCode::NEQ: case OpCode::NEQ_B: emit_cmp_op(NE, op == OpCode::NEQ_B); break;
            case OpCode::LT:  case OpCode::LT_B:  emit_cmp_op(L,  op == OpCode::LT_B); break;
            case OpCode::LE:  case OpCode::LE_B:  emit_cmp_op(LE, op == OpCode::LE_B); break;
            case OpCode::GT:  case OpCode::GT_B:  emit_cmp_op(G,  op == OpCode::GT_B); break;
            case OpCode::GE:  case OpCode::GE_B:  emit_cmp_op(GE, op == OpCode::GE_B); break;

            // --- Fused Compare & Jump ---
            case OpCode::JUMP_IF_EQ: emit_fused_cmp_jump(E); break;
            case OpCode::JUMP_IF_NEQ: emit_fused_cmp_jump(NE); break;
            case OpCode::JUMP_IF_GT: emit_fused_cmp_jump(G); break;
            case OpCode::JUMP_IF_GE: emit_fused_cmp_jump(GE); break;
            case OpCode::JUMP_IF_LT: emit_fused_cmp_jump(L); break;
            case OpCode::JUMP_IF_LE: emit_fused_cmp_jump(LE); break;

            case OpCode::JUMP: {
                uint16_t off = read_u16();
                size_t target = off;
                if (bc_to_native_.count(target)) {
                    size_t target_native = bc_to_native_[target];
                    size_t current = asm_.cursor();
                    int32_t diff = (int32_t)(target_native - (current + 5));
                    asm_.jmp(diff);
                } else {
                    fixups_.push_back({asm_.cursor(), target, false});
                    asm_.jmp(0); 
                }
                break;
            }
            case OpCode::JUMP_IF_FALSE: case OpCode::JUMP_IF_FALSE_B: {
                bool is_b = (op == OpCode::JUMP_IF_FALSE_B);
                uint16_t reg = is_b ? read_u8() : read_u16();
                uint16_t off = read_u16();

                Reg r = map_vm_reg(reg);
                if (r == INVALID_REG) { load_vm_reg(RAX, reg); r = RAX; }

                asm_.mov(RCX, VALUE_FALSE); asm_.cmp(r, RCX);
                fixups_.push_back({asm_.cursor(), (size_t)off, true}); asm_.jcc(E, 0); 

                asm_.mov(RCX, TAG_NULL); asm_.cmp(r, RCX);
                fixups_.push_back({asm_.cursor(), (size_t)off, true}); asm_.jcc(E, 0); 
                break;
            }
            case OpCode::JUMP_IF_TRUE: case OpCode::JUMP_IF_TRUE_B: {
                bool is_b = (op == OpCode::JUMP_IF_TRUE_B);
                uint16_t reg = is_b ? read_u8() : read_u16();
                uint16_t off = read_u16();

                Reg r = map_vm_reg(reg);
                if (r == INVALID_REG) { load_vm_reg(RAX, reg); r = RAX; }

                asm_.mov(RCX, VALUE_FALSE); asm_.cmp(r, RCX);
                size_t skip1 = asm_.cursor(); asm_.jcc(E, 0);

                asm_.mov(RCX, TAG_NULL); asm_.cmp(r, RCX);
                size_t skip2 = asm_.cursor(); asm_.jcc(E, 0);

                fixups_.push_back({asm_.cursor(), (size_t)off, false}); asm_.jmp(0); 

                size_t next = asm_.cursor();
                asm_.patch_u32(skip1 + 2, (int32_t)(next - (skip1 + 6)));
                asm_.patch_u32(skip2 + 2, (int32_t)(next - (skip2 + 6)));
                break;
            }

            case OpCode::RETURN: case OpCode::HALT:
                emit_epilogue();
                break;

            default: break;
        }
    }

    // --- Generate Slow Paths ---
    for (auto& sp : slow_paths_) {
        size_t slow_start = asm_.cursor();
        
        for (size_t jump_src : sp.jumps_to_here) {
            int32_t off = (int32_t)(slow_start - (jump_src + 6));
            asm_.patch_u32(jump_src + 2, off);
        }

        flush_cached_regs();

        asm_.mov(RAX, 8); asm_.sub(RSP, RAX); // Align Stack

        bool is_fused_jump = (sp.op >= static_cast<int>(OpCode::JUMP_IF_EQ) && 
                              sp.op <= static_cast<int>(OpCode::JUMP_IF_LE));

        asm_.mov(RDI, (uint64_t)sp.op);    
        load_vm_reg(RSI, sp.src1_reg_idx); 
        load_vm_reg(RDX, sp.src2_reg_idx); 

        if (is_fused_jump) {
            // FIXED: Dùng RCX làm temp để trừ RSP
            asm_.mov(RCX, 8);
            asm_.sub(RSP, RCX); 
            
            asm_.mov(RCX, RSP);  // Arg4: Address of temp slot
            
            asm_.mov(RAX, (uint64_t)&runtime::compare_generic);
            asm_.call(RAX);

            // FIXED: Dùng mov(dst, base, disp) thay vì MEM_REG_PTR
            asm_.mov(RAX, RSP, 0);

            // FIXED: Dùng RCX làm temp để cộng RSP
            asm_.mov(RCX, 8);    
            asm_.add(RSP, RCX);

            // Check if True (TAG_BOOL | 1)
            asm_.mov(RCX, TAG_BOOL | 1);
            asm_.cmp(RAX, RCX);

            // Restore Stack Alignment trước khi Jump
            asm_.mov(RCX, 8); asm_.add(RSP, RCX); 
            reload_cached_regs();

            fixups_.push_back({asm_.cursor(), (size_t)sp.dst_reg_idx, true});
            asm_.jcc(E, 0); 
        } 
        else {
            // Standard Op
            asm_.mov(RCX, REG_VM_REGS_BASE);
            asm_.mov(RAX, sp.dst_reg_idx * 8);
            asm_.add(RCX, RAX); 

            if (sp.op >= static_cast<int>(OpCode::EQ)) asm_.mov(RAX, (uint64_t)&runtime::compare_generic);
            else asm_.mov(RAX, (uint64_t)&runtime::binary_op_generic);
            asm_.call(RAX);
            
            asm_.mov(RAX, 8); asm_.add(RSP, RAX); 
            reload_cached_regs();

            Reg mapped_dst = map_vm_reg(sp.dst_reg_idx);
            if (mapped_dst != INVALID_REG) asm_.mov(mapped_dst, MEM_REG(sp.dst_reg_idx));
        }

        size_t jump_back_target = sp.patch_jump_over + 5; 
        int32_t back_off = (int32_t)(jump_back_target - (asm_.cursor() + 5));
        asm_.jmp(back_off);
    }

    // --- Patch Forward Jumps ---
    for (const auto& fix : fixups_) {
        if (bc_to_native_.count(fix.target_bc)) {
            size_t target_native = bc_to_native_[fix.target_bc];
            size_t jump_len = fix.is_cond ? 6 : 5;
            int32_t rel = (int32_t)(target_native - (fix.jump_op_pos + jump_len));
            asm_.patch_u32(fix.jump_op_pos + (fix.is_cond ? 2 : 1), rel);
        }
    }

    return reinterpret_cast<JitFunc>(asm_.start_ptr());
}

} // namespace meow::jit::x64
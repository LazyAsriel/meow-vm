#include "x64/code_generator.h"
#include "jit_config.h"
#include "x64/common.h"
#include "meow/value.h"
#include "meow/bytecode/op_codes.h"
#include <cstring>
#include <iostream>

// --- Runtime Helper Definitions ---
namespace meow::jit::runtime {
    // Helper cho số học (+, -, *)
    extern "C" void binary_op_generic(int op, uint64_t v1, uint64_t v2, uint64_t* dst);
    // Helper cho so sánh (==, <, >...) - Trả về 1 (True) hoặc 0 (False) vào *dst
    extern "C" void compare_generic(int op, uint64_t v1, uint64_t v2, uint64_t* dst);
}

namespace meow::jit::x64 {

// --- Constants derived from MeowVM Layout ---

using Layout = meow::Value::layout_traits;
using Variant = meow::base_t;

// Register Mapping
static constexpr Reg REG_VM_REGS_BASE = R14;
static constexpr Reg REG_CONSTS_BASE  = R15;

#define MEM_REG(idx)   REG_VM_REGS_BASE, (idx) * 8
#define MEM_CONST(idx) REG_CONSTS_BASE,  (idx) * 8

// --- Nanbox Constants Calculation ---
// Tính toán Tag tại Compile-time dựa trên thứ tự trong meow::variant

// 1. INT (Index ?)
static constexpr uint64_t INT_INDEX = Variant::index_of<meow::int_t>();
static constexpr uint64_t TAG_INT   = Layout::make_tag(INT_INDEX);

// 2. BOOL (Index ?)
static constexpr uint64_t BOOL_INDEX = Variant::index_of<meow::bool_t>();
static constexpr uint64_t TAG_BOOL   = Layout::make_tag(BOOL_INDEX);

// 3. NULL (Index ?)
static constexpr uint64_t NULL_INDEX = Variant::index_of<meow::null_t>();
static constexpr uint64_t TAG_NULL   = Layout::make_tag(NULL_INDEX);

// Mask & Check Constants
static constexpr uint64_t TAG_SHIFT     = Layout::TAG_SHIFT;
static constexpr uint64_t TAG_CHECK_VAL = TAG_INT >> TAG_SHIFT; 

// Giá trị thực tế của FALSE trong Nanboxing (Tag Bool | 0)
// Lưu ý: True là (TAG_BOOL | 1)
static constexpr uint64_t VALUE_FALSE   = TAG_BOOL | 0;

CodeGenerator::CodeGenerator(uint8_t* buffer, size_t capacity) 
    : asm_(buffer, capacity) {}

// --- Register Allocation ---

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

// --- Frame Management ---

void CodeGenerator::emit_prologue() {
    asm_.push(RBP); 
    asm_.mov(RBP, RSP);
    
    // Save Callee-saved regs
    // Total pushes: 5. Stack alignment check:
    // Entry (le 8) -> push RBP (chan 16) -> push 5 regs (le 8).
    // => Hiện tại RSP đang LẺ (misaligned).
    asm_.push(RBX); asm_.push(R12); asm_.push(R13); asm_.push(R14); asm_.push(R15);

    // Setup VM Context
    asm_.mov(REG_VM_REGS_BASE, RDI, 0); // R14 = state->registers
    asm_.mov(REG_CONSTS_BASE,  RDI, 8); // R15 = state->constants

    // Load Cached Registers from RAM
    for (int i = 0; i <= 2; ++i) {
        Reg r = map_vm_reg(i);
        asm_.mov(r, MEM_REG(i));
    }
}

void CodeGenerator::emit_epilogue() {
    // Write-back Cached Registers to RAM
    for (int i = 0; i <= 2; ++i) {
        Reg r = map_vm_reg(i);
        asm_.mov(MEM_REG(i), r);
    }

    // Restore Callee-saved
    asm_.pop(R15); asm_.pop(R14); asm_.pop(R13); asm_.pop(R12); asm_.pop(RBX);
    asm_.pop(RBP);
    asm_.ret();
}

// --- Helpers for Code Gen ---

// Flush các thanh ghi vật lý về RAM trước khi gọi hàm C++
// Để đảm bảo GC nhìn thấy dữ liệu mới nhất.
void CodeGenerator::flush_cached_regs() {
    for (int i = 0; i <= 2; ++i) {
        Reg r = map_vm_reg(i);
        asm_.mov(MEM_REG(i), r);
    }
}

// Load lại thanh ghi từ RAM sau khi gọi hàm C++
// Để đảm bảo nếu GC di chuyển object, ta có địa chỉ mới.
void CodeGenerator::reload_cached_regs() {
    for (int i = 0; i <= 2; ++i) {
        Reg r = map_vm_reg(i);
        asm_.mov(r, MEM_REG(i));
    }
}

// --- Main Compile Loop ---

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

        // --- Guarded Arithmetic Implementation ---
        auto emit_binary_op = [&](uint8_t opcode_alu, bool is_byte_op) {
            uint16_t dst = is_byte_op ? read_u8() : read_u16();
            uint16_t r1  = is_byte_op ? read_u8() : read_u16();
            uint16_t r2  = is_byte_op ? read_u8() : read_u16();

            Reg r1_reg = map_vm_reg(r1);
            if (r1_reg == INVALID_REG) { load_vm_reg(RAX, r1); r1_reg = RAX; }
            
            Reg r2_reg = map_vm_reg(r2);
            if (r2_reg == INVALID_REG) { load_vm_reg(RCX, r2); r2_reg = RCX; }

            // 1. Tag Check (Chỉ tối ưu cho INT)
            // Check R1 is INT
            asm_.mov(R8, r1_reg);
            asm_.sar(R8, TAG_SHIFT);
            asm_.mov(R9, TAG_CHECK_VAL);
            asm_.cmp(R8, R9);
            
            Condition not_int1 = NE;
            size_t jump_slow1 = asm_.cursor();
            asm_.jcc(not_int1, 0); // Patch sau

            // Check R2 is INT
            asm_.mov(R8, r2_reg);
            asm_.sar(R8, TAG_SHIFT);
            asm_.cmp(R8, R9);
            
            Condition not_int2 = NE;
            size_t jump_slow2 = asm_.cursor();
            asm_.jcc(not_int2, 0); // Patch sau

            // 2. Fast Path (Unbox -> ALU -> Rebox)
            asm_.mov(R8, r1_reg);
            asm_.shl(R8, 16); asm_.sar(R8, 16); // Unbox R1
            
            asm_.mov(R9, r2_reg);
            asm_.shl(R9, 16); asm_.sar(R9, 16); // Unbox R2

            switch(opcode_alu) {
                case 0: asm_.add(R8, R9); break; // ADD
                case 1: asm_.sub(R8, R9); break; // SUB
                case 2: asm_.imul(R8, R9); break; // MUL
            }

            // Rebox
            asm_.mov(R9, Layout::PAYLOAD_MASK);
            asm_.and_(R8, R9);
            asm_.mov(R9, TAG_INT);
            asm_.or_(R8, R9);

            store_vm_reg(dst, R8);

            // Jump Over Slow Path
            size_t jump_over = asm_.cursor();
            asm_.jmp(0); 

            // Register Slow Path
            SlowPath sp;
            sp.jumps_to_here.push_back(jump_slow1);
            sp.jumps_to_here.push_back(jump_slow2);
            sp.op = static_cast<int>(op);
            sp.dst_reg_idx = dst;
            sp.src1_reg_idx = r1;
            sp.src2_reg_idx = r2;
            sp.patch_jump_over = jump_over;
            slow_paths_.push_back(sp);
        };

        // --- Comparison Logic (Chỉ Fast Path cho INT) ---
        auto emit_cmp_op = [&](Condition cond_code, bool is_byte_op) {
            uint16_t dst = is_byte_op ? read_u8() : read_u16();
            uint16_t r1  = is_byte_op ? read_u8() : read_u16();
            uint16_t r2  = is_byte_op ? read_u8() : read_u16();

            Reg r1_reg = map_vm_reg(r1);
            if (r1_reg == INVALID_REG) { load_vm_reg(RAX, r1); r1_reg = RAX; }
            Reg r2_reg = map_vm_reg(r2);
            if (r2_reg == INVALID_REG) { load_vm_reg(RCX, r2); r2_reg = RCX; }

            // Tag Check (R1 & R2 must be INT)
            asm_.mov(R8, r1_reg); asm_.sar(R8, TAG_SHIFT);
            asm_.mov(R9, TAG_CHECK_VAL);
            asm_.cmp(R8, R9);
            size_t j1 = asm_.cursor(); asm_.jcc(NE, 0);

            asm_.mov(R8, r2_reg); asm_.sar(R8, TAG_SHIFT);
            asm_.cmp(R8, R9);
            size_t j2 = asm_.cursor(); asm_.jcc(NE, 0);

            // Fast CMP
            asm_.cmp(r1_reg, r2_reg);
            asm_.setcc(cond_code, RAX); // Set AL based on flags
            asm_.movzx_b(RAX, RAX);     // Zero extend AL to RAX

            // Convert result (0/1) to Bool Value
            // Result = (RAX == 1) ? (TAG_BOOL | 1) : (TAG_BOOL | 0)
            // Trick: Bool Value = TAG_BOOL | RAX (vì RAX là 0 hoặc 1)
            asm_.mov(R9, TAG_BOOL);
            asm_.or_(RAX, R9);
            store_vm_reg(dst, RAX);

            size_t j_over = asm_.cursor(); asm_.jmp(0);

            // Slow Path (Nếu không phải Int)
            SlowPath sp;
            sp.jumps_to_here.push_back(j1);
            sp.jumps_to_here.push_back(j2);
            sp.op = static_cast<int>(op);
            sp.dst_reg_idx = dst;
            sp.src1_reg_idx = r1;
            sp.src2_reg_idx = r2;
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
                // Box Int
                asm_.mov(RAX, val);
                asm_.mov(RCX, Layout::PAYLOAD_MASK);
                asm_.and_(RAX, RCX);
                asm_.mov(RCX, TAG_INT);
                asm_.or_(RAX, RCX);
                store_vm_reg(dst, RAX);
                break;
            }
            case OpCode::LOAD_TRUE: {
                 uint16_t dst = read_u16();
                 asm_.mov(RAX, TAG_BOOL | 1);
                 store_vm_reg(dst, RAX);
                 break;
            }
            case OpCode::LOAD_FALSE: {
                 uint16_t dst = read_u16();
                 asm_.mov(RAX, TAG_BOOL); 
                 store_vm_reg(dst, RAX);
                 break;
            }
            case OpCode::MOVE: {
                uint16_t dst = read_u16();
                uint16_t src = read_u16();
                Reg src_reg = map_vm_reg(src);
                if (src_reg != INVALID_REG) {
                    store_vm_reg(dst, src_reg);
                } else {
                    asm_.mov(RAX, MEM_REG(src));
                    store_vm_reg(dst, RAX);
                }
                break;
            }

            // --- Arithmetic ---
            case OpCode::ADD: case OpCode::ADD_B: emit_binary_op(0, op == OpCode::ADD_B); break;
            case OpCode::SUB: case OpCode::SUB_B: emit_binary_op(1, op == OpCode::SUB_B); break;
            case OpCode::MUL: case OpCode::MUL_B: emit_binary_op(2, op == OpCode::MUL_B); break;

            // --- Comparisons ---
            case OpCode::EQ:  case OpCode::EQ_B:  emit_cmp_op(E,  op == OpCode::EQ_B); break;
            case OpCode::NEQ: case OpCode::NEQ_B: emit_cmp_op(NE, op == OpCode::NEQ_B); break;
            case OpCode::LT:  case OpCode::LT_B:  emit_cmp_op(L,  op == OpCode::LT_B); break;
            case OpCode::LE:  case OpCode::LE_B:  emit_cmp_op(LE, op == OpCode::LE_B); break;
            case OpCode::GT:  case OpCode::GT_B:  emit_cmp_op(G,  op == OpCode::GT_B); break;
            case OpCode::GE:  case OpCode::GE_B:  emit_cmp_op(GE, op == OpCode::GE_B); break;

            // --- Control Flow ---
            case OpCode::JUMP: {
                uint16_t off = read_u16();
                size_t target = off;
                if (bc_to_native_.count(target)) {
                    // Backward Jump
                    size_t target_native = bc_to_native_[target];
                    size_t current = asm_.cursor();
                    int32_t diff = (int32_t)(target_native - (current + 5));
                    asm_.jmp(diff);
                } else {
                    // Forward Jump
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

                // Check FALSE: (Val == VALUE_FALSE)
                asm_.mov(RCX, VALUE_FALSE);
                asm_.cmp(r, RCX);
                
                // Nếu Equal (là False) -> Jump
                fixups_.push_back({asm_.cursor(), (size_t)off, true});
                asm_.jcc(E, 0); 

                // Check NULL: (Val == TAG_NULL)
                asm_.mov(RCX, TAG_NULL);
                asm_.cmp(r, RCX);
                
                // Nếu Equal (là Null) -> Jump
                fixups_.push_back({asm_.cursor(), (size_t)off, true});
                asm_.jcc(E, 0); 

                break;
            }

            case OpCode::RETURN: case OpCode::HALT:
                emit_epilogue();
                break;

            default: break;
        }
    }

    // --- Generate Out-of-Line Slow Paths ---
    for (auto& sp : slow_paths_) {
        size_t slow_start = asm_.cursor();
        sp.label_start = slow_start;
        
        // 1. Patch jumps to here
        for (size_t jump_src : sp.jumps_to_here) {
            int32_t off = (int32_t)(slow_start - (jump_src + 6));
            asm_.patch_u32(jump_src + 2, off);
        }

        // 2. State Flushing (Cực kỳ quan trọng!)
        flush_cached_regs();

        // 3. Align Stack (Linux/System V requirement)
        // Hiện tại RSP đang lệch 8 (do Push 5 regs + RBP).
        asm_.mov(RAX, 8);
        asm_.sub(RSP, RAX);

        // 4. Setup Runtime Call (System V ABI: RDI, RSI, RDX, RCX, ...)
        asm_.mov(RDI, (uint64_t)sp.op);    // Arg1: Opcode
        load_vm_reg(RSI, sp.src1_reg_idx); // Arg2: Value 1
        load_vm_reg(RDX, sp.src2_reg_idx); // Arg3: Value 2

        // Arg4: Dst pointer (&regs[dst])
        asm_.mov(RCX, REG_VM_REGS_BASE);
        asm_.mov(RAX, sp.dst_reg_idx * 8);
        asm_.add(RCX, RAX); 

        // 5. Call helper
        if (sp.op >= static_cast<int>(OpCode::EQ)) {
             asm_.mov(RAX, (uint64_t)&runtime::compare_generic);
        } else {
             asm_.mov(RAX, (uint64_t)&runtime::binary_op_generic);
        }
        asm_.call(RAX);

        // 6. Restore Stack
        asm_.mov(RAX, 8);
        asm_.add(RSP, RAX);

        // 7. Reload State (vì GC có thể đã chạy)
        reload_cached_regs();

        // Reload kết quả từ RAM lên Register (nếu dst đang được cache)
        Reg mapped_dst = map_vm_reg(sp.dst_reg_idx);
        if (mapped_dst != INVALID_REG) {
            asm_.mov(mapped_dst, MEM_REG(sp.dst_reg_idx));
        }

        // 8. Jump back
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
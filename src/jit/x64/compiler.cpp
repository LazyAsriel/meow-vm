#include "jit/x64/compiler.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>

#include "vm/handlers/math_ops.h"
#include "vm/vm_state.h"

namespace meow::jit::x64 {

static constexpr uint64_t NANBOX_INT_TAG = meow::Value::layout_traits::make_tag(2);
static constexpr size_t PAGE_SIZE = 4096;

static constexpr Reg REG_VM_REGS  = RDI; 
static constexpr Reg REG_CONSTS   = RSI; 
static constexpr Reg REG_STATE    = RDX; 

static constexpr Reg RAX_REG = RAX;
static constexpr Reg RCX_REG = RCX;

#define MEM_REG(idx) REG_VM_REGS, (idx) * 8
#define MEM_CONST(idx) REG_CONSTS, (idx) * 8

Compiler::Compiler() : emit_(nullptr, 0) {
    if (capacity_ % PAGE_SIZE != 0) {
        capacity_ = (capacity_ / PAGE_SIZE + 1) * PAGE_SIZE;
    }
#if defined(__linux__) || defined(__APPLE__)
    code_mem_ = (uint8_t*)mmap(nullptr, capacity_, 
                               PROT_READ | PROT_WRITE | PROT_EXEC, 
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#else
    code_mem_ = new uint8_t[capacity_]; 
#endif
    emit_ = Emitter(code_mem_, capacity_);
}

Compiler::~Compiler() {
#if defined(__linux__) || defined(__APPLE__)
    if (code_mem_) munmap(code_mem_, capacity_);
#else
    if (code_mem_) delete[] code_mem_;
#endif
}

inline Reg Compiler::map_vm_reg(int vm_reg) const {
    switch (vm_reg) {
        case 0: return RBX;
        case 1: return R12;
        case 2: return R13;
        case 3: return R14;
        case 4: return R15;
        default: return INVALID_REG;
    }
}

inline void Compiler::load_vm_reg(Reg cpu_dst, int vm_src) {
    Reg mapped = map_vm_reg(vm_src);
    if (mapped != INVALID_REG) {
        if (mapped != cpu_dst) emit_.mov(cpu_dst, mapped);
    } else {
        emit_.mov(cpu_dst, MEM_REG(vm_src));
        emit_.shl(cpu_dst, 16);
        emit_.sar(cpu_dst, 16);
    }
}

inline void Compiler::store_vm_reg(int vm_dst, Reg cpu_src) {
    Reg mapped = map_vm_reg(vm_dst);
    if (mapped != INVALID_REG) {
        if (mapped != cpu_src) emit_.mov(mapped, cpu_src);
    } else {
        emit_.mov(MEM_REG(vm_dst), cpu_src); 
    }
}

Compiler::JitFunc Compiler::compile(const uint8_t* bytecode, size_t len) {
    emit_ = Emitter(code_mem_, capacity_);
    fixups_.clear();
    bc_to_native_.clear();

    emit_prologue(); 
    
    emit_.align(16);

    size_t ip = 0;
    while (ip < len) {
        bc_to_native_[ip] = emit_.cursor();
        
        OpCode op = static_cast<OpCode>(bytecode[ip]);
        ip++; 

        auto read_u8  = [&]() { return bytecode[ip++]; };
        auto read_u16 = [&]() { 
            uint16_t v; std::memcpy(&v, bytecode + ip, 2); ip += 2; return v; 
        };

        auto emit_cmp_logic = [&](OpCode op_cmp, uint16_t r1, uint16_t r2) -> Condition {
             Reg r1_reg = map_vm_reg(r1);
             if (r1_reg == INVALID_REG) { load_vm_reg(RAX_REG, r1); r1_reg = RAX_REG; }
             
             Reg r2_reg = map_vm_reg(r2);
             if (r2_reg == INVALID_REG) { load_vm_reg(RCX_REG, r2); r2_reg = RCX_REG; }

             emit_.cmp(r1_reg, r2_reg);

             switch (op_cmp) {
                case OpCode::EQ: case OpCode::EQ_B: return E;
                case OpCode::NEQ: case OpCode::NEQ_B: return NE;
                case OpCode::LT: case OpCode::LT_B: return L;
                case OpCode::LE: case OpCode::LE_B: return LE;
                case OpCode::GT: case OpCode::GT_B: return G;
                case OpCode::GE: case OpCode::GE_B: return GE;
                default: return E;
             }
        };

        switch (op) {
            case OpCode::LOAD_CONST: {
                uint16_t dst = read_u16();
                uint16_t idx = read_u16();
                emit_.mov(RAX_REG, MEM_CONST(idx));
                store_vm_reg(dst, RAX_REG);
                break;
            }
            case OpCode::LOAD_INT: {
                uint16_t dst = read_u16();
                int64_t val; std::memcpy(&val, bytecode + ip, 8); ip += 8;
                Reg mapped = map_vm_reg(dst);
                if (mapped != INVALID_REG) emit_.mov(mapped, val);
                else { emit_.mov(RAX_REG, val); store_vm_reg(dst, RAX_REG); }
                break;
            }
            case OpCode::MOVE: {
                uint16_t dst = read_u16();
                uint16_t src = read_u16();
                Reg dst_reg = map_vm_reg(dst);
                Reg src_reg = map_vm_reg(src);
                if (dst_reg != INVALID_REG && src_reg != INVALID_REG) emit_.mov(dst_reg, src_reg);
                else { load_vm_reg(RAX_REG, src); store_vm_reg(dst, RAX_REG); }
                break;
            }

            #define JIT_MATH_OP(OP_NAME, NATIVE_ALU) \
            case OpCode::OP_NAME: case OpCode::OP_NAME##_B: { \
                bool is_b = (op == OpCode::OP_NAME##_B); \
                uint16_t dst = is_b ? read_u8() : read_u16(); \
                uint16_t r1  = is_b ? read_u8() : read_u16(); \
                uint16_t r2  = is_b ? read_u8() : read_u16(); \
                Reg r1_reg = map_vm_reg(r1); \
                if (r1_reg == INVALID_REG) { load_vm_reg(RAX_REG, r1); r1_reg = RAX_REG; } \
                Reg r2_reg = map_vm_reg(r2); \
                if (r2_reg == INVALID_REG) { load_vm_reg(RCX_REG, r2); r2_reg = RCX_REG; } \
                Reg dst_reg = map_vm_reg(dst); \
                if (dst_reg != INVALID_REG) { \
                    if (dst_reg != r1_reg) emit_.mov(dst_reg, r1_reg); \
                    NATIVE_ALU(dst_reg, r2_reg); \
                } else { \
                    if (r1_reg != RAX_REG) emit_.mov(RAX_REG, r1_reg); \
                    NATIVE_ALU(RAX_REG, r2_reg); \
                    store_vm_reg(dst, RAX_REG); \
                } \
                break; \
            }
            JIT_MATH_OP(ADD, emit_.add)
            JIT_MATH_OP(SUB, emit_.sub)
            JIT_MATH_OP(MUL, emit_.imul)
            
            case OpCode::EQ: case OpCode::EQ_B:
            case OpCode::NEQ: case OpCode::NEQ_B:
            case OpCode::LT: case OpCode::LT_B:
            case OpCode::LE: case OpCode::LE_B:
            case OpCode::GT: case OpCode::GT_B:
            case OpCode::GE: case OpCode::GE_B: 
            {
                bool is_b = (op >= OpCode::ADD_B); 
                if (op == OpCode::LT_B || op == OpCode::GT_B || op == OpCode::LE_B || op == OpCode::GE_B || op == OpCode::EQ_B || op == OpCode::NEQ_B) is_b = true;
                uint16_t dst = is_b ? read_u8() : read_u16();
                uint16_t r1  = is_b ? read_u8() : read_u16();
                uint16_t r2  = is_b ? read_u8() : read_u16();

                Condition base_cond = emit_cmp_logic(op, r1, r2);

                // LOOP HEADER OPTIMIZATION (Forward Jump Fusion)
                size_t next_ip = ip;
                if (next_ip < len) {
                     OpCode next_op = static_cast<OpCode>(bytecode[next_ip]);
                     auto check_fuse = [&](bool on_true, size_t skip) {
                         uint8_t cond_reg;
                         if (skip == 4) cond_reg = bytecode[next_ip+1]; // _B
                         else { uint16_t r; memcpy(&r, bytecode+next_ip+1, 2); cond_reg = (uint8_t)r; }
                         
                         if (cond_reg == dst) {
                             uint16_t off; memcpy(&off, bytecode+next_ip+(skip-2), 2);
                             size_t target = (size_t)off;
                             Condition jmp_cond = on_true ? base_cond : (Condition)(base_cond ^ 1);
                             
                             if (bc_to_native_.count(target)) {
                                  size_t target_native = bc_to_native_[target];
                                  int64_t diff = (int64_t)target_native - (int64_t)(emit_.cursor() + 2);
                                  if (diff >= -128 && diff <= 127) emit_.jcc_short(jmp_cond, (int8_t)diff);
                                  else { fixups_.push_back({emit_.cursor(), target, true}); emit_.jcc(jmp_cond, 0); }
                             } else {
                                  fixups_.push_back({emit_.cursor(), target, true});
                                  emit_.jcc(jmp_cond, 0);
                             }
                             ip = next_ip + skip;
                             return true;
                         }
                         return false;
                     };

                     if (next_op == OpCode::JUMP_IF_TRUE_B && check_fuse(true, 4)) continue;
                     if (next_op == OpCode::JUMP_IF_FALSE_B && check_fuse(false, 4)) continue;
                     if (next_op == OpCode::JUMP_IF_TRUE && check_fuse(true, 5)) continue;
                     if (next_op == OpCode::JUMP_IF_FALSE && check_fuse(false, 5)) continue;
                }
                
                emit_.setcc(base_cond, RAX_REG); 
                emit_.movzx_b(RAX_REG, RAX_REG); 
                store_vm_reg(dst, RAX_REG);
                break;
            }

            case OpCode::JUMP: {
                uint16_t off = read_u16();
                size_t target_bc = off;
                
                // === LOOP ROTATION (Peeling) ===
                if (target_bc < ip) {
                    OpCode target_op = static_cast<OpCode>(bytecode[target_bc]);
                    
                    // Simple check for CMP opcodes
                    bool is_cmp = (target_op >= OpCode::EQ && target_op <= OpCode::GE_B); 

                    if (is_cmp) {
                        size_t t_ip = target_bc + 1; 
                        bool is_b = (target_op >= OpCode::ADD_B); 
                        if (target_op == OpCode::LT_B || target_op == OpCode::GT_B || target_op == OpCode::LE_B || target_op == OpCode::GE_B || target_op == OpCode::EQ_B || target_op == OpCode::NEQ_B) is_b = true;

                        // Skip operands
                        uint16_t t_dst = is_b ? bytecode[t_ip++] : (t_ip+=2, *(uint16_t*)(bytecode+t_ip-2));
                        uint16_t t_r1  = is_b ? bytecode[t_ip++] : (t_ip+=2, *(uint16_t*)(bytecode+t_ip-2));
                        uint16_t t_r2  = is_b ? bytecode[t_ip++] : (t_ip+=2, *(uint16_t*)(bytecode+t_ip-2));

                        OpCode next_op = static_cast<OpCode>(bytecode[t_ip]);
                        bool is_jmp_if = (next_op == OpCode::JUMP_IF_TRUE_B || next_op == OpCode::JUMP_IF_FALSE_B);
                        
                        if (is_jmp_if) {
                            uint8_t cond_reg = bytecode[t_ip + 1];
                            if (cond_reg == t_dst) {
                                // FOUND LOOP HEADER -> ROTATE
                                Condition cond = emit_cmp_logic(target_op, t_r1, t_r2);
                                
                                bool header_jumps_on_true = (next_op == OpCode::JUMP_IF_TRUE_B);
                                Condition loop_cond = header_jumps_on_true ? (Condition)(cond ^ 1) : cond;

                                // Target BODY is right after the conditional jump (size 4)
                                size_t body_bc = t_ip + 4; 
                                if (bc_to_native_.count(body_bc)) {
                                    size_t body_native = bc_to_native_[body_bc];
                                    int64_t diff = (int64_t)body_native - (int64_t)(emit_.cursor() + 2);
                                    if (diff >= -128 && diff <= 127) {
                                        emit_.jcc_short(loop_cond, (int8_t)diff);
                                        continue; 
                                    }
                                }
                            }
                        }
                    }
                }

                // Standard Jump
                if (bc_to_native_.count(target_bc)) {
                    size_t target_native = bc_to_native_[target_bc];
                    size_t curr = emit_.cursor();
                    int64_t diff = (int64_t)target_native - (int64_t)(curr + 2);
                    if (diff >= -128 && diff <= 127) emit_.jmp_short((int8_t)diff);
                    else emit_.jmp((int32_t)(target_native - (curr + 5)));
                } else {
                    fixups_.push_back({emit_.cursor(), target_bc, false});
                    emit_.jmp(0);
                }
                break;
            }

            case OpCode::JUMP_IF_TRUE_B: case OpCode::JUMP_IF_FALSE_B: {
                bool on_true = (op == OpCode::JUMP_IF_TRUE_B);
                uint8_t reg = read_u8();
                uint16_t off = read_u16();
                Reg r = map_vm_reg(reg);
                if (r == INVALID_REG) { load_vm_reg(RAX_REG, reg); r = RAX_REG; }
                emit_.test(r, r);
                fixups_.push_back({emit_.cursor(), (size_t)off, true});
                emit_.jcc(on_true ? NE : E, 0); 
                break;
            }

            case OpCode::RETURN: case OpCode::HALT:
                emit_epilogue();
                break;

            default: break;
        }
    }

    for (const auto& fix : fixups_) {
        if (bc_to_native_.count(fix.target_bc)) {
            size_t target_native = bc_to_native_[fix.target_bc];
            size_t jump_end = fix.jump_op_pos + (fix.is_cond ? 6 : 5);
            int32_t rel = (int32_t)(target_native - jump_end);
            emit_.patch_u32(fix.jump_op_pos + (fix.is_cond ? 2 : 1), rel);
        }
    }
    return reinterpret_cast<JitFunc>(code_mem_);
}

void Compiler::emit_prologue() {
    emit_.push(RBP); emit_.mov(RBP, RSP);
    emit_.push(RBX); emit_.push(R12); emit_.push(R13); emit_.push(R14); emit_.push(R15);
    for (int i = 0; i <= 4; ++i) {
        Reg cpu = map_vm_reg(i);
        if (cpu != INVALID_REG) {
            emit_.mov(cpu, MEM_REG(i));
            emit_.shl(cpu, 16); emit_.sar(cpu, 16);
        }
    }
}

void Compiler::emit_epilogue() {
    for (int i = 0; i <= 4; ++i) {
        Reg cpu = map_vm_reg(i);
        if (cpu != INVALID_REG) {
            emit_.mov(RAX_REG, cpu);
            emit_.mov(RCX_REG, NANBOX_INT_TAG);
            emit_.or_(RAX_REG, RCX_REG); 
            emit_.mov(MEM_REG(i), RAX_REG);
        }
    }
    emit_.pop(R15); emit_.pop(R14); emit_.pop(R13); emit_.pop(R12); emit_.pop(RBX);
    emit_.mov(RSP, RBP); emit_.pop(RBP); emit_.ret();
}

} // namespace meow::jit::x64
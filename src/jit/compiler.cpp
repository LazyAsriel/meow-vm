#include "compiler.h"
#include "meow_nanbox_layout.h" 
#include <sys/mman.h>
#include <cstring>
#include <iostream>

namespace meow::jit {

using namespace x64;

// --- Helpers ---
static bool is_conditional_jump(OpCode op) {
    return op == OpCode::JUMP_IF_TRUE || op == OpCode::JUMP_IF_TRUE_B ||
           op == OpCode::JUMP_IF_FALSE || op == OpCode::JUMP_IF_FALSE_B;
}

static int get_jump_condition_reg(const uint8_t* bytecode, size_t pc, OpCode op) {
    if (op == OpCode::JUMP_IF_TRUE_B || op == OpCode::JUMP_IF_FALSE_B) {
        return bytecode[pc];
    } else {
        uint16_t r; memcpy(&r, bytecode + pc, 2); return r;
    }
}

// --- Compiler ---

Compiler::Compiler() : emit_(nullptr, 0) {
    code_mem_ = (uint8_t*)mmap(nullptr, capacity_, 
                               PROT_READ | PROT_WRITE | PROT_EXEC, 
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code_mem_ == MAP_FAILED) throw std::runtime_error("JIT mmap failed");
    emit_ = Emitter(code_mem_, capacity_);
}

Compiler::~Compiler() {
    if (code_mem_) munmap(code_mem_, capacity_);
}

Reg Compiler::map_vm_reg(int vm_reg) const {
    switch (vm_reg) {
        case 0: return RBX; case 1: return R12;
        case 2: return R13; case 3: return R14;
        case 4: return R15; default: return INVALID_REG;
    }
}

void Compiler::load_vm_reg(Reg cpu_dst, int vm_src) {
    Reg direct = map_vm_reg(vm_src);
    if (direct != INVALID_REG) {
        emit_.mov(cpu_dst, direct);
    } else {
        emit_.mov(cpu_dst, RDI, vm_src * 8);
        emit_.shl(cpu_dst, 16); // Unbox
        emit_.sar(cpu_dst, 16);
    }
}

void Compiler::store_vm_reg(int vm_dst, Reg cpu_src) {
    Reg direct = map_vm_reg(vm_dst);
    if (direct != INVALID_REG) {
        emit_.mov(direct, cpu_src);
    } else {
        emit_.mov(RDX, NanboxLayout::make_tag(2));
        emit_.mov(RAX, cpu_src); 
        emit_.or_(RAX, RDX); // Box
        emit_.mov(RDI, vm_dst * 8, RAX); 
    }
}

void Compiler::emit_prologue() {
    emit_.push(RBX); emit_.push(R12); emit_.push(R13);
    emit_.push(R14); emit_.push(R15);
    for (int i = 0; i <= 4; ++i) {
        Reg r = map_vm_reg(i);
        emit_.mov(r, RDI, i * 8);
        emit_.shl(r, 16); emit_.sar(r, 16);
    }
}

void Compiler::emit_epilogue() {
    for (int i = 0; i <= 4; ++i) {
        Reg r = map_vm_reg(i);
        emit_.mov(RAX, r);
        emit_.mov(RDX, NanboxLayout::make_tag(2));
        emit_.or_(RAX, RDX);
        emit_.mov(RDI, i * 8, RAX);
    }
    emit_.pop(R15); emit_.pop(R14); emit_.pop(R13);
    emit_.pop(R12); emit_.pop(RBX); emit_.ret();
}

Compiler::JitFunc Compiler::compile(const uint8_t* bytecode, size_t len) {
    bc_to_native_.clear(); fixups_.clear();
    emit_prologue();

    size_t pc = 0;
    while (pc < len) {
        bc_to_native_[pc] = emit_.cursor();
        OpCode op = static_cast<OpCode>(bytecode[pc++]);
        auto read_u16 = [&]() { uint16_t v; memcpy(&v, bytecode + pc, 2); pc += 2; return v; };
        auto read_u8 = [&]() { return bytecode[pc++]; };

        switch (op) {
            case OpCode::LOAD_INT: {
                uint16_t dst = read_u16();
                int64_t val; memcpy(&val, bytecode + pc, 8); pc += 8;
                Reg r = map_vm_reg(dst);
                
                // Zero Idiom (XOR r, r)
                if (r != INVALID_REG) {
                    if (val == 0) emit_.xor_(r, r);
                    else emit_.mov(r, val);
                } else {
                    if (val == 0) emit_.xor_(RAX, RAX);
                    else emit_.mov(RAX, val | NanboxLayout::make_tag(2));
                    emit_.mov(RDI, dst * 8, RAX);
                }
                break;
            }

            case OpCode::MOVE: case OpCode::MOVE_B: {
                bool b = (op == OpCode::MOVE_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t src = b ? read_u8() : read_u16();
                
                Reg rd = map_vm_reg(dst);
                Reg rs = map_vm_reg(src);
                if (rd != INVALID_REG && rs != INVALID_REG) emit_.mov(rd, rs);
                else { load_vm_reg(RAX, src); store_vm_reg(dst, RAX); }
                break;
            }

            // --- ARITHMETIC ---
            case OpCode::ADD: case OpCode::ADD_B: {
                bool b = (op == OpCode::ADD_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                
                Reg rd = map_vm_reg(dst), rs1 = map_vm_reg(r1), rs2 = map_vm_reg(r2);
                if (rd != INVALID_REG && rs1 != INVALID_REG && rs2 != INVALID_REG) {
                    if (dst == r1) emit_.add(rd, rs2);
                    else if (dst == r2) emit_.add(rd, rs1);
                    else { emit_.mov(rd, rs1); emit_.add(rd, rs2); }
                } else {
                    load_vm_reg(RAX, r1); load_vm_reg(RCX, r2);
                    emit_.add(RAX, RCX); store_vm_reg(dst, RAX);
                }
                break;
            }

            case OpCode::SUB: case OpCode::SUB_B: {
                bool b = (op == OpCode::SUB_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                Reg rd = map_vm_reg(dst), rs1 = map_vm_reg(r1), rs2 = map_vm_reg(r2);
                if (rd != INVALID_REG && rs1 != INVALID_REG && rs2 != INVALID_REG) {
                    if (dst == r1) emit_.sub(rd, rs2);
                    else { emit_.mov(rd, rs1); emit_.sub(rd, rs2); }
                } else {
                    load_vm_reg(RAX, r1); load_vm_reg(RCX, r2);
                    emit_.sub(RAX, RCX); store_vm_reg(dst, RAX);
                }
                break;
            }

            case OpCode::MUL: case OpCode::MUL_B: {
                bool b = (op == OpCode::MUL_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                Reg rd = map_vm_reg(dst), rs1 = map_vm_reg(r1), rs2 = map_vm_reg(r2);
                if (rd != INVALID_REG && rs1 != INVALID_REG && rs2 != INVALID_REG) {
                    if (dst == r1) emit_.imul(rd, rs2);
                    else if (dst == r2) emit_.imul(rd, rs1);
                    else { emit_.mov(rd, rs1); emit_.imul(rd, rs2); }
                } else {
                    load_vm_reg(RAX, r1); load_vm_reg(RCX, r2);
                    emit_.imul(RAX, RCX); store_vm_reg(dst, RAX);
                }
                break;
            }

            case OpCode::DIV: case OpCode::DIV_B: {
                bool b = (op == OpCode::DIV_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                load_vm_reg(RAX, r1); load_vm_reg(RCX, r2);
                emit_.cqo(); emit_.idiv(RCX); store_vm_reg(dst, RAX);
                break;
            }

            // --- BITWISE ---
            case OpCode::BIT_AND: case OpCode::BIT_AND_B:
            case OpCode::BIT_OR: case OpCode::BIT_OR_B:
            case OpCode::BIT_XOR: case OpCode::BIT_XOR_B: {
                bool b = (op >= OpCode::ADD_B); // Simplified check
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                Reg rd = map_vm_reg(dst), rs1 = map_vm_reg(r1), rs2 = map_vm_reg(r2);
                
                auto emit_op = [&](Reg d, Reg s) {
                    if (op == OpCode::BIT_AND || op == OpCode::BIT_AND_B) emit_.and_(d, s);
                    else if (op == OpCode::BIT_OR || op == OpCode::BIT_OR_B) emit_.or_(d, s);
                    else emit_.xor_(d, s);
                };

                if (rd != INVALID_REG && rs1 != INVALID_REG && rs2 != INVALID_REG) {
                    if (dst == r1) emit_op(rd, rs2);
                    else if (dst == r2) emit_op(rd, rs1);
                    else { emit_.mov(rd, rs1); emit_op(rd, rs2); }
                } else {
                    load_vm_reg(RAX, r1); load_vm_reg(RCX, r2);
                    emit_op(RAX, RCX); store_vm_reg(dst, RAX);
                }
                break;
            }

            // --- COMPARISONS ---
            case OpCode::EQ: case OpCode::EQ_B: case OpCode::NEQ: case OpCode::NEQ_B:
            case OpCode::LT: case OpCode::LT_B: case OpCode::GT: case OpCode::GT_B:
            case OpCode::LE: case OpCode::LE_B: case OpCode::GE: case OpCode::GE_B: {
                bool b = (op >= OpCode::ADD_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                
                Reg rs1 = map_vm_reg(r1), rs2 = map_vm_reg(r2);
                if (rs1 != INVALID_REG && rs2 != INVALID_REG) emit_.cmp(rs1, rs2);
                else { load_vm_reg(RAX, r1); load_vm_reg(RCX, r2); emit_.cmp(RAX, RCX); }

                // Fusion Logic
                size_t next_pc = pc;
                bool fused = false;
                if (next_pc < len) {
                    OpCode next_op = static_cast<OpCode>(bytecode[next_pc]);
                    if (is_conditional_jump(next_op)) {
                        int jump_reg = get_jump_condition_reg(bytecode, next_pc + 1, next_op);
                        if (jump_reg == dst) {
                            fused = true;
                            bool is_b = (next_op == OpCode::JUMP_IF_TRUE_B || next_op == OpCode::JUMP_IF_FALSE_B);
                            size_t offset_size = is_b ? 1 : 2;
                            size_t jump_param_offset = next_pc + 1 + offset_size;
                            uint16_t target; memcpy(&target, bytecode + jump_param_offset, 2);

                            Condition cond;
                            switch(op) {
                                case OpCode::EQ: case OpCode::EQ_B: cond = E; break;
                                case OpCode::NEQ: case OpCode::NEQ_B: cond = NE; break;
                                case OpCode::LT: case OpCode::LT_B: cond = L; break;
                                case OpCode::GT: case OpCode::GT_B: cond = G; break;
                                case OpCode::LE: case OpCode::LE_B: cond = LE; break;
                                case OpCode::GE: case OpCode::GE_B: cond = GE; break;
                                default: cond = E; break;
                            }
                            // Đảo logic nếu là JUMP_FALSE
                            if (next_op == OpCode::JUMP_IF_FALSE || next_op == OpCode::JUMP_IF_FALSE_B) {
                                cond = static_cast<Condition>(cond ^ 1); // Toggle logic bit (E <-> NE)
                            }

                            fixups_.push_back({emit_.cursor(), (size_t)target, true});
                            emit_.jcc(cond, 0);
                            pc = jump_param_offset + 2; 
                        }
                    }
                }

                if (!fused) {
                    Condition cond; // ... (Map opcode to cond similar to above)
                    switch(op) {
                        case OpCode::EQ: case OpCode::EQ_B: cond = E; break;
                        case OpCode::NEQ: case OpCode::NEQ_B: cond = NE; break;
                        case OpCode::LT: case OpCode::LT_B: cond = L; break;
                        default: cond = E; break; // Simplified fallback
                    }
                    emit_.setcc(cond, RAX);
                    emit_.mov(RCX, 0xFF);
                    emit_.and_(RAX, RCX);
                    store_vm_reg(dst, RAX);
                }
                break;
            }

            case OpCode::JUMP: {
                uint16_t target = read_u16();
                fixups_.push_back({emit_.cursor(), (size_t)target, false});
                emit_.jmp(0); 
                break;
            }
            
            case OpCode::JUMP_IF_TRUE: case OpCode::JUMP_IF_TRUE_B: {
                bool b = (op == OpCode::JUMP_IF_TRUE_B);
                uint16_t reg = b ? read_u8() : read_u16();
                uint16_t target = read_u16();
                load_vm_reg(RAX, reg);
                emit_.test(RAX, RAX);
                fixups_.push_back({emit_.cursor(), (size_t)target, true});
                emit_.jcc(NE, 0);
                break;
            }

            case OpCode::HALT:
                emit_epilogue(); break;
            default: emit_epilogue(); break;
        }
    }

    for (const auto& fix : fixups_) {
        if (bc_to_native_.count(fix.target_bc)) {
            size_t target = bc_to_native_[fix.target_bc];
            size_t src = fix.jump_op_pos + (fix.is_cond ? 6 : 5);
            emit_.patch_u32(fix.jump_op_pos + (fix.is_cond ? 2 : 1), (int32_t)(target - src));
        }
    }
    return (JitFunc)code_mem_;
}

} // namespace meow::jit
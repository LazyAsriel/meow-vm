#include "compiler.h"
#include "meow_nanbox_layout.h" 
#include <sys/mman.h>
#include <cstring>
#include <iostream>

namespace meow::jit {

using namespace x64;

Compiler::Compiler() : emit_(nullptr, 0) {
    code_mem_ = (uint8_t*)mmap(nullptr, capacity_, 
                               PROT_READ | PROT_WRITE | PROT_EXEC, 
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code_mem_ == MAP_FAILED) throw std::runtime_error("JIT mmap failed");
    
    // Khởi tạo lại emitter với vùng nhớ thật
    emit_ = Emitter(code_mem_, capacity_);
}

Compiler::~Compiler() {
    if (code_mem_) munmap(code_mem_, capacity_);
}

// Map 5 thanh ghi đầu tiên của VM (0-4) vào các thanh ghi callee-saved của CPU
Reg Compiler::map_vm_reg(int vm_reg) const {
    switch (vm_reg) {
        case 0: return RBX;
        case 1: return R12;
        case 2: return R13;
        case 3: return R14;
        case 4: return R15;
        default: return INVALID_REG;
    }
}

// Load: Từ VM Memory (hoặc Cache Reg) -> CPU Reg (UNBOXED)
void Compiler::load_vm_reg(Reg cpu_dst, int vm_src) {
    Reg direct = map_vm_reg(vm_src);
    if (direct != INVALID_REG) {
        emit_.mov_reg_reg(cpu_dst, direct);
    } else {
        // Load từ [RDI + index*8]
        emit_.mov_reg_mem(cpu_dst, RDI, vm_src * 8);
        // Unbox (bỏ Tag ở 16 bit cao)
        emit_.shift_imm(cpu_dst, 16, false, false); // SHL 16
        emit_.shift_imm(cpu_dst, 16, true, true);   // SAR 16 (giữ dấu)
    }
}

// Store: Từ CPU Reg (UNBOXED) -> VM Memory (hoặc Cache Reg)
void Compiler::store_vm_reg(int vm_dst, Reg cpu_src) {
    Reg direct = map_vm_reg(vm_dst);
    if (direct != INVALID_REG) {
        emit_.mov_reg_reg(direct, cpu_src);
    } else {
        // Box: OR với Tag
        emit_.mov_reg_imm(RDX, NanboxLayout::make_tag(2)); // RDX làm temp chứa TAG INT
        // Nếu cpu_src == RAX, ta cần save RAX hoặc dùng register khác?
        // Ở đây giả sử cpu_src là kết quả tính toán tạm thời, có thể modify.
        // Tuy nhiên tốt nhất là copy ra RAX để box rồi ghi.
        
        emit_.mov_reg_reg(RAX, cpu_src); // RAX = val
        emit_.alu_reg_reg(0x09, RAX, RDX); // OR RAX, RDX (Box)
        emit_.mov_mem_reg(RDI, vm_dst * 8, RAX); // Store [RDI...]
    }
}

void Compiler::emit_prologue() {
    emit_.push(RBX);
    emit_.push(R12); emit_.push(R13);
    emit_.push(R14); emit_.push(R15);

    // Pre-load các thanh ghi hot (0-4) và Unbox sẵn
    for (int i = 0; i <= 4; ++i) {
        Reg r = map_vm_reg(i);
        emit_.mov_reg_mem(r, RDI, i * 8);
        emit_.shift_imm(r, 16, false, false); // SHL
        emit_.shift_imm(r, 16, true, true);   // SAR
    }
}

void Compiler::emit_epilogue() {
    // Save ngược các thanh ghi hot về RAM (Box lại)
    for (int i = 0; i <= 4; ++i) {
        Reg r = map_vm_reg(i);
        emit_.mov_reg_reg(RAX, r);
        emit_.mov_reg_imm(RDX, NanboxLayout::make_tag(2));
        emit_.alu_reg_reg(0x09, RAX, RDX); // Box
        emit_.mov_mem_reg(RDI, i * 8, RAX);
    }
    
    emit_.pop(R15); emit_.pop(R14);
    emit_.pop(R13); emit_.pop(R12);
    emit_.pop(RBX);
    emit_.ret();
}

Compiler::JitFunc Compiler::compile(const uint8_t* bytecode, size_t len) {
    bc_to_native_.clear();
    fixups_.clear();
    
    // TODO: Reset emitter cursor hoặc tạo buffer mới mỗi lần compile
    // Ở đây giả định compile 1 lần rồi thôi hoặc quản lý buffer bên ngoài.
    
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
                if (r != INVALID_REG) {
                    emit_.mov_reg_imm(r, val);
                } else {
                    // Spill to RAM
                    emit_.mov_reg_imm(RAX, val | NanboxLayout::make_tag(2));
                    emit_.mov_mem_reg(RDI, dst * 8, RAX);
                }
                break;
            }

            case OpCode::MOVE: {
                uint16_t dst = read_u16();
                uint16_t src = read_u16();
                load_vm_reg(RAX, src);
                store_vm_reg(dst, RAX);
                break;
            }

            case OpCode::ADD: case OpCode::ADD_B: {
                bool b = (op == OpCode::ADD_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                load_vm_reg(RAX, r1);
                load_vm_reg(RCX, r2);
                emit_.alu_reg_reg(0x01, RAX, RCX); // ADD
                store_vm_reg(dst, RAX);
                break;
            }
            
            case OpCode::SUB: case OpCode::SUB_B: {
                bool b = (op == OpCode::SUB_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                load_vm_reg(RAX, r1);
                load_vm_reg(RCX, r2);
                emit_.alu_reg_reg(0x29, RAX, RCX); // SUB
                store_vm_reg(dst, RAX);
                break;
            }

            case OpCode::MUL: case OpCode::MUL_B: {
                bool b = (op == OpCode::MUL_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                load_vm_reg(RAX, r1);
                load_vm_reg(RCX, r2);
                emit_.imul_reg_reg(RAX, RCX); 
                store_vm_reg(dst, RAX);
                break;
            }

            case OpCode::DIV: case OpCode::DIV_B: {
                bool b = (op == OpCode::DIV_B);
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                load_vm_reg(RAX, r1);
                load_vm_reg(RCX, r2);
                emit_.cqo();          // Sign extend RAX -> RDX:RAX
                emit_.idiv_reg(RCX);  // Divide by RCX
                store_vm_reg(dst, RAX); // Quote
                break;
            }

            // Comparisons: EQ, NEQ, LT, GT...
            case OpCode::EQ: case OpCode::EQ_B:
            case OpCode::NEQ: case OpCode::NEQ_B:
            case OpCode::LT: case OpCode::LT_B: {
                bool b = (op >= OpCode::ADD_B); // check range thô
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1 = b ? read_u8() : read_u16();
                uint16_t r2 = b ? read_u8() : read_u16();
                
                load_vm_reg(RAX, r1);
                load_vm_reg(RCX, r2);
                emit_.alu_reg_reg(0x39, RCX, RAX); // CMP RAX, RCX (Lưu ý thứ tự dst/src của CMP)

                Condition cond;
                switch(op) {
                    case OpCode::EQ: case OpCode::EQ_B: cond = Condition::E; break;
                    case OpCode::NEQ: case OpCode::NEQ_B: cond = Condition::NE; break;
                    case OpCode::LT: case OpCode::LT_B: cond = Condition::L; break;
                    default: cond = Condition::E; break;
                }
                
                emit_.setcc(cond, RAX); // Kết quả 1 byte vào AL
                // Zero extend AL -> RAX (MOVZX)
                // Đơn giản nhất: AND RAX, 0xFF
                emit_.alu_reg_reg(0x21, RAX, RAX); // Không đúng, AND không có dạng movzx
                // Workaround: MOVZX bằng cách shift nếu lười implement movzx:
                // SHL 56, SHR 56? Không, setcc chỉ set byte thấp. 
                // Đúng chuẩn: MOVZX. Vì chưa có movzx trong emitter, ta dùng:
                // AND RAX, 0xFF (nhưng phải load 0xFF vào reg khác).
                emit_.mov_reg_imm(RCX, 0xFF);
                emit_.alu_reg_reg(0x21, RAX, RCX); // AND RAX, 0xFF

                store_vm_reg(dst, RAX);
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
                emit_.alu_reg_reg(0x21, RAX, RAX); // Test zero? No, AND. Use TEST logic implies CMP.
                // Emitter chưa có TEST, dùng CMP RAX, 0
                emit_.mov_reg_imm(RCX, 0);
                emit_.alu_reg_reg(0x39, RCX, RAX); // CMP RAX, 0

                // JNE (Not Equal 0) -> Jump if True
                fixups_.push_back({emit_.cursor(), (size_t)target, true});
                emit_.jcc(Condition::NE, 0);
                break;
            }

            case OpCode::HALT:
                emit_epilogue();
                break;

            default:
                emit_.ret(); // Fallback
                break;
        }
    }

    // Patch Jumps
    for (const auto& fix : fixups_) {
        size_t target_native = bc_to_native_[fix.target_bc];
        // JMP (E9 xx xx xx xx) -> 5 bytes
        // Jcc (0F 8x xx xx xx xx) -> 6 bytes
        size_t instr_len = fix.is_cond ? 6 : 5;
        size_t src_native = fix.jump_op_pos + instr_len; 
        
        int32_t rel = (int32_t)(target_native - src_native);
        // Patch vào offset +1 (cho JMP) hoặc +2 (cho Jcc)
        emit_.patch_u32(fix.jump_op_pos + (fix.is_cond ? 2 : 1), rel);
    }

    return (JitFunc)code_mem_;
}

} // namespace meow::jit

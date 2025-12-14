#pragma once

#include <vector>
#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <stdexcept>
#include <unordered_map>
#include <array>
#include <bit> 
#include <cassert>

#include <meow/value.h>
#include <meow/compiler/op_codes.h>
#include "meow_nanbox_layout.h" 

namespace meow {

class JitCompiler {
private:
    uint8_t* code_buffer_ = nullptr;
    size_t capacity_ = 1024 * 256; // Tăng dung lượng buffer
    size_t size_ = 0;
    
    struct JumpFixup {
        size_t instruction_pos;
        size_t target_bytecode_offset;
        bool is_short; 
    };
    std::vector<JumpFixup> fixups_;
    std::unordered_map<size_t, size_t> bytecode_to_native_map_;

    // --- CPU Register Mapping ---
    // Sử dụng callee-saved registers để giữ giá trị VM Regs 0-4
    static constexpr int CPU_RBX = 3;
    static constexpr int CPU_R12 = 12;
    static constexpr int CPU_R13 = 13;
    static constexpr int CPU_R14 = 14;
    static constexpr int CPU_R15 = 15;

    // Scratch registers (volatile)
    static constexpr int CPU_RAX = 0;
    static constexpr int CPU_RCX = 1;
    static constexpr int CPU_RDX = 2;
    static constexpr int CPU_RDI = 7; // Chứa con trỏ 'regs' (Argument 1)

    // Helper: Map VM Register -> CPU Register
    // Trả về -1 nếu register này không được cache (phải đọc từ RAM)
    [[nodiscard]] constexpr int map_vm_reg_to_cpu(int vm_reg) const noexcept {
        switch (vm_reg) {
            case 0: return CPU_RBX;
            case 1: return CPU_R12;
            case 2: return CPU_R13;
            case 3: return CPU_R14;
            case 4: return CPU_R15;
            default: return -1;
        }
    }

public:
    JitCompiler() {
        // PROT_EXEC là bắt buộc
        code_buffer_ = (uint8_t*)mmap(nullptr, capacity_, 
                                      PROT_READ | PROT_WRITE | PROT_EXEC, 
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (code_buffer_ == MAP_FAILED) throw std::runtime_error("JIT mmap failed");
    }

    ~JitCompiler() {
        if (code_buffer_) munmap(code_buffer_, capacity_);
    }

    // --- Low-level Emitter ---

    void emit(uint8_t b) { 
        if (size_ >= capacity_) throw std::runtime_error("JIT buffer overflow");
        code_buffer_[size_++] = b; 
    }
    
    void emit_u32(uint32_t v) { 
        if (size_ + 4 > capacity_) throw std::runtime_error("JIT buffer overflow");
        std::memcpy(code_buffer_ + size_, &v, 4); 
        size_ += 4; 
    }
    
    void emit_u64(uint64_t v) { 
        if (size_ + 8 > capacity_) throw std::runtime_error("JIT buffer overflow");
        std::memcpy(code_buffer_ + size_, &v, 8); 
        size_ += 8; 
    }

    // REX prefix: Cho phép truy cập 64-bit và register r8-r15
    void emit_rex(bool w, bool r, bool x, bool b) {
        uint8_t rex = 0x40;
        if (w) rex |= 0x08; // 64-bit operand size
        if (r) rex |= 0x04; // Extension of ModRM reg field
        if (x) rex |= 0x02; // Extension of SIB index field
        if (b) rex |= 0x01; // Extension of ModRM r/m field
        emit(rex);
    }

    // Emit ModR/M byte
    void emit_modrm(int mode, int reg, int rm) {
        emit((mode << 6) | ((reg & 7) << 3) | (rm & 7));
    }

    // --- Operand Abstraction ---

    // MOV reg64, imm64
    void emit_mov_reg_imm(int dst, uint64_t imm) {
        bool r12_15 = dst >= 8;
        if (imm <= 0xFFFFFFFF && (imm & 0x80000000) == 0) {
            // Optim: MOV r32, imm32 (zero extends to 64-bit)
            if (r12_15) emit(0x41); 
            emit(0xB8 | (dst & 7));
            emit_u32((uint32_t)imm);
        } else if ((int64_t)imm >= -2147483648LL && (int64_t)imm <= 2147483647LL) {
             // Optim: MOV r64, sign_extended_imm32 (C7 /0)
             emit_rex(true, false, false, r12_15);
             emit(0xC7);
             emit_modrm(3, 0, dst);
             emit_u32((uint32_t)imm);
        } else {
            // Full 64-bit load
            emit_rex(true, false, false, r12_15);
            emit(0xB8 | (dst & 7));
            emit_u64(imm);
        }
    }

    // MOV dst, src
    void emit_mov_reg_reg(int dst, int src) {
        if (dst == src) return;
        emit_rex(true, dst >= 8, false, src >= 8);
        emit(0x8B); 
        emit_modrm(3, dst, src);
    }

    // ALU op: dst = dst OP src
    // 0x01: ADD, 0x29: SUB, 0x21: AND, 0x09: OR, 0x31: XOR, 0x39: CMP
    void emit_alu_reg_reg(uint8_t opcode, int dst, int src) {
        emit_rex(true, src >= 8, false, dst >= 8);
        emit(opcode); 
        emit_modrm(3, src, dst); // Lưu ý: chiều op src, dst phụ thuộc opcode
    }

    // IMUL dst, src (Signed Multiply)
    void emit_imul_reg_reg(int dst, int src) {
        emit_rex(true, dst >= 8, false, src >= 8);
        emit(0x0F); emit(0xAF);
        emit_modrm(3, dst, src);
    }

    // IDIV/DIV src (RAX = RDX:RAX / src)
    void emit_idiv_reg(int src) {
        emit_rex(true, false, false, src >= 8);
        emit(0xF7);
        emit_modrm(3, 7, src); // /7 ch extension cho IDIV
    }

    // Shift Ops: SAR, SHL (dst = dst shift by cl)
    void emit_shift_cl(int dst, bool right, bool arithmetic) {
        emit_rex(true, false, false, dst >= 8);
        emit(0xD3); // Shift by CL
        // /4 for SHL, /7 for SAR, /5 for SHR
        int extension = right ? (arithmetic ? 7 : 5) : 4; 
        emit_modrm(3, extension, dst);
    }

    // Unary Ops: NEG, NOT
    void emit_unary(int dst, int extension) {
        emit_rex(true, false, false, dst >= 8);
        emit(0xF7);
        emit_modrm(3, extension, dst);
    }

    // --- High Level VM Helpers ---

    // Load giá trị từ VM reg vào CPU reg (và UNBOX nếu cần)
    // - Nếu VM reg nằm trong cache (RBX...), nó đã unboxed -> chỉ cần mov.
    // - Nếu VM reg nằm trong RAM -> load và unbox.
    void load_vm_reg_to_cpu(int cpu_dst, int vm_src) {
        int mapped = map_vm_reg_to_cpu(vm_src);
        if (mapped != -1) {
            emit_mov_reg_reg(cpu_dst, mapped);
        } else {
            // Load from memory: MOV cpu_dst, [RDI + vm_src * 8]
            emit_rex(true, cpu_dst >= 8, false, false);
            emit(0x8B); 
            if (vm_src * 8 < 128) { // Short displacement
                emit_modrm(1, cpu_dst, CPU_RDI); 
                emit(vm_src * 8);
            } else { // Long displacement
                emit_modrm(2, cpu_dst, CPU_RDI);
                emit_u32(vm_src * 8);
            }
            // Unbox: SHL 16, SAR 16 (loại bỏ tag Nanbox)
            emit_rex(true, false, false, cpu_dst >= 8);
            emit(0xC1); emit_modrm(3, 4, cpu_dst); emit(16); // SHL
            emit_rex(true, false, false, cpu_dst >= 8);
            emit(0xC1); emit_modrm(3, 7, cpu_dst); emit(16); // SAR
        }
    }

    // Store giá trị từ CPU reg vào VM reg (và BOX nếu cần)
    // - Nếu VM reg nằm trong cache -> chỉ cần mov (giữ unboxed).
    // - Nếu VM reg nằm trong RAM -> box và store.
    void store_cpu_to_vm_reg(int vm_dst, int cpu_src) {
        int mapped = map_vm_reg_to_cpu(vm_dst);
        if (mapped != -1) {
            emit_mov_reg_reg(mapped, cpu_src);
        } else {
            // Box trước khi ghi ra RAM
            // Tag logic: OR cpu_src, TAG_CONST
            // Chúng ta mượn tạm CPU_RAX hoặc CPU_RDX để tính toán nếu cần, 
            // nhưng ở đây giả sử cpu_src là thanh ghi tạm có thể bị thay đổi.
            
            // 1. Move value to temp register (if cpu_src is not temp)
            // Để an toàn, ta dùng một thanh ghi trung gian hoặc xử lý trực tiếp.
            // Ở đây ta giả định cpu_src là kết quả tính toán, ta có thể modify nó để box.
            
            // Load TAG (0x7FFC...0000)
            uint64_t tag = NanboxLayout::make_tag(2); // INT_TAG
            emit_mov_reg_imm(CPU_RDX, tag); // RDX làm temp chứa tag
            
            emit_rex(true, cpu_src >= 8, false, false); 
            emit(0x09); emit_modrm(3, CPU_RDX, cpu_src); // OR cpu_src, RDX
            
            // MOV [RDI + vm_dst * 8], cpu_src
            emit_rex(true, cpu_src >= 8, false, false);
            emit(0x89);
            if (vm_dst * 8 < 128) {
                emit_modrm(1, cpu_src, CPU_RDI);
                emit(vm_dst * 8);
            } else {
                emit_modrm(2, cpu_src, CPU_RDI);
                emit_u32(vm_dst * 8);
            }
        }
    }

    using JitFunc = void (*)(Value* regs);

    JitFunc compile(const uint8_t* bytecode, size_t len) {
        size_t pc = 0;
        fixups_.clear();
        bytecode_to_native_map_.clear();

        // --- PROLOGUE ---
        // Save callee-saved registers
        emit(0x53);             // push rbx
        emit(0x41); emit(0x54); // push r12
        emit(0x41); emit(0x55); // push r13
        emit(0x41); emit(0x56); // push r14
        emit(0x41); emit(0x57); // push r15

        // Load & Unbox Hot VM Registers (0-4) vào CPU Registers
        for (int i = 0; i <= 4; ++i) {
            int cpu_reg = map_vm_reg_to_cpu(i);
            // Load raw from memory
            emit_rex(true, cpu_reg >= 8, false, false);
            emit(0x8B); emit_modrm(1, cpu_reg, CPU_RDI); emit(i * 8);
            // Unbox
            emit_rex(true, false, false, cpu_reg >= 8);
            emit(0xC1); emit_modrm(3, 4, cpu_reg); emit(16); // SHL 16
            emit_rex(true, false, false, cpu_reg >= 8);
            emit(0xC1); emit_modrm(3, 7, cpu_reg); emit(16); // SAR 16
        }

        while (pc < len) {
            bytecode_to_native_map_[pc] = size_;
            OpCode op = static_cast<OpCode>(bytecode[pc++]);

            // Helper macro để đọc tham số
            auto read_u16 = [&]() { uint16_t v; memcpy(&v, bytecode + pc, 2); pc += 2; return v; };
            auto read_u8 = [&]() { return bytecode[pc++]; };
            
            switch (op) {
                case OpCode::LOAD_INT: {
                    uint16_t dst = read_u16();
                    int64_t val; memcpy(&val, bytecode + pc, 8); pc += 8;
                    // Với LOAD_INT, ta load trực tiếp giá trị unboxed
                    int cpu_reg = map_vm_reg_to_cpu(dst);
                    if (cpu_reg != -1) {
                        emit_mov_reg_imm(cpu_reg, val);
                    } else {
                        // Nếu spill ra RAM, phải Box luôn
                        emit_mov_reg_imm(CPU_RAX, val | NanboxLayout::make_tag(2));
                        // Store [RDI + dst*8]
                        emit_rex(true, false, false, false);
                        emit(0x89); emit_modrm(2, CPU_RAX, CPU_RDI); emit_u32(dst * 8);
                    }
                    break;
                }

                case OpCode::MOVE: {
                    uint16_t dst = read_u16();
                    uint16_t src = read_u16();
                    load_vm_reg_to_cpu(CPU_RAX, src);
                    store_cpu_to_vm_reg(dst, CPU_RAX);
                    break;
                }

                // --- Binary Ops ---
                case OpCode::ADD: case OpCode::SUB: case OpCode::MUL:
                case OpCode::BIT_AND: case OpCode::BIT_OR: case OpCode::BIT_XOR:
                case OpCode::ADD_B: case OpCode::SUB_B: case OpCode::MUL_B: {
                    bool is_byte = (op >= OpCode::ADD_B);
                    uint16_t dst = is_byte ? read_u8() : read_u16();
                    uint16_t r1  = is_byte ? read_u8() : read_u16();
                    uint16_t r2  = is_byte ? read_u8() : read_u16();

                    load_vm_reg_to_cpu(CPU_RAX, r1); // LHS in RAX
                    load_vm_reg_to_cpu(CPU_RCX, r2); // RHS in RCX
                    
                    switch (op) {
                        case OpCode::ADD: case OpCode::ADD_B: emit_alu_reg_reg(0x01, CPU_RAX, CPU_RCX); break; // ADD
                        case OpCode::SUB: case OpCode::SUB_B: emit_alu_reg_reg(0x29, CPU_RAX, CPU_RCX); break; // SUB
                        case OpCode::MUL: case OpCode::MUL_B: emit_imul_reg_reg(CPU_RAX, CPU_RCX); break; // IMUL
                        case OpCode::BIT_AND: emit_alu_reg_reg(0x21, CPU_RAX, CPU_RCX); break; // AND
                        case OpCode::BIT_OR:  emit_alu_reg_reg(0x09, CPU_RAX, CPU_RCX); break; // OR
                        case OpCode::BIT_XOR: emit_alu_reg_reg(0x31, CPU_RAX, CPU_RCX); break; // XOR
                        default: break;
                    }
                    store_cpu_to_vm_reg(dst, CPU_RAX);
                    break;
                }

                // --- Division / Modulo (Special handling) ---
                case OpCode::DIV: case OpCode::DIV_B:
                case OpCode::MOD: case OpCode::MOD_B: {
                    bool is_byte = (op == OpCode::DIV_B || op == OpCode::MOD_B);
                    uint16_t dst = is_byte ? read_u8() : read_u16();
                    uint16_t r1  = is_byte ? read_u8() : read_u16();
                    uint16_t r2  = is_byte ? read_u8() : read_u16();

                    load_vm_reg_to_cpu(CPU_RAX, r1); // Dividend -> RAX
                    load_vm_reg_to_cpu(CPU_RCX, r2); // Divisor -> RCX

                    emit_rex(true, false, false, false);
                    emit(0x99); // CQO: Sign-extend RAX into RDX:RAX
                    emit_idiv_reg(CPU_RCX); // IDIV RCX

                    // Result: RAX = Quote, RDX = Remainder
                    if (op == OpCode::DIV || op == OpCode::DIV_B) {
                        store_cpu_to_vm_reg(dst, CPU_RAX);
                    } else {
                        store_cpu_to_vm_reg(dst, CPU_RDX);
                    }
                    break;
                }

                // --- Shifts ---
                case OpCode::LSHIFT: case OpCode::RSHIFT: {
                    uint16_t dst = read_u16();
                    uint16_t r1  = read_u16();
                    uint16_t r2  = read_u16();
                    
                    load_vm_reg_to_cpu(CPU_RAX, r1); // Value
                    load_vm_reg_to_cpu(CPU_RCX, r2); // Shift amount (vào CL)
                    
                    emit_shift_cl(CPU_RAX, (op == OpCode::RSHIFT), true); // SAR / SHL
                    store_cpu_to_vm_reg(dst, CPU_RAX);
                    break;
                }

                // --- Unary ---
                case OpCode::NEG: case OpCode::BIT_NOT: {
                    uint16_t dst = read_u16();
                    uint16_t src = read_u16();
                    load_vm_reg_to_cpu(CPU_RAX, src);
                    emit_unary(CPU_RAX, (op == OpCode::NEG) ? 3 : 2); // 3=NEG, 2=NOT
                    store_cpu_to_vm_reg(dst, CPU_RAX);
                    break;
                }

                // --- Comparisons ---
                case OpCode::EQ: case OpCode::EQ_B:
                case OpCode::NEQ: case OpCode::NEQ_B:
                case OpCode::LT: case OpCode::LT_B:
                case OpCode::LE: case OpCode::LE_B:
                case OpCode::GT: case OpCode::GT_B:
                case OpCode::GE: case OpCode::GE_B: {
                    bool is_byte = (op >= OpCode::ADD_B); // Check range roughly or explicit
                    is_byte = (op == OpCode::EQ_B || op == OpCode::NEQ_B || op == OpCode::LT_B || 
                               op == OpCode::LE_B || op == OpCode::GT_B || op == OpCode::GE_B);

                    uint16_t dst = is_byte ? read_u8() : read_u16();
                    uint16_t r1  = is_byte ? read_u8() : read_u16();
                    uint16_t r2  = is_byte ? read_u8() : read_u16();

                    load_vm_reg_to_cpu(CPU_RAX, r1);
                    load_vm_reg_to_cpu(CPU_RCX, r2);
                    emit_alu_reg_reg(0x39, CPU_RCX, CPU_RAX); // CMP RAX, RCX

                    // Set byte cond
                    emit(0x0F);
                    switch (op) {
                        case OpCode::EQ: case OpCode::EQ_B:   emit(0x94); break; // SETE
                        case OpCode::NEQ: case OpCode::NEQ_B: emit(0x95); break; // SETNE
                        case OpCode::LT: case OpCode::LT_B:   emit(0x9C); break; // SETL
                        case OpCode::LE: case OpCode::LE_B:   emit(0x9E); break; // SETLE
                        case OpCode::GT: case OpCode::GT_B:   emit(0x9F); break; // SETG
                        case OpCode::GE: case OpCode::GE_B:   emit(0x9D); break; // SETGE
                        default: break;
                    }
                    // MOVZX RAX, AL
                    emit_rex(true, false, false, false);
                    emit(0x0F); emit(0xB6); emit(0xC0);
                    
                    store_cpu_to_vm_reg(dst, CPU_RAX);
                    break;
                }

                // --- Jumps ---
                case OpCode::JUMP: {
                    uint16_t target = read_u16();
                    if (bytecode_to_native_map_.count(target)) {
                        // Backward jump
                        size_t target_addr = bytecode_to_native_map_[target];
                        int32_t rel = (int32_t)(target_addr - (size_ + 5));
                        emit(0xE9); emit_u32(rel);
                    } else {
                        // Forward jump
                        emit(0xE9);
                        fixups_.push_back({size_, (size_t)target, false});
                        emit_u32(0);
                    }
                    break;
                }

                case OpCode::JUMP_IF_TRUE: case OpCode::JUMP_IF_TRUE_B:
                case OpCode::JUMP_IF_FALSE: case OpCode::JUMP_IF_FALSE_B: {
                    bool is_byte = (op == OpCode::JUMP_IF_TRUE_B || op == OpCode::JUMP_IF_FALSE_B);
                    uint16_t reg = is_byte ? read_u8() : read_u16();
                    uint16_t target = read_u16();

                    load_vm_reg_to_cpu(CPU_RAX, reg);
                    emit_rex(true, false, false, false);
                    emit(0x85); emit(0xC0); // TEST RAX, RAX

                    // JNE (75) if TRUE (not zero), JE (74) if FALSE (zero)
                    bool jump_on_true = (op == OpCode::JUMP_IF_TRUE || op == OpCode::JUMP_IF_TRUE_B);
                    
                    // Ta dùng Near Jump (0F 8x) để an toàn khoảng cách
                    emit(0x0F); 
                    emit(jump_on_true ? 0x85 : 0x84); // JNE or JE

                    if (bytecode_to_native_map_.count(target)) {
                        size_t target_addr = bytecode_to_native_map_[target];
                        int32_t rel = (int32_t)(target_addr - (size_ + 4));
                        emit_u32(rel);
                    } else {
                        fixups_.push_back({size_, (size_t)target, false});
                        emit_u32(0);
                    }
                    break;
                }

                case OpCode::HALT: {
                    // Box & Store Hot Registers back to Memory
                    for (int i = 0; i <= 4; ++i) {
                        int cpu_reg = map_vm_reg_to_cpu(i);
                        // Move to Temp (RAX)
                        emit_mov_reg_reg(CPU_RAX, cpu_reg);
                        // Box
                        uint64_t tag = NanboxLayout::make_tag(2);
                        emit_mov_reg_imm(CPU_RDX, tag);
                        emit_rex(true, false, false, false);
                        emit(0x09); emit_modrm(3, CPU_RDX, CPU_RAX); // OR RAX, RDX
                        // Store
                        emit_rex(true, false, false, false);
                        emit(0x89); emit_modrm(1, CPU_RAX, CPU_RDI); emit(i * 8);
                    }

                    // --- EPILOGUE ---
                    emit(0x41); emit(0x5F); // pop r15
                    emit(0x41); emit(0x5E); // pop r14
                    emit(0x41); emit(0x5D); // pop r13
                    emit(0x41); emit(0x5C); // pop r12
                    emit(0x5B);             // pop rbx
                    emit(0xC3);             // ret
                    break;
                }

                default:
                    // Với Opcode chưa support (ví dụ: CALL, NEW_OBJECT), ta tạm thời RET
                    // Trong thực tế cần fallback về Interpreter hoặc throw
                    emit(0xC3); 
                    break;
            }
        }

        // Patch Fixups
        for (auto& fix : fixups_) {
            size_t target_native = bytecode_to_native_map_[fix.target_bytecode_offset];
            size_t src_native = fix.instruction_pos + 4; // Offset tính từ sau lệnh nhảy 4 byte
            int32_t rel = (int32_t)(target_native - src_native);
            std::memcpy(code_buffer_ + fix.instruction_pos, &rel, 4);
        }

        return (JitFunc)code_buffer_;
    }
};

} // namespace meow
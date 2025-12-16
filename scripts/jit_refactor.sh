#!/bin/bash

# Dừng script nếu có lỗi xảy ra
set -e

echo "Dang khoi tao cau truc thu muc jit/x64..."
mkdir -p jit/x64

# ==============================================================================
# 1. Tạo file jit/x64/memory.h
# Quản lý bộ nhớ thực thi (Executable Memory)
# ==============================================================================
cat << 'EOF' > jit/x64/memory.h
#pragma once
#include <cstdint>
#include <cstddef>

namespace meow::jit::x64 {

class ExecutableMemory {
public:
    ExecutableMemory(size_t capacity);
    ~ExecutableMemory();
    
    // Xóa copy constructor để tránh double-free
    ExecutableMemory(const ExecutableMemory&) = delete;
    ExecutableMemory& operator=(const ExecutableMemory&) = delete;

    uint8_t* data() const { return ptr_; }
    size_t capacity() const { return capacity_; }

private:
    uint8_t* ptr_ = nullptr;
    size_t capacity_;
};

} // namespace meow::jit::x64
EOF

# ==============================================================================
# 2. Tạo file jit/x64/memory.cpp
# Implementation của mmap/munmap
# ==============================================================================
cat << 'EOF' > jit/x64/memory.cpp
#include "jit/x64/memory.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdexcept>

namespace meow::jit::x64 {

static constexpr size_t PAGE_SIZE = 4096;

ExecutableMemory::ExecutableMemory(size_t capacity) : capacity_(capacity) {
    // Align capacity theo PAGE_SIZE
    if (capacity_ % PAGE_SIZE != 0) {
        capacity_ = (capacity_ / PAGE_SIZE + 1) * PAGE_SIZE;
    }

#if defined(__linux__) || defined(__APPLE__)
    ptr_ = (uint8_t*)mmap(nullptr, capacity_, 
                          PROT_READ | PROT_WRITE | PROT_EXEC, 
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr_ == MAP_FAILED) {
        throw std::runtime_error("Failed to allocate executable memory");
    }
#else
    // Fallback cho non-POSIX (ví dụ Windows nếu chưa port VirtualAlloc)
    ptr_ = new uint8_t[capacity_]; 
#endif
}

ExecutableMemory::~ExecutableMemory() {
#if defined(__linux__) || defined(__APPLE__)
    if (ptr_) munmap(ptr_, capacity_);
#else
    if (ptr_) delete[] ptr_;
#endif
}

} // namespace meow::jit::x64
EOF

# ==============================================================================
# 3. Tạo file jit/x64/compiler.h
# Header chính, khai báo các helper functions mới
# ==============================================================================
cat << 'EOF' > jit/x64/compiler.h
#pragma once
#include "jit/x64/emitter.h"
#include "jit/x64/memory.h"
#include <meow/value.h> // Để lấy layout_traits
#include <meow/bytecode/op_codes.h>
#include <vector>
#include <unordered_map>

namespace meow { struct VMState; }

namespace meow::jit::x64 {

class Compiler {
public:
    using JitFunc = void (*)(Value* regs, const Value* consts, VMState* state);

    Compiler();
    ~Compiler() = default;

    JitFunc compile(const uint8_t* bytecode, size_t len);

private:
    // Quản lý bộ nhớ
    ExecutableMemory code_mem_{1024 * 256}; // Mặc định 256KB
    Emitter emit_;
    
    struct Fixup {
        size_t jump_op_pos;
        size_t target_bc;
        bool is_cond;
    };
    std::vector<Fixup> fixups_;
    std::unordered_map<size_t, size_t> bc_to_native_;

    // --- Core Registers Helpers (trong compiler.cpp) ---
    Reg map_vm_reg(int vm_reg) const;
    void load_vm_reg(Reg cpu_dst, int vm_src);
    void store_vm_reg(int vm_dst, Reg cpu_src);
    void emit_prologue();
    void emit_epilogue();

    // --- Instruction Handlers (Tách file) ---
    
    // Trong math_ops.cpp
    void emit_load_const(uint16_t dst, uint16_t idx);
    void emit_load_int(uint16_t dst, int64_t val);
    void emit_move(uint16_t dst, uint16_t src);
    void emit_math_op(OpCode op, uint16_t dst, uint16_t r1, uint16_t r2, bool is_b, const uint8_t* bytecode, size_t& ip);

    // Trong control_flow.cpp
    Condition emit_cmp_logic(OpCode op_cmp, uint16_t r1, uint16_t r2);
    void emit_compare_and_set(OpCode op, uint16_t dst, uint16_t r1, uint16_t r2, bool is_b, const uint8_t* bytecode, size_t& ip, size_t len);
    void emit_jump(size_t target_bc, size_t current_ip, const uint8_t* bytecode, size_t len); // Xử lý loop peeling
    void emit_cond_jump(OpCode op, uint8_t reg, uint16_t off);
};

} // namespace meow::jit::x64
EOF

# ==============================================================================
# 4. Tạo file jit/x64/compiler.cpp
# Main loop, Register logic, Prologue/Epilogue
# ==============================================================================
cat << 'EOF' > jit/x64/compiler.cpp
#include "jit/x64/compiler.h"
#include <cstring>
#include <iostream>

namespace meow::jit::x64 {

// Sử dụng layout_traits từ Value thay vì include nanbox nội bộ
static constexpr uint64_t NANBOX_INT_TAG = meow::Value::layout_traits::make_tag(2);

static constexpr Reg REG_VM_REGS  = RDI; 
static constexpr Reg REG_CONSTS   = RSI; 
// static constexpr Reg REG_STATE = RDX; // Chưa dùng

static constexpr Reg RAX_REG = RAX;
static constexpr Reg RCX_REG = RCX;

#define MEM_REG(idx) REG_VM_REGS, (idx) * 8

Compiler::Compiler() : emit_(code_mem_.data(), code_mem_.capacity()) {}

// --- Register Helpers ---
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

void Compiler::load_vm_reg(Reg cpu_dst, int vm_src) {
    Reg mapped = map_vm_reg(vm_src);
    if (mapped != INVALID_REG) {
        if (mapped != cpu_dst) emit_.mov(cpu_dst, mapped);
    } else {
        emit_.mov(cpu_dst, MEM_REG(vm_src));
        emit_.shl(cpu_dst, 16);
        emit_.sar(cpu_dst, 16);
    }
}

void Compiler::store_vm_reg(int vm_dst, Reg cpu_src) {
    Reg mapped = map_vm_reg(vm_dst);
    if (mapped != INVALID_REG) {
        if (mapped != cpu_src) emit_.mov(mapped, cpu_src);
    } else {
        emit_.mov(MEM_REG(vm_dst), cpu_src); 
    }
}

// --- Main Compile Loop ---
Compiler::JitFunc Compiler::compile(const uint8_t* bytecode, size_t len) {
    // Reset emitter về đầu buffer
    emit_ = Emitter(code_mem_.data(), code_mem_.capacity());
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

        switch (op) {
            case OpCode::LOAD_CONST: {
                emit_load_const(read_u16(), read_u16());
                break;
            }
            case OpCode::LOAD_INT: {
                uint16_t dst = read_u16();
                int64_t val; std::memcpy(&val, bytecode + ip, 8); ip += 8;
                emit_load_int(dst, val);
                break;
            }
            case OpCode::MOVE: {
                emit_move(read_u16(), read_u16());
                break;
            }

            // Math Ops
            case OpCode::ADD: case OpCode::ADD_B:
            case OpCode::SUB: case OpCode::SUB_B:
            case OpCode::MUL: case OpCode::MUL_B: {
                bool is_b = (op == OpCode::ADD_B || op == OpCode::SUB_B || op == OpCode::MUL_B);
                uint16_t dst = is_b ? read_u8() : read_u16();
                uint16_t r1  = is_b ? read_u8() : read_u16();
                uint16_t r2  = is_b ? read_u8() : read_u16();
                emit_math_op(op, dst, r1, r2, is_b, bytecode, ip);
                break;
            }

            // Comparisons
            case OpCode::EQ: case OpCode::EQ_B:
            case OpCode::NEQ: case OpCode::NEQ_B:
            case OpCode::LT: case OpCode::LT_B:
            case OpCode::LE: case OpCode::LE_B:
            case OpCode::GT: case OpCode::GT_B:
            case OpCode::GE: case OpCode::GE_B: {
                bool is_b = (op >= OpCode::ADD_B); // Check range simple
                 if (op == OpCode::LT_B || op == OpCode::GT_B || op == OpCode::LE_B || op == OpCode::GE_B || op == OpCode::EQ_B || op == OpCode::NEQ_B) is_b = true;
                
                uint16_t dst = is_b ? read_u8() : read_u16();
                uint16_t r1  = is_b ? read_u8() : read_u16();
                uint16_t r2  = is_b ? read_u8() : read_u16();
                
                emit_compare_and_set(op, dst, r1, r2, is_b, bytecode, ip, len);
                break;
            }

            // Control Flow
            case OpCode::JUMP: {
                size_t current_ip = ip - 1; // IP của lệnh JUMP
                uint16_t off = read_u16();
                emit_jump((size_t)off, current_ip, bytecode, len);
                break;
            }
            case OpCode::JUMP_IF_TRUE_B: 
            case OpCode::JUMP_IF_FALSE_B: {
                emit_cond_jump(op, read_u8(), read_u16());
                break;
            }

            case OpCode::RETURN: case OpCode::HALT:
                emit_epilogue();
                break;

            default: break;
        }
    }

    // Patch các điểm nhảy (backpatching)
    for (const auto& fix : fixups_) {
        if (bc_to_native_.count(fix.target_bc)) {
            size_t target_native = bc_to_native_[fix.target_bc];
            size_t jump_end = fix.jump_op_pos + (fix.is_cond ? 6 : 5);
            int32_t rel = (int32_t)(target_native - jump_end);
            emit_.patch_u32(fix.jump_op_pos + (fix.is_cond ? 2 : 1), rel);
        }
    }
    
    return reinterpret_cast<JitFunc>(code_mem_.data());
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
EOF

# ==============================================================================
# 5. Tạo file jit/x64/math_ops.cpp
# Xử lý các lệnh toán học và di chuyển dữ liệu
# ==============================================================================
cat << 'EOF' > jit/x64/math_ops.cpp
#include "jit/x64/compiler.h"

namespace meow::jit::x64 {

static constexpr Reg REG_CONSTS = RSI; 
#define MEM_CONST(idx) REG_CONSTS, (idx) * 8
static constexpr Reg RAX_REG = RAX;
static constexpr Reg RCX_REG = RCX;

void Compiler::emit_load_const(uint16_t dst, uint16_t idx) {
    emit_.mov(RAX_REG, MEM_CONST(idx));
    store_vm_reg(dst, RAX_REG);
}

void Compiler::emit_load_int(uint16_t dst, int64_t val) {
    Reg mapped = map_vm_reg(dst);
    if (mapped != INVALID_REG) emit_.mov(mapped, val);
    else { emit_.mov(RAX_REG, val); store_vm_reg(dst, RAX_REG); }
}

void Compiler::emit_move(uint16_t dst, uint16_t src) {
    Reg dst_reg = map_vm_reg(dst);
    Reg src_reg = map_vm_reg(src);
    if (dst_reg != INVALID_REG && src_reg != INVALID_REG) emit_.mov(dst_reg, src_reg);
    else { load_vm_reg(RAX_REG, src); store_vm_reg(dst, RAX_REG); }
}

void Compiler::emit_math_op(OpCode op, uint16_t dst, uint16_t r1, uint16_t r2, bool is_b, const uint8_t*, size_t&) {
    Reg r1_reg = map_vm_reg(r1);
    if (r1_reg == INVALID_REG) { load_vm_reg(RAX_REG, r1); r1_reg = RAX_REG; }
    
    Reg r2_reg = map_vm_reg(r2);
    if (r2_reg == INVALID_REG) { load_vm_reg(RCX_REG, r2); r2_reg = RCX_REG; }
    
    Reg dst_reg = map_vm_reg(dst);

    // Lambda helper để gọi đúng hàm ALU
    auto do_alu = [&](auto func) {
        if (dst_reg != INVALID_REG) {
            if (dst_reg != r1_reg) emit_.mov(dst_reg, r1_reg);
            (emit_.*func)(dst_reg, r2_reg);
        } else {
            if (r1_reg != RAX_REG) emit_.mov(RAX_REG, r1_reg);
            (emit_.*func)(RAX_REG, r2_reg);
            store_vm_reg(dst, RAX_REG);
        }
    };

    if (op == OpCode::ADD || op == OpCode::ADD_B) do_alu(&Emitter::add);
    else if (op == OpCode::SUB || op == OpCode::SUB_B) do_alu(&Emitter::sub);
    else if (op == OpCode::MUL || op == OpCode::MUL_B) do_alu(&Emitter::imul);
}

} // namespace meow::jit::x64
EOF

# ==============================================================================
# 6. Tạo file jit/x64/control_flow.cpp
# Xử lý Jump, Compare, Loop Optimizations
# ==============================================================================
cat << 'EOF' > jit/x64/control_flow.cpp
#include "jit/x64/compiler.h"
#include <cstring>

namespace meow::jit::x64 {

static constexpr Reg RAX_REG = RAX;
static constexpr Reg RCX_REG = RCX;

Condition Compiler::emit_cmp_logic(OpCode op_cmp, uint16_t r1, uint16_t r2) {
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
}

void Compiler::emit_compare_and_set(OpCode op, uint16_t dst, uint16_t r1, uint16_t r2, bool, const uint8_t* bytecode, size_t& ip, size_t len) {
    Condition base_cond = emit_cmp_logic(op, r1, r2);

    // === LOOP HEADER OPTIMIZATION (Fusion) ===
    // Kiểm tra xem lệnh tiếp theo có phải là Jump If dựa trên kết quả so sánh này không
    size_t next_ip = ip;
    if (next_ip < len) {
        OpCode next_op = static_cast<OpCode>(bytecode[next_ip]);
        
        auto try_fuse = [&](bool on_true, size_t skip_bytes) -> bool {
            uint8_t cond_reg;
            if (skip_bytes == 4) cond_reg = bytecode[next_ip+1]; // _B variant
            else { uint16_t r; std::memcpy(&r, bytecode+next_ip+1, 2); cond_reg = (uint8_t)r; }
            
            // Nếu Jump kiểm tra đúng register `dst` vừa so sánh
            if (cond_reg == dst) {
                uint16_t off; 
                std::memcpy(&off, bytecode + next_ip + (skip_bytes - 2), 2);
                size_t target = (size_t)off;
                
                // Đảo điều kiện nếu cần
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
                
                // Bỏ qua lệnh Jump trong bytecode vì ta đã emit lệnh nhảy Native rồi
                ip = next_ip + skip_bytes;
                return true;
            }
            return false;
        };

        if (next_op == OpCode::JUMP_IF_TRUE_B && try_fuse(true, 4)) return;
        if (next_op == OpCode::JUMP_IF_FALSE_B && try_fuse(false, 4)) return;
        if (next_op == OpCode::JUMP_IF_TRUE && try_fuse(true, 5)) return;
        if (next_op == OpCode::JUMP_IF_FALSE && try_fuse(false, 5)) return;
    }
    
    // Nếu không fuse được, thực hiện setcc như bình thường
    emit_.setcc(base_cond, RAX_REG); 
    emit_.movzx_b(RAX_REG, RAX_REG); 
    store_vm_reg(dst, RAX_REG);
}

void Compiler::emit_jump(size_t target_bc, size_t current_ip, const uint8_t* bytecode, size_t) {
    // === LOOP ROTATION (Peeling) ===
    // Nếu nhảy ngược về trước (potential loop)
    if (target_bc < current_ip) {
        OpCode target_op = static_cast<OpCode>(bytecode[target_bc]);
        
        // Kiểm tra target có phải là lệnh so sánh không
        bool is_cmp = (target_op >= OpCode::EQ && target_op <= OpCode::GE_B); 

        if (is_cmp) {
            size_t t_ip = target_bc + 1; 
            bool is_b = (target_op >= OpCode::ADD_B); // Simplification, should check properly
             if (target_op == OpCode::LT_B || target_op == OpCode::GT_B || target_op == OpCode::LE_B || target_op == OpCode::GE_B || target_op == OpCode::EQ_B || target_op == OpCode::NEQ_B) is_b = true;

            // Đọc operand của lệnh so sánh tại target
            uint16_t t_dst = is_b ? bytecode[t_ip++] : (t_ip+=2, *(uint16_t*)(bytecode+t_ip-2));
            uint16_t t_r1  = is_b ? bytecode[t_ip++] : (t_ip+=2, *(uint16_t*)(bytecode+t_ip-2));
            uint16_t t_r2  = is_b ? bytecode[t_ip++] : (t_ip+=2, *(uint16_t*)(bytecode+t_ip-2));

            OpCode next_op = static_cast<OpCode>(bytecode[t_ip]);
            bool is_jmp_if = (next_op == OpCode::JUMP_IF_TRUE_B || next_op == OpCode::JUMP_IF_FALSE_B);
            
            if (is_jmp_if) {
                uint8_t cond_reg = bytecode[t_ip + 1];
                if (cond_reg == t_dst) {
                    // PHÁT HIỆN LOOP HEADER -> ROTATE
                    Condition cond = emit_cmp_logic(target_op, t_r1, t_r2);
                    
                    bool header_jumps_on_true = (next_op == OpCode::JUMP_IF_TRUE_B);
                    Condition loop_cond = header_jumps_on_true ? (Condition)(cond ^ 1) : cond;

                    // Nhảy thẳng vào phần thân vòng lặp (bỏ qua phần check ở đầu vì đã check ở cuối)
                    size_t body_bc = t_ip + 4; 
                    if (bc_to_native_.count(body_bc)) {
                        size_t body_native = bc_to_native_[body_bc];
                        int64_t diff = (int64_t)body_native - (int64_t)(emit_.cursor() + 2);
                        if (diff >= -128 && diff <= 127) {
                            emit_.jcc_short(loop_cond, (int8_t)diff);
                            return; // Xong, return sớm
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
}

void Compiler::emit_cond_jump(OpCode op, uint8_t reg, uint16_t off) {
    bool on_true = (op == OpCode::JUMP_IF_TRUE_B);
    Reg r = map_vm_reg(reg);
    if (r == INVALID_REG) { load_vm_reg(RAX_REG, reg); r = RAX_REG; }
    emit_.test(r, r);
    fixups_.push_back({emit_.cursor(), (size_t)off, true});
    emit_.jcc(on_true ? NE : E, 0); 
}

} // namespace meow::jit::x64
EOF

echo "Da tao xong cac file trong jit/x64!"
echo "Hay nho cap nhat CMakeLists.txt de them: jit/x64/memory.cpp, jit/x64/math_ops.cpp, jit/x64/control_flow.cpp"

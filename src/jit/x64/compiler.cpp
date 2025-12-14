#include "jit/x64/compiler.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>

#include "vm/handlers/exception_ops.h" 
#include "vm/handlers/math_ops.h"
#include "vm/handlers/data_ops.h"
#include "vm/handlers/flow_ops.h"
#include "vm/handlers/memory_ops.h"
#include "vm/handlers/oop_ops.h"
#include "meow/core/meow_object.h"
#include "vm/vm_state.h" // Include đầy đủ định nghĩa VMState

namespace meow::jit::x64 {

// ============================================================================
// 1. CONSTANTS & REGISTER MAPPING Strategy
// ============================================================================

static constexpr size_t PAGE_SIZE = 4096;

static constexpr Reg REG_VM_REGS  = R15; // Pinned: VM Registers Array
static constexpr Reg REG_CONSTS   = R14; // Pinned: Constants Array
static constexpr Reg REG_STATE    = R13; // Pinned: VMState Pointer

static constexpr Reg RAX_REG = RAX;
static constexpr Reg RCX_REG = RCX;

// Helper Macros
#define MEM_REG(idx) REG_VM_REGS, (idx) * 8
#define MEM_CONST(idx) REG_CONSTS, (idx) * 8

// ============================================================================
// 2. HELPER FUNCTIONS
// ============================================================================

extern "C" {
    static void helper_add(Value* regs, VMState* state, uint16_t dst, uint16_t r1, uint16_t r2) {
        regs[dst] = OperatorDispatcher::find(OpCode::ADD, regs[r1], regs[r2])(&state->heap, regs[r1], regs[r2]);
    }
    static void helper_sub(Value* regs, VMState* state, uint16_t dst, uint16_t r1, uint16_t r2) {
        regs[dst] = OperatorDispatcher::find(OpCode::SUB, regs[r1], regs[r2])(&state->heap, regs[r1], regs[r2]);
    }
    static void helper_mul(Value* regs, VMState* state, uint16_t dst, uint16_t r1, uint16_t r2) {
        regs[dst] = OperatorDispatcher::find(OpCode::MUL, regs[r1], regs[r2])(&state->heap, regs[r1], regs[r2]);
    }
    static void helper_div(Value* regs, VMState* state, uint16_t dst, uint16_t r1, uint16_t r2) {
        regs[dst] = OperatorDispatcher::find(OpCode::DIV, regs[r1], regs[r2])(&state->heap, regs[r1], regs[r2]);
    }
    
    static void helper_new_array(Value* regs, Value* consts, VMState* state, uint16_t dst, uint16_t start, uint16_t count) {
        auto arr = state->heap.new_array();
        arr->reserve(count);
        for(size_t i=0; i<count; ++i) arr->push(regs[start+i]);
        regs[dst] = Value(arr);
    }

    static void helper_get_global(Value* regs, Value* consts, VMState* state, uint16_t dst, uint16_t name_idx) {
        state->error("JIT Error: Global variable access is not fully implemented in JIT yet.");
    }

    static void helper_set_global(Value* regs, Value* consts, VMState* state, uint16_t name_idx, uint16_t val_reg) {
        state->error("JIT Error: Global variable access is not fully implemented in JIT yet.");
    }
}

// ============================================================================
// 3. COMPILER IMPLEMENTATION
// ============================================================================

Compiler::Compiler() : emit_(nullptr, 0) {
    if (capacity_ % PAGE_SIZE != 0) {
        capacity_ = (capacity_ / PAGE_SIZE + 1) * PAGE_SIZE;
    }

#if defined(__linux__) || defined(__APPLE__)
    code_mem_ = (uint8_t*)mmap(nullptr, capacity_, 
                               PROT_READ | PROT_WRITE | PROT_EXEC, 
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code_mem_ == MAP_FAILED) {
        perror("JIT mmap failed");
        exit(1);
    }
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

Compiler::JitFunc Compiler::compile(const uint8_t* bytecode, size_t len) {
    emit_ = Emitter(code_mem_, capacity_);
    fixups_.clear();
    bc_to_native_.clear();

    emit_prologue();

    size_t ip = 0;
    while (ip < len) {
        bc_to_native_[ip] = emit_.cursor();
        
        OpCode op = static_cast<OpCode>(bytecode[ip]);
        auto read_u8  = [&]() { return bytecode[++ip]; };
        auto read_u16 = [&]() { 
            uint16_t v; std::memcpy(&v, bytecode + ip + 1, 2); ip += 2; return v; 
        };
        ip++; 

        switch (op) {
            case OpCode::LOAD_CONST: {
                uint16_t dst = read_u16();
                uint16_t idx = read_u16();
                emit_.mov(RAX_REG, MEM_CONST(idx));
                emit_.mov(MEM_REG(dst), RAX_REG);
                break;
            }
            case OpCode::LOAD_INT: {
                uint16_t dst = read_u16();
                int64_t val; std::memcpy(&val, bytecode + ip, 8); ip += 8;
                Value v(val);
                emit_.mov(RAX_REG, v.as_raw_u64());
                emit_.mov(MEM_REG(dst), RAX_REG);
                break;
            }
            case OpCode::LOAD_TRUE: {
                uint16_t dst = read_u16();
                Value v(true);
                emit_.mov(RAX_REG, v.as_raw_u64());
                emit_.mov(MEM_REG(dst), RAX_REG);
                break;
            }
            case OpCode::LOAD_FALSE: {
                uint16_t dst = read_u16();
                Value v(false);
                emit_.mov(RAX_REG, v.as_raw_u64());
                emit_.mov(MEM_REG(dst), RAX_REG);
                break;
            }
            case OpCode::LOAD_NULL: {
                uint16_t dst = read_u16();
                Value v(null_t{});
                emit_.mov(RAX_REG, v.as_raw_u64());
                emit_.mov(MEM_REG(dst), RAX_REG);
                break;
            }
            case OpCode::MOVE: {
                uint16_t dst = read_u16();
                uint16_t src = read_u16();
                emit_.mov(RAX_REG, MEM_REG(src));
                emit_.mov(MEM_REG(dst), RAX_REG);
                break;
            }

            #define JIT_MATH_OP(OP_NAME, HELPER_FUNC, NATIVE_ALU) \
            case OpCode::OP_NAME: case OpCode::OP_NAME##_B: { \
                bool is_b = (op == OpCode::OP_NAME##_B); \
                uint16_t dst = is_b ? read_u8() : read_u16(); \
                uint16_t r1  = is_b ? read_u8() : read_u16(); \
                uint16_t r2  = is_b ? read_u8() : read_u16(); \
                emit_.mov(RAX_REG, MEM_REG(r1)); \
                emit_.mov(RCX_REG, MEM_REG(r2)); \
                uint64_t tag_mask = 0xFFFF000000000000ULL; \
                uint64_t int_tag  = Value((int_t)0).as_raw_u64() & tag_mask; \
                emit_.mov(R11, tag_mask); \
                emit_.mov(R8, RAX_REG); emit_.and_(R8, R11); \
                emit_.mov(R9, RCX_REG); emit_.and_(R9, R11); \
                emit_.mov(R10, int_tag); \
                emit_.cmp(R8, R10); emit_.jcc(Condition::NE, 0); size_t l1 = emit_.cursor(); \
                emit_.cmp(R9, R10); emit_.jcc(Condition::NE, 0); size_t l2 = emit_.cursor(); \
                uint64_t payload_mask = 0x0000FFFFFFFFFFFFULL; \
                emit_.mov(R11, payload_mask); \
                emit_.and_(RAX_REG, R11); emit_.and_(RCX_REG, R11); \
                NATIVE_ALU \
                emit_.or_(RAX_REG, R10); \
                emit_.mov(MEM_REG(dst), RAX_REG); \
                emit_.jmp(0); size_t done = emit_.cursor(); \
                size_t slow = emit_.cursor(); \
                emit_.patch_u32(l1 - 4, slow - l1); \
                emit_.patch_u32(l2 - 4, slow - l2); \
                emit_.mov(RDI, REG_VM_REGS); \
                emit_.mov(RSI, REG_STATE); \
                emit_.mov(RDX, dst); \
                emit_.mov(RCX, r1); \
                emit_.mov(R8, r2); \
                emit_.mov(RAX_REG, (uint64_t)HELPER_FUNC); \
                emit_.call(RAX_REG); \
                size_t exit_pos = emit_.cursor(); \
                emit_.patch_u32(done - 4, exit_pos - done); \
                break; \
            }

            JIT_MATH_OP(ADD, helper_add, emit_.add(RAX_REG, RCX_REG);)
            JIT_MATH_OP(SUB, helper_sub, emit_.sub(RAX_REG, RCX_REG);)
            
            case OpCode::EQ: case OpCode::EQ_B:
            case OpCode::NEQ: case OpCode::NEQ_B:
            case OpCode::LT: case OpCode::LT_B:
            case OpCode::LE: case OpCode::LE_B:
            case OpCode::GT: case OpCode::GT_B:
            case OpCode::GE: case OpCode::GE_B: 
            {
                bool b = (op >= OpCode::ADD_B); 
                uint16_t dst = b ? read_u8() : read_u16();
                uint16_t r1  = b ? read_u8() : read_u16();
                uint16_t r2  = b ? read_u8() : read_u16();

                emit_.mov(RAX_REG, MEM_REG(r1));
                emit_.mov(RCX_REG, MEM_REG(r2));
                emit_.cmp(RAX_REG, RCX_REG);

                size_t next_ip = ip;
                bool fused = false;
                if (next_ip < len) {
                    OpCode next_op = static_cast<OpCode>(bytecode[next_ip]);
                    bool is_cond_jump = (next_op == OpCode::JUMP_IF_TRUE || next_op == OpCode::JUMP_IF_FALSE ||
                                         next_op == OpCode::JUMP_IF_TRUE_B || next_op == OpCode::JUMP_IF_FALSE_B);
                    
                    if (is_cond_jump) {
                        bool is_jb = (next_op == OpCode::JUMP_IF_TRUE_B || next_op == OpCode::JUMP_IF_FALSE_B);
                        size_t reg_idx_pos = next_ip + 1;
                        int cond_reg = is_jb ? bytecode[reg_idx_pos] : (*(uint16_t*)(bytecode + reg_idx_pos));
                        
                        if (cond_reg == dst) {
                            fused = true;
                            Condition cc = E;
                            switch(op) {
                                case OpCode::EQ: case OpCode::EQ_B: cc = E; break;
                                case OpCode::NEQ: case OpCode::NEQ_B: cc = NE; break;
                                case OpCode::LT: case OpCode::LT_B: cc = L; break;
                                case OpCode::LE: case OpCode::LE_B: cc = LE; break;
                                case OpCode::GT: case OpCode::GT_B: cc = G; break;
                                case OpCode::GE: case OpCode::GE_B: cc = GE; break;
                                default: break;
                            }
                            if (next_op == OpCode::JUMP_IF_FALSE || next_op == OpCode::JUMP_IF_FALSE_B) {
                                if (cc % 2 == 0) cc = (Condition)(cc + 1); else cc = (Condition)(cc - 1);
                            }
                            
                            size_t off_pos = reg_idx_pos + (is_jb ? 1 : 2);
                            uint16_t jump_target; std::memcpy(&jump_target, bytecode + off_pos, 2);
                            
                            fixups_.push_back({emit_.cursor(), (size_t)jump_target, true});
                            emit_.jcc(cc, 0);
                            ip = off_pos + 2; 
                        }
                    }
                }
                
                if (!fused) {
                    Condition cc = E;
                    switch(op) {
                        case OpCode::EQ: case OpCode::EQ_B: cc = E; break;
                        case OpCode::NEQ: case OpCode::NEQ_B: cc = NE; break;
                        case OpCode::LT: case OpCode::LT_B: cc = L; break;
                        case OpCode::LE: case OpCode::LE_B: cc = LE; break;
                        case OpCode::GT: case OpCode::GT_B: cc = G; break;
                        case OpCode::GE: case OpCode::GE_B: cc = GE; break;
                        default: break;
                    }
                    emit_.setcc(cc, RAX_REG);
                    emit_.movzx_b(RAX_REG, RAX_REG);
                    uint64_t bool_tag = Value(false).as_raw_u64() & 0xFFFF000000000000ULL;
                    emit_.mov(RCX_REG, bool_tag);
                    emit_.or_(RAX_REG, RCX_REG);
                    emit_.mov(MEM_REG(dst), RAX_REG);
                }
                break;
            }

            case OpCode::JUMP: {
                uint16_t off = read_u16();
                size_t target = off;
                if (target < ip) { 
                    size_t target_native = bc_to_native_[target];
                    size_t curr = emit_.cursor();
                    emit_.jmp((int32_t)(target_native) - (int32_t)(curr) - 5);
                } else { 
                    fixups_.push_back({emit_.cursor(), target, false});
                    emit_.jmp(0);
                }
                break;
            }

            case OpCode::GET_GLOBAL: {
                uint16_t dst = read_u16();
                uint16_t name = read_u16();
                emit_.mov(RDI, REG_VM_REGS);
                emit_.mov(RSI, REG_CONSTS);
                emit_.mov(RDX, REG_STATE);
                emit_.mov(RCX, dst);
                emit_.mov(R8, name);
                emit_.mov(RAX_REG, (uint64_t)helper_get_global);
                emit_.call(RAX_REG);
                break;
            }
            case OpCode::SET_GLOBAL: {
                uint16_t name = read_u16();
                uint16_t val = read_u16();
                emit_.mov(RDI, REG_VM_REGS);
                emit_.mov(RSI, REG_CONSTS);
                emit_.mov(RDX, REG_STATE);
                emit_.mov(RCX, name);
                emit_.mov(R8, val);
                emit_.mov(RAX_REG, (uint64_t)helper_set_global);
                emit_.call(RAX_REG);
                break;
            }
            case OpCode::NEW_ARRAY: {
                uint16_t dst = read_u16();
                uint16_t start = read_u16();
                uint16_t count = read_u16();
                emit_.mov(RDI, REG_VM_REGS);
                emit_.mov(RSI, REG_CONSTS);
                emit_.mov(RDX, REG_STATE);
                emit_.mov(RCX, dst);
                emit_.mov(R8, start);
                emit_.mov(R9, count);
                emit_.mov(RAX_REG, (uint64_t)helper_new_array);
                emit_.call(RAX_REG);
                break;
            }

            case OpCode::RETURN: {
                emit_epilogue();
                break;
            }

            default:
                break;
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
    emit_.push(RBP);
    emit_.mov(RBP, RSP);
    
    emit_.push(RBX);
    emit_.push(R12);
    emit_.push(R13);
    emit_.push(R14);
    emit_.push(R15);

    // Chú ý: Thứ tự này phải khớp với JitFunc(regs, consts, state)
    // RDI = regs
    // RSI = consts
    // RDX = state
    emit_.mov(REG_VM_REGS, RDI);  
    emit_.mov(REG_CONSTS, RSI);   
    emit_.mov(REG_STATE, RDX);    
}

void Compiler::emit_epilogue() {
    emit_.pop(R15);
    emit_.pop(R14);
    emit_.pop(R13);
    emit_.pop(R12);
    emit_.pop(RBX);

    emit_.mov(RSP, RBP);
    emit_.pop(RBP);
    emit_.ret();
}

Reg Compiler::map_vm_reg(int vm_reg) const { return INVALID_REG; }
void Compiler::load_vm_reg(Reg cpu_dst, int vm_src) {}
void Compiler::store_vm_reg(int vm_dst, Reg cpu_src) {}

} // namespace meow::jit::x64